/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 03:34:30
 */
#ifndef C0B8F27B_5134_63A8_332D_5F7EB7D8492C
#define C0B8F27B_5134_63A8_332D_5F7EB7D8492C

#include "config.h"
#include "timing.h"
#include "dma.h"
#include <hardware/pio.h>
#include <stdint.h>
#include <array>
#include <vector>
#include <util/queue.h>
#include <util/ring_buffer.h>

namespace dvi
{
    class DVI
    {
    public:
        using PixelType = uint16_t;
        using LineBuffer = std::vector<PixelType>;
        using AudioSample = std::array<int16_t, 2>;

    public:
        DVI(PIO pio, const Config *cfg, const Timing *timing);
        void registerIRQThisCore();
        void unregisterIRQThisCore();

        void start();
        void stop();

        //        void __not_in_flash_func(loopScanBuffer16bpp)();
        void __not_in_flash_func(loopScanBuffer15bpp)();

        void __not_in_flash_func(convertScanBuffer15bpp)();
        void __not_in_flash_func(convertScanBuffer12bpp)();
        void __not_in_flash_func(convertScanBuffer12bpp)(uint16_t line, uint16_t *buffer, size_t size);
        void __not_in_flash_func(convertScanBuffer12bppScaled16_7)(int srcPixelOfs, int dstPixelOfs, int dstPixels);
        void __not_in_flash_func(convertScanBuffer12bppScaled16_7)(int srcPixelOfs, int dstPixelOfs, int dstPixels, uint16_t line, uint16_t *buffer, size_t size);
        // One row of `width` pixels (a multiple of 4), pixel doubled and centred
        // between black borders, shown on the `count` output lines from `first`.
        void __not_in_flash_func(convertScanBuffer12bppRows)(int first, int count, const uint16_t *src, int width);
        uint32_t getFrameCounter() const
        {
            return frameCounter_;
        }
        // Running totals, for statistics: active lines shown without a TMDS
        // buffer (the red error lines), and the time convertScanBuffer12bppRows()
        // waited for a free one.
        uint32_t getMissedLines() const { return missedLines_; }
        uint32_t getRowsWaitUs() const { return rowsWaitUs_; }

        BlankSettings &getBlankSettings() { return blankSettings_; }
        void setScanLine(bool f) { enableScanLine_ = f; }

        LineBuffer *__not_in_flash_func(getLineBuffer)();
        void __not_in_flash_func(setLineBuffer)(int line, LineBuffer *);
        void __not_in_flash_func(waitForValidLine)();

        void setAudioFreq(int freq, int CTS, int N);
        void allocateAudioBuffer(size_t size);
        util::RingBuffer<AudioSample> &getAudioRingBuffer() { return audioSampleRing_; }

    protected:
        void initSerialiser();
        void enableSerialiser(bool enable = true);
        void allocateBuffers(const Timing *timing);
        void enableDataIsland();

        void __not_in_flash_func(advanceLine)();
        void __not_in_flash_func(updateDataPacket)();
        void __not_in_flash_func(dmaIRQHandler)();

        static void __not_in_flash_func(dmaIRQEntry)();

    private:
        PIO pio_;
        const Config *config_{};
        const Timing *timing_{};
        BlankSettings blankSettings_;
        bool enableDataIsland_ = false;
        bool enableScanLine_ = false;

        bool started_ = false;
        LineState lineState_{};
        int lineCounter_ = 0;

        DMA dma_;

        uint32_t frameCounter_ = 0;

        template <class T>
        struct ResultBuffer
        {
            int line{};
            T buffer{};
        };

        using TMDSBuffer = std::vector<uint32_t>;
        using ResultLineBuffer = ResultBuffer<LineBuffer *>;

        // A TMDS buffer is shown on `count` output lines from `first`: a source
        // line on two (N_LINE_PER_DATA), a row of convertScanBuffer12bppRows()
        // on as many as it covers.
        struct ResultTMDSBuffer
        {
            int first{};
            int count{};
            TMDSBuffer *buffer{};
        };

        // Encode one source line into dst and queue it for output lines
        // 2 * line and 2 * line + 1. src is only read: the caller keeps it.
        // One implementation per conversion, shared by the line-queue and
        // the caller-buffer variants of convertScanBuffer12bpp*().
        //
        // Always inlined: a call frame of its own puts these paths 40 bytes
        // deeper on core1's stack, which the DMA IRQ shares. With a 512-byte
        // core1 stack that overflowed in the 8:7 mode on RP2040. Inlined, the
        // code also lands in the SRAM-resident callers at any -O level.
        __attribute__((always_inline)) inline void encodeLine12bpp(TMDSBuffer *dst, int line, const uint16_t *src, size_t size);
        __attribute__((always_inline)) inline void encodeLine12bppScaled16_7(TMDSBuffer *dst, int line, const uint16_t *src, size_t size,
                                                                             int srcPixelOfs, int dstPixelOfs, int dstPixels);

        // Depth of the line-buffer pool, i.e. how many scanlines the producer
        // may run ahead of scanout.
        //
        // This bounds the producer's tolerance for being slower per line than
        // the display consumes. A source line is scanned out every
        // 2 * (1/31500) = 63.5us at 640x480p60. A producer needing more than
        // that per line falls behind by the difference on every line and
        // starves once its lead is spent -- and dmaIRQHandler() only accepts a
        // buffer on an exact `first == lineCounter_` match, so one miss
        // orphans the rest of the frame (red lines) and costs a whole display
        // frame. Five buffers is only ~318us of lead: fine for a producer that
        // is faster per line than scanout, useless for one that is marginally
        // slower, which is why the failure is abrupt rather than gradual.
        //
        // Only the line pool needs to grow. Core1 drains it into TMDS buffers
        // as fast as it can and then blocks on the (small) free TMDS pool, so
        // the producer's lead simply accumulates as queued line buffers.
        // Each costs hActivePixels * sizeof(PixelType) = 1280 bytes at 640 wide.
        //
        // Default stays at 5 so other users of this library are unaffected;
        // override with -DDVI_N_LINE_BUFFERS=n.
#ifndef DVI_N_LINE_BUFFERS
#define DVI_N_LINE_BUFFERS 5
#endif
        static inline constexpr size_t N_BUFFERS = 5;
        static inline constexpr size_t N_LINE_BUFFERS = DVI_N_LINE_BUFFERS;
        static_assert(N_LINE_BUFFERS >= 2, "need at least a producer/consumer pair");
        static inline constexpr size_t N_COLOR_CH = 3;

        TMDSBuffer tmdsBuffers_[N_BUFFERS];
        LineBuffer lineBuffers_[N_LINE_BUFFERS];

        util::Queue<ResultTMDSBuffer> validTMDSQueue_{N_BUFFERS};
        util::Queue<TMDSBuffer *> freeTMDSQueue_{N_BUFFERS};
        TMDSBuffer *curTMDSBuffer_{};
        int curLast_ = 0; // the last output line of curTMDSBuffer_
        TMDSBuffer *releaseTMDSBuffer_[2]{};
        uint32_t missedLines_ = 0;
        uint32_t rowsWaitUs_ = 0;

        util::Queue<ResultLineBuffer> validLineQueue_{N_LINE_BUFFERS};
        util::Queue<LineBuffer *> freeLineQueue_{N_LINE_BUFFERS};

        DataPacket aviInfoFrame_;
        DataPacket audioClockRegeneration_;
        DataPacket audioInfoFrame_;
        int audioFreq_ = 0;
        int samplesPerFrame_ = 0;
        int samplesPerLine16_ = 0;

        std::vector<AudioSample> audioSampleBuffer_;
        util::RingBuffer<AudioSample> audioSampleRing_;

        int leftAudioSampleCount_ = 0;
        int audioSamplePos_ = 0;
        int audioFrameCount_ = 0;

        int pendingAudioLineCount_ = 0;
    };
}

#endif /* C0B8F27B_5134_63A8_332D_5F7EB7D8492C */
