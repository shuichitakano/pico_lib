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
            }
            else
            {
                if (!curTMDSBuffer_)
                {
                    if (validTMDSQueue_.size())
                    {
                        int line = validTMDSQueue_.peek().line;
                        if (line * 2 == lineCounter_)
                        {
                            curTMDSBuffer_ = validTMDSQueue_.deque().buffer;
                        }
                    }
                }
                tmdsBuf = curTMDSBuffer_ ? curTMDSBuffer_->data() : nullptr;

                if (lineCounter_ % N_LINE_PER_DATA == N_LINE_PER_DATA - 1)
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

            if (pendingAudioLineCount_)
            {
                --pendingAudioLineCount_;
                return false;
            }
            if (audioSampleRing_.getReadableSize() == 0)
            {
                pendingAudioLineCount_ = 1024; // 枯渇しているようなら 2 frame くらい待ってみる
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

#if 0
            if (leftAudioSampleCount_)
            {
                const auto n = std::min(leftAudioSampleCount_, std::min<int>(audioSampleRing_.getReadableSize(), 2));
                if (n)
                {
                    const auto *p = audioSampleRing_.getReadPointer();
                    audioFrameCount_ = packet.setAudioSample(p, n, audioFrameCount_);
                    audioSampleRing_.advanceReadPointer(n);
                    leftAudioSampleCount_ -= n;
                    return true;
                }
            }
#else
            //            audioSamplePos_ += samplesPerLine16_;
            int n = std::max(0, std::min(4, std::min<int>(audioSamplePos_ >> 16, audioSampleRing_.getReadableSize())));
            audioSamplePos_ -= n << 16;
            if (n)
            {
                const auto *p = audioSampleRing_.getReadPointer();
                audioFrameCount_ = packet.setAudioSample(p, n, audioFrameCount_);
                audioSampleRing_.advanceReadPointer(n);
                return true;
            }
#endif

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
        // Adjust PIO for gpio pins > 32
        // The Waveshare RP2350-PiZero is a board that is using gpio pins > 32.
        if (config_->pinTMDS[0] > 32 || config_->pinTMDS[1] > 32 || config_->pinTMDS[2] > 32)
        {
            pio_set_gpio_base(pio_, 16);
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

        validTMDSQueue_.enque({srcLine.line, dstTMDS});
        freeLineQueue_.enque(std::move(srcLine.buffer));
    }

    void
    DVI::convertScanBuffer12bpp()
    {
        auto dstTMDS = freeTMDSQueue_.deque();
        auto srcLine = validLineQueue_.deque();

        encodeTMDS_RGB444(dstTMDS->data(),
                          srcLine.buffer->data(), srcLine.buffer->size());

        validTMDSQueue_.enque({srcLine.line, dstTMDS});
        freeLineQueue_.enque(std::move(srcLine.buffer));
    }

    void
    DVI::convertScanBuffer12bpp(uint16_t line, uint16_t *buffer, size_t size)
    {
        auto dstTMDS = freeTMDSQueue_.deque();
        encodeTMDS_RGB444(dstTMDS->data(), buffer, size);
        validTMDSQueue_.enque({line, dstTMDS});
    }

    void
    DVI::convertScanBuffer12bppScaled16_7(int srcPixelOfs, int dstPixelOfs, int dstPixels)
    {
        auto dstTMDS = freeTMDSQueue_.deque();
        auto srcLine = validLineQueue_.deque();

        srcPixelOfs &= ~1u;
        dstPixelOfs &= ~1u;

        auto *p = dstTMDS->data() + (dstPixelOfs >> 1);
        encodeTMDS_RGB444_Scaled16_7(p,
                                     srcLine.buffer->data() + srcPixelOfs,
                                     dstPixels,
                                     srcLine.buffer->size());

        validTMDSQueue_.enque({srcLine.line, dstTMDS});
        freeLineQueue_.enque(std::move(srcLine.buffer));
    }

    void
    DVI::convertScanBuffer12bppScaled16_7(int srcPixelOfs, int dstPixelOfs, int dstPixels, uint16_t line, uint16_t *buffer, size_t size)
    {
        auto dstTMDS = freeTMDSQueue_.deque();

        srcPixelOfs &= ~1u;
        dstPixelOfs &= ~1u;

        auto *p = dstTMDS->data() + (dstPixelOfs >> 1);
        encodeTMDS_RGB444_Scaled16_7(p,
                                     buffer + srcPixelOfs,
                                     dstPixels,
                                     size);

        validTMDSQueue_.enque({line, dstTMDS});
    }


    void
    DVI::setAudioFreq(int freq, int CTS, int N)
    {
        audioFreq_ = freq;
        audioClockRegeneration_.setAudioClockRegeneration(CTS, N);
        audioInfoFrame_.setAudioInfoFrame(freq);

        audioClockRegeneration_.dump();
        audioInfoFrame_.dump();

        auto pixelClock = timing_->getPixelClock();
        auto nPixPerFrame = timing_->getPixelsPerFrame();
        auto nPixPerLine = timing_->getPixelsPerLine();

        samplesPerFrame_ = static_cast<int>(static_cast<uint64_t>(freq) * nPixPerFrame / pixelClock);
        samplesPerLine16_ = static_cast<int>(static_cast<uint64_t>(freq) * nPixPerLine * 65536 / pixelClock);
        printf("setAudioFreq: %d Hz, CTS %d, N %d, %d samples/frame %d/65536 samples/line\n", freq, CTS, N, samplesPerFrame_,
               samplesPerLine16_);

        enableDataIsland();
    }

    void
    DVI::allocateAudioBuffer(size_t size)
    {
        audioSampleBuffer_.resize(size, {0, 0});
        audioSampleRing_.setBuffer(audioSampleBuffer_.data(), size);
    }
}
