/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 06:16:00
 */
#ifndef CC31A9F7_8134_63A8_5DED_6FB68E08C7AE
#define CC31A9F7_8134_63A8_5DED_6FB68E08C7AE

#include <stdint.h>

namespace dvi
{
    struct Timing
    {
        bool hSyncPolarity;
        int hFrontPorch;
        int hSyncWidth;
        int hBackPorch;
        int hActivePixels;

        bool vSyncPolarity;
        int vFrontPorch;
        int vSyncWidth;
        int vBackPorch;
        int vActiveLines;

        int bitClockKHz;

        uint32_t getPixelsPerLine() const;
        uint32_t getPixelsPerFrame() const;
        uint32_t getPixelClock() const { return bitClockKHz * 100; }
    };

    const Timing *getTiming640x480p60Hz();
}

#endif /* CC31A9F7_8134_63A8_5DED_6FB68E08C7AE */
