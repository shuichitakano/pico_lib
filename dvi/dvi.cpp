/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 03:32:27
 */

#include "dvi.h"
#include "defines.h"
#include "tmds_encode.h"
#include <pico.h>
#include <hardware/pio.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/irq.h>
#include <hardware/timer.h>
#include <hardware/structs/padsbank0.h>
#include <stdio.h>
#include <assert.h>

#include <dvi_serialiser.pio.h>

namespace dvi
{
    namespace
    {
        DVI *dmaIRQInst_{};
    }

    DVI::DVI(PIO pio, const Config *cfg, const Timing *timing)
        : pio_(pio), config_(cfg), timing_(timing),
          dma_(*timing, pio)
    {
        assert(pio_);
        assert(config_);
        assert(timing_);
       
        initSerialiser();
        allocateBuffers(timing);

        aviInfoFrame_.setAVIInfoFrame(ScanInfo::UNDERSCAN,
                                      PixelFormat::RGB,
                                      Colorimetry::ITU601,
                                      PixtureAspectRatio::_4_3,
                                      ActiveFormatAspectRatio::SAME_AS_PAR,
                                      RGBQuantizationRange::FULL,
                                      VideoCode::_640x480P60);

        aviInfoFrame_.dump();
    }

    void
    DVI::registerIRQThisCore()
    {
        dmaIRQInst_ = this;

        irq_set_exclusive_handler(DMA_IRQ_0, dmaIRQEntry);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    void
    DVI::unregisterIRQThisCore()
    {
        irq_set_enabled(DMA_IRQ_0, false);
        irq_remove_handler(DMA_IRQ_0, dmaIRQEntry);

        for (auto &p : releaseTMDSBuffer_)
        {
            if (p)
            {
                freeTMDSQueue_.enque(std::move(p));
                p = nullptr;
            }
        }
    }

    void
    DVI::start()
    {
        lineState_ = {};
        lineCounter_ = 0;
        for (int i = 0; i < N_TMDS_LANES; ++i)
        {
            pio_sm_clear_fifos(pio_, i);
        }

        dma_.start();
        started_ = true;

        // FIFOを完全に埋めてからシリアライズを始める
        for (int i = 0; i < N_TMDS_LANES; ++i)
        {
            while (!pio_sm_is_tx_fifo_full(pio_, i))
            {
                tight_loop_contents();
            }
        }

        enableSerialiser(true);
    }

    void
    DVI::stop()
    {
        dma_.stop();
        enableSerialiser(false);
        started_ = false;
    }

    void
    DVI::enableDataIsland()
    {
        enableDataIsland_ = true;
        dma_.setupInternalDataPacketStream();
    }

    void
    DVI::dmaIRQEntry()
    {
        dmaIRQInst_->dmaIRQHandler();
    }

    void
    DVI::dmaIRQHandler()
    {
        dma_.clearInterruptReq();
        if (!started_)
        {
            return;
        }

        auto prevState = lineState_;
        advanceLine();

        dma_.waitForLastBlockTransferToStart(*timing_);

        if (auto p = releaseTMDSBuffer_[1])
        {
            freeTMDSQueue_.enque(std::move(p));
        }
        releaseTMDSBuffer_[1] = releaseTMDSBuffer_[0];
        releaseTMDSBuffer_[0] = nullptr;

        uint32_t *tmdsBuf = nullptr;
        bool blankLine = false;
        if (lineState_ == LineState::ACTIVE)
        {
            if (lineCounter_ < blankSettings_.top ||
                lineCounter_ >= (timing_->vActiveLines - blankSettings_.bottom))
            {
                blankLine = true;

                // Retire whatever came due on this line even though the line is
                // blanked, rather than leaving it queued forever.
                //
                // The blank margins are changed at runtime: every menu sets them
                // to zero while it is open, draws all SCREENHEIGHT lines, and
                // restores them on exit. Buffers already queued for lines that
                // the restore puts back inside a margin were never consumed --
                // the branch below is the only place that touches the queue, and
                // it does not run for a blank line. freeTMDSQueue_ then drained,
                // core1 blocked in convertScanBuffer12bpp()'s
                // freeTMDSQueue_.deque(), validLineQueue_ filled behind it and
                // the producer blocked in getLineBuffer() forever, with the
                // display stuck on listActiveError_ (red). Deeper line pools
                // widen the window, because more stale menu lines survive the
                // transition, but the stranding itself is margin-driven.
                if (curTMDSBuffer_)
                {
                    // Straddled the boundary mid-source-line. Release through
                    // the deferred slot: the DMA may still be reading it.
                    releaseTMDSBuffer_[0] = curTMDSBuffer_;
                    curTMDSBuffer_ = nullptr;
                }
                else if (validTMDSQueue_.size() &&
                         validTMDSQueue_.peek().first == lineCounter_)
                {
                    // Never handed to the DMA, so it can go straight back.
                    freeTMDSQueue_.enque(validTMDSQueue_.deque().buffer);
                }
            }
            else
            {
                if (!curTMDSBuffer_)
                {
                    if (validTMDSQueue_.size())
                    {
                        const auto next = validTMDSQueue_.peek();
                        if (next.first == lineCounter_)
                        {
                            curLast_ = next.first + next.count - 1;
                            curTMDSBuffer_ = validTMDSQueue_.deque().buffer;
                        }
                    }
                }
                tmdsBuf = curTMDSBuffer_ ? curTMDSBuffer_->data() : nullptr;
                if (!tmdsBuf)
                {
                    ++missedLines_;
                }

                if (curTMDSBuffer_ && lineCounter_ == curLast_)
                {
                    releaseTMDSBuffer_[0] = curTMDSBuffer_;
                    curTMDSBuffer_ = nullptr;
                }

                if (enableScanLine_ && (lineCounter_ & 1))
                {
                    blankLine = true;
                }
            }
        }

        dma_.update(lineState_, tmdsBuf, *timing_, blankSettings_, blankLine);
        if (enableDataIsland_)
        {
            updateDataPacket();
        }

        if (prevState != lineState_ && lineState_ == LineState::SYNC)
        {
            ++frameCounter_;
        }
    }

    void
    DVI::updateDataPacket()
    {
        DataPacket packet;
        auto proc = [&]
        {
            if (samplesPerFrame_ == 0)
            {
                return false;
            }

            audioSamplePos_ += samplesPerLine16_;

            if (lineState_ == LineState::FRONT_PORCH)
            {
                if (lineCounter_ == 0)
                {
                    if (frameCounter_ & 1)
                    {
                        packet = aviInfoFrame_;
                    }
                    else
                    {
                        packet = audioInfoFrame_;
                    }
                    leftAudioSampleCount_ = samplesPerFrame_;
                    return true;
                }
                else if (lineCounter_ == 1)
                {
                    packet = audioClockRegeneration_;
                    return true;
                }
            }

            int due = std::max(0, std::min(4, static_cast<int>(audioSamplePos_ >> 16)));
            int avail = audioSampleRing_.getReadableSize();
            int n = std::min(due, avail);
            if (n > 0)
            {
                const auto *p = audioSampleRing_.getReadPointer();
                audioFrameCount_ = packet.setAudioSample(p, n, audioFrameCount_);
                audioSampleRing_.advanceReadPointer(n);
                audioSamplePos_ -= n << 16;
                return true;
            }
            if (due > 0)
            {
                // SRAM-resident silence: a static const in flash would
                // hardfault core1 if reached during an XIP-off window,
                // and would also add a cold-cache flash fetch to the
                // first frames after boot.
                static AudioSample __not_in_flash("dvi_silence") silence[4] = {};
                audioFrameCount_ = packet.setAudioSample(silence, due, audioFrameCount_);
                audioSamplePos_ -= due << 16;
                return true;
            }

            return false;
        };

        if (!proc())
        {
            packet.setNull();
        }
        dma_.updateNextDataPacket(lineState_, packet, *timing_);
    }

    void
    DVI::allocateBuffers(const Timing *timing)
    {
        assert(timing);
        auto w = timing->hActivePixels;

        {
            auto chSize = w / N_CHAR_PER_WORD;
            auto size = chSize * N_TMDS_LANES;
            for (auto &v : tmdsBuffers_)
            {
                v.resize(size, 0x7fd00u /* 0, 0 */);
                freeTMDSQueue_.enque(&v);
            }
        }
        {
            // alignment が4なのを期待する…
            for (auto &v : lineBuffers_)
            {
                v.resize(w);
                freeLineQueue_.enque(&v);
            }
        }
    }

    void
    DVI::initSerialiser()
    {
        // A PIO drives a window of 32 GPIOs from its GPIO base: 0-31, or
        // 16-47 on RP2350. Both pins of every TMDS data pair (pin and pin + 1)
        // must lie in it; GPIO 32-39 on the Waveshare RP2350-PiZero need 16.
        // The clock pair is driven by PWM, not by the PIO, so it is left out.
        int lo = config_->pinTMDS[0];
        int hi = config_->pinTMDS[0] + 1;
        for (int i = 1; i < N_TMDS_LANES; ++i)
        {
            lo = std::min(lo, config_->pinTMDS[i]);
            hi = std::max(hi, config_->pinTMDS[i] + 1);
        }
        auto fits = [lo, hi](int base)
        { return lo >= base && hi <= base + 31; };

        int gpioBase = -1;
        if (fits(0))
        {
            gpioBase = 0;
        }
#if PICO_PIO_VERSION > 0
        else if (fits(16))
        {
            gpioBase = 16;
        }
#endif
        if (gpioBase < 0)
        {
            panic("DVI: TMDS data pins %d, %d, %d do not fit in one PIO GPIO window\n",
                  config_->pinTMDS[0], config_->pinTMDS[1], config_->pinTMDS[2]);
        }
        // Before pio_add_program(): the SDK does not change the base of a PIO
        // that has a program loaded.
        if (pio_get_gpio_base(pio_) != static_cast<uint>(gpioBase))
        {
            const int rc = pio_set_gpio_base(pio_, gpioBase);
            if (rc != PICO_OK)
            {
                panic("DVI: cannot set the PIO GPIO base to %d (error %d)\n", gpioBase, rc);
            }
        }

        auto configurePad = [](int gpio, bool invert)
        {
            hw_write_masked(
                &padsbank0_hw->io[gpio],
                (0 << PADS_BANK0_GPIO0_DRIVE_LSB),
                PADS_BANK0_GPIO0_DRIVE_BITS | PADS_BANK0_GPIO0_SLEWFAST_BITS | PADS_BANK0_GPIO0_IE_BITS);
            gpio_set_outover(gpio, invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        };

        auto prgOfs = pio_add_program(pio_, &dvi_serialiser_program);
        for (int i = 0; i < N_TMDS_LANES; ++i)
        {
            auto sm = i;
            pio_sm_claim(pio_, sm);

            auto pin = config_->pinTMDS[i];
            serialiserProgramInit(pio_, sm, prgOfs, pin, N_CHAR_PER_WORD);

            configurePad(pin, config_->invert);
            configurePad(pin + 1, config_->invert);
        }

        {
            int pin = config_->pinClock;
            assert((pin & 1) == 0);
            auto slice = pwm_gpio_to_slice_num(pin);

            auto cfg = pwm_get_default_config();
            pwm_config_set_output_polarity(&cfg, true, false);
            pwm_config_set_wrap(&cfg, 9);
            pwm_init(slice, &cfg, false);
            pwm_set_both_levels(slice, 5, 5);

            gpio_set_function(pin + 0, GPIO_FUNC_PWM);
            gpio_set_function(pin + 1, GPIO_FUNC_PWM);
            configurePad(pin + 0, config_->invert);
            configurePad(pin + 1, config_->invert);
        }
    }

    void
    DVI::enableSerialiser(bool enable)
    {
        pwm_set_enabled(pwm_gpio_to_slice_num(config_->pinClock), enable);

        uint mask = 0;
        for (int i = 0; i < N_TMDS_LANES; ++i)
        {
            int sm = i;
            mask |= 1u << (sm + PIO_CTRL_SM_ENABLE_LSB);
        }
        if (enable)
        {
            hw_set_bits(&pio_->ctrl, mask);
        }
        else
        {
            hw_clear_bits(&pio_->ctrl, mask);
        }
    }

    void DVI::advanceLine()
    {
        auto getNextLine = [&]
        {
            switch (lineState_)
            {
            case LineState::FRONT_PORCH:
                return timing_->vFrontPorch;

            case LineState::SYNC:
                return timing_->vSyncWidth;

            case LineState::BACK_PORCH:
                return timing_->vBackPorch;

            case LineState::ACTIVE:
                return timing_->vActiveLines;

            default:
                return 0;
            };
        };

        if (++lineCounter_ == getNextLine())
        {
            lineState_ = static_cast<LineState>((static_cast<int>(lineState_) + 1) % static_cast<int>(LineState::MAX));
            lineCounter_ = 0;
        }
    }

    DVI::LineBuffer *
    DVI::getLineBuffer()
    {
        return freeLineQueue_.deque();
    }

    void
    DVI::setLineBuffer(int line, LineBuffer *p)
    {
        validLineQueue_.enque({line, p});
    }

    void
    DVI::waitForValidLine()
    {
        validLineQueue_.waitUntilContentAvailable();
    }
    /*
    void
    DVI::loopScanBuffer16bpp()
    {
        while (true)
        {
            auto dstTMDS = freeTMDSQueue_.deque();
            auto srcLine = validLineQueue_.deque();

            encodeTMDS_RGB565(dstTMDS->data(), srcLine->data(), srcLine->size());

            validTMDSQueue_.enque(std::move(dstTMDS));
            freeLineQueue_.enque(std::move(srcLine));
        }
    }
*/
    void
    DVI::loopScanBuffer15bpp()
    {
        while (true)
        {
            convertScanBuffer15bpp();
        }
    }

    void
    DVI::convertScanBuffer15bpp()
    {
        auto dstTMDS = freeTMDSQueue_.deque();
        auto srcLine = validLineQueue_.deque();

        encodeTMDS_RGB555(dstTMDS->data(),
                          srcLine.buffer->data(), srcLine.buffer->size());

        validTMDSQueue_.enque({srcLine.line * N_LINE_PER_DATA, N_LINE_PER_DATA, dstTMDS});
        freeLineQueue_.enque(std::move(srcLine.buffer));
    }

    void
    DVI::encodeLine12bpp(TMDSBuffer *dst, int line, const uint16_t *src, size_t size)
    {
        encodeTMDS_RGB444(dst->data(), src, size);
        validTMDSQueue_.enque({line * N_LINE_PER_DATA, N_LINE_PER_DATA, dst});
    }

    void
    DVI::convertScanBuffer12bpp()
    {
        auto dstTMDS = freeTMDSQueue_.deque();
        auto srcLine = validLineQueue_.deque();

        encodeLine12bpp(dstTMDS, srcLine.line, srcLine.buffer->data(), srcLine.buffer->size());
        // The line buffer belongs to the driver: give it back to the producer.
        freeLineQueue_.enque(std::move(srcLine.buffer));
    }

    void
    DVI::convertScanBuffer12bpp(uint16_t line, uint16_t *buffer, size_t size)
    {
        // The caller owns buffer, so it is not returned to freeLineQueue_.
        encodeLine12bpp(freeTMDSQueue_.deque(), line, buffer, size);
    }

    void
    DVI::convertScanBuffer12bppRows(int first, int count, const uint16_t *src, int width)
    {
        const uint32_t t0 = time_us_32();
        auto dstTMDS = freeTMDSQueue_.deque();
        rowsWaitUs_ += time_us_32() - t0;

        // A word of TMDS symbols per source pixel. The buffers also carry
        // 320-pixel lines, so the borders are written every time.
        const int chStride = timing_->hActivePixels / N_CHAR_PER_WORD;
        const int border = (chStride - width) / 2;
        assert((width & 3) == 0 && border >= 0);
        auto *p = dstTMDS->data();
        for (int ch = 0; ch < static_cast<int>(N_COLOR_CH); ++ch, p += chStride)
        {
            for (int i = 0; i < border; ++i)
            {
                p[i] = 0x7fd00u; // 0, 0
                p[chStride - 1 - i] = 0x7fd00u;
            }
            encodeTMDSChannel16bpp(p + border, reinterpret_cast<const uint32_t *>(src), width, ch * 4, 4);
        }

        validTMDSQueue_.enque({first, count, dstTMDS});
    }

    void
    DVI::encodeLine12bppScaled16_7(TMDSBuffer *dst, int line, const uint16_t *src, size_t size,
                                   int srcPixelOfs, int dstPixelOfs, int dstPixels)
    {
        srcPixelOfs &= ~1u;
        dstPixelOfs &= ~1u;

        encodeTMDS_RGB444_Scaled16_7(dst->data() + (dstPixelOfs >> 1),
                                     src + srcPixelOfs,
                                     dstPixels,
                                     size);

        validTMDSQueue_.enque({line * N_LINE_PER_DATA, N_LINE_PER_DATA, dst});
    }

    void
    DVI::convertScanBuffer12bppScaled16_7(int srcPixelOfs, int dstPixelOfs, int dstPixels)
    {
        auto dstTMDS = freeTMDSQueue_.deque();
        auto srcLine = validLineQueue_.deque();

        encodeLine12bppScaled16_7(dstTMDS, srcLine.line, srcLine.buffer->data(), srcLine.buffer->size(),
                                  srcPixelOfs, dstPixelOfs, dstPixels);
        // The line buffer belongs to the driver: give it back to the producer.
        freeLineQueue_.enque(std::move(srcLine.buffer));
    }

    void
    DVI::convertScanBuffer12bppScaled16_7(int srcPixelOfs, int dstPixelOfs, int dstPixels, uint16_t line, uint16_t *buffer, size_t size)
    {
        // The caller owns buffer, so it is not returned to freeLineQueue_.
        encodeLine12bppScaled16_7(freeTMDSQueue_.deque(), line, buffer, size,
                                  srcPixelOfs, dstPixelOfs, dstPixels);
    }


    void
    DVI::setAudioFreq(int freq, int CTS, int N)
    {
        // Map/advise on non-standard sample rates: only 32000, 44100, 48000 are explicitly encoded
        if (!(freq == 32000 || freq == 44100 || freq == 48000))
        {
            printf("[DVI] Warning: non-standard audio freq %d Hz requested. InfoFrame will advertise nearest standard.\n", freq);
        }
        audioFreq_ = freq;

        // If CTS==0, auto-compute a suitable CTS for given N,freq and current pixel clock
        // Fs = Fpix * N / (128 * CTS)  => CTS = Fpix * N / (128 * Fs)
        if (CTS == 0 && freq > 0 && N > 0)
        {
            uint64_t Fpix = timing_->getPixelClock();
            printf("Pixel clock %llu Hz, computing CTS for %d Hz, N=%d\n", Fpix, freq, N);
            uint64_t numerator = Fpix * static_cast<uint64_t>(N);
            uint64_t denom = static_cast<uint64_t>(128) * static_cast<uint64_t>(freq);
            CTS = static_cast<int>((numerator + denom / 2) / denom); // rounded
            // For common 48k/6144 pattern, CTS should equal pixelClock/1000 if pixelClock in Hz multiple of 1k.
        }

        audioClockRegeneration_.setAudioClockRegeneration(CTS, N);
        audioInfoFrame_.setAudioInfoFrame(freq);

        audioClockRegeneration_.dump();
        audioInfoFrame_.dump();

        auto pixelClock = timing_->getPixelClock();
        auto nPixPerFrame = timing_->getPixelsPerFrame();
        auto nPixPerLine = timing_->getPixelsPerLine();

        samplesPerFrame_ = static_cast<int>(static_cast<uint64_t>(freq) * nPixPerFrame / pixelClock);
        samplesPerLine16_ = static_cast<int>(static_cast<uint64_t>(freq) * nPixPerLine * 65536 / pixelClock);
     // Reconstructed Fs from CTS/N for diagnostics
     double reconstructedFs = (double)pixelClock * (double)N / (128.0 * (double)CTS);
     double ppmError = (reconstructedFs - freq) * 1e6 / freq;
     printf("setAudioFreq: req %d Hz, CTS %d, N %d, recon %.3f Hz (%.1f ppm), %d samples/frame %d/65536 samples/line\n",
         freq, CTS, N, reconstructedFs, ppmError, samplesPerFrame_, samplesPerLine16_);

     // Reset internal audio packet scheduling state so a runtime switch is clean.
     audioSamplePos_ = 0;
     leftAudioSampleCount_ = 0;
     pendingAudioLineCount_ = 0;
     audioFrameCount_ = 0;

        enableDataIsland();
    }

    void
    DVI::allocateAudioBuffer(size_t size)
    {
        audioSampleBuffer_.resize(size, {0, 0});
        audioSampleRing_.setBuffer(audioSampleBuffer_.data(), size);
    }
}
