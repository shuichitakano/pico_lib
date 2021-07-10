/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 12:55:08
 */

#include "timing.h"
#include <pico.h>

namespace dvi
{
    namespace
    {
        const Timing __not_in_flash_func(timing640x480p60_) = {
            .hSyncPolarity = false,
            .hFrontPorch = 16,
            .hSyncWidth = 96,
            .hBackPorch = 48,
            .hActivePixels = 640,

            .vSyncPolarity = false,
            .vFrontPorch = 10,
            .vSyncWidth = 2,
            .vBackPorch = 33,
            .vActiveLines = 480,

            .bitClockKHz = 252000,
        };
    }

    const Timing *getTiming640x480p60Hz()
    {
        return &timing640x480p60_;
    }

    uint32_t
    Timing::getPixelsPerLine() const
    {
        return hFrontPorch + hSyncWidth + hBackPorch + hActivePixels;
    }

    uint32_t
    Timing::getPixelsPerFrame() const
    {
        uint32_t w = hFrontPorch + hSyncWidth + hBackPorch + hActivePixels;
        uint32_t h = vFrontPorch + vSyncWidth + vBackPorch + vActiveLines;
        return w * h;
    }

}
