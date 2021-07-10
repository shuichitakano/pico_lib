/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 14:27:10
 */
#ifndef _0D945FC8_B134_63A8_D9AC_25D2B6B92422
#define _0D945FC8_B134_63A8_D9AC_25D2B6B92422

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
