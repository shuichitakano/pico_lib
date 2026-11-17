/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 14:27:10
 */
#ifndef _0D945FC8_B134_63A8_D9AC_25D2B6B92422
#define _0D945FC8_B134_63A8_D9AC_25D2B6B92422

#include <cstdio>

// Set DVI_DEBUG_DUMP to 1 to print the DMA control lists and the HDMI data
// packets (AVI and audio InfoFrames, audio clock regeneration) as the output
// is set up. Off by default: the dump is about 6 KB, which at 115200 baud
// delays start-up by about half a second. The if (0) keeps the arguments
// type-checked when it is off.
#ifndef DVI_DEBUG_DUMP
#define DVI_DEBUG_DUMP 0
#endif
#if DVI_DEBUG_DUMP
#define DVI_DUMP(...) printf(__VA_ARGS__)
#else
#define DVI_DUMP(...) do { if (0) printf(__VA_ARGS__); } while (0)
#endif

namespace dvi
{
    inline constexpr int N_TMDS_LANES = 3;
    inline constexpr int TMDS_SYNC_LANE = 0;

    inline constexpr int N_CHAR_PER_WORD = 2;
    inline constexpr int N_LINE_PER_DATA = 2;

    inline constexpr int W_GUARDBAND = 2;
    inline constexpr int W_PREAMBLE = 8;
    inline constexpr int W_DATA_PACKET = 32;

    // 単純のため packet は 1つに限定する
    inline constexpr int W_DATA_ISLAND = W_GUARDBAND * 2 + W_DATA_PACKET;
    inline constexpr int N_DATA_ISLAND_WORDS = W_DATA_ISLAND / N_CHAR_PER_WORD;

    enum class LineState
    {
        FRONT_PORCH,
        SYNC,
        BACK_PORCH,
        ACTIVE,
        MAX,
    };
}

#endif /* _0D945FC8_B134_63A8_D9AC_25D2B6B92422 */
