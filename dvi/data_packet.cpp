/*
 * author : Shuichi TAKANO
 * since  : Sat Jul 03 2021 20:04:34
 */

#include "data_packet.h"

#include <pico.h>
#include <string.h>

namespace dvi
{
    namespace
    {
        // clang-format off
        constexpr uint8_t __not_in_flash_func(bchTable_)[256] = {
            0x00, 0xd9, 0xb5, 0x6c, 0x6d, 0xb4, 0xd8, 0x01, 
            0xda, 0x03, 0x6f, 0xb6, 0xb7, 0x6e, 0x02, 0xdb, 
            0xb3, 0x6a, 0x06, 0xdf, 0xde, 0x07, 0x6b, 0xb2, 
            0x69, 0xb0, 0xdc, 0x05, 0x04, 0xdd, 0xb1, 0x68, 
            0x61, 0xb8, 0xd4, 0x0d, 0x0c, 0xd5, 0xb9, 0x60, 
            0xbb, 0x62, 0x0e, 0xd7, 0xd6, 0x0f, 0x63, 0xba, 
            0xd2, 0x0b, 0x67, 0xbe, 0xbf, 0x66, 0x0a, 0xd3, 
            0x08, 0xd1, 0xbd, 0x64, 0x65, 0xbc, 0xd0, 0x09, 
            0xc2, 0x1b, 0x77, 0xae, 0xaf, 0x76, 0x1a, 0xc3, 
            0x18, 0xc1, 0xad, 0x74, 0x75, 0xac, 0xc0, 0x19, 
            0x71, 0xa8, 0xc4, 0x1d, 0x1c, 0xc5, 0xa9, 0x70, 
            0xab, 0x72, 0x1e, 0xc7, 0xc6, 0x1f, 0x73, 0xaa, 
            0xa3, 0x7a, 0x16, 0xcf, 0xce, 0x17, 0x7b, 0xa2, 
            0x79, 0xa0, 0xcc, 0x15, 0x14, 0xcd, 0xa1, 0x78, 
            0x10, 0xc9, 0xa5, 0x7c, 0x7d, 0xa4, 0xc8, 0x11, 
            0xca, 0x13, 0x7f, 0xa6, 0xa7, 0x7e, 0x12, 0xcb, 
            0x83, 0x5a, 0x36, 0xef, 0xee, 0x37, 0x5b, 0x82, 
            0x59, 0x80, 0xec, 0x35, 0x34, 0xed, 0x81, 0x58, 
            0x30, 0xe9, 0x85, 0x5c, 0x5d, 0x84, 0xe8, 0x31, 
            0xea, 0x33, 0x5f, 0x86, 0x87, 0x5e, 0x32, 0xeb, 
            0xe2, 0x3b, 0x57, 0x8e, 0x8f, 0x56, 0x3a, 0xe3, 
            0x38, 0xe1, 0x8d, 0x54, 0x55, 0x8c, 0xe0, 0x39, 
            0x51, 0x88, 0xe4, 0x3d, 0x3c, 0xe5, 0x89, 0x50, 
            0x8b, 0x52, 0x3e, 0xe7, 0xe6, 0x3f, 0x53, 0x8a, 
            0x41, 0x98, 0xf4, 0x2d, 0x2c, 0xf5, 0x99, 0x40, 
            0x9b, 0x42, 0x2e, 0xf7, 0xf6, 0x2f, 0x43, 0x9a, 
            0xf2, 0x2b, 0x47, 0x9e, 0x9f, 0x46, 0x2a, 0xf3, 
            0x28, 0xf1, 0x9d, 0x44, 0x45, 0x9c, 0xf0, 0x29, 
            0x20, 0xf9, 0x95, 0x4c, 0x4d, 0x94, 0xf8, 0x21, 
            0xfa, 0x23, 0x4f, 0x96, 0x97, 0x4e, 0x22, 0xfb, 
            0x93, 0x4a, 0x26, 0xff, 0xfe, 0x27, 0x4b, 0x92, 
            0x49, 0x90, 0xfc, 0x25, 0x24, 0xfd, 0x91, 0x48, 
        };
        // clang-format on

        int encodeBCH3(const uint8_t *p)
        {
            auto v = bchTable_[p[0]];
            v = bchTable_[p[1] ^ v];
            v = bchTable_[p[2] ^ v];
            return v;
        }

        int encodeBCH7(const uint8_t *p)
        {
            auto v = bchTable_[p[0]];
            v = bchTable_[p[1] ^ v];
            v = bchTable_[p[2] ^ v];
            v = bchTable_[p[3] ^ v];
            v = bchTable_[p[4] ^ v];
            v = bchTable_[p[5] ^ v];
            v = bchTable_[p[6] ^ v];
            return v;
        }

        struct ParityTable
        {
            uint8_t v_[256]{};
            constexpr ParityTable()
            {
                for (int i = 0; i < 256; ++i)
                {
                    v_[i] = (i ^ (i >> 1) ^ (i >> 2) ^ (i >> 3) ^ (i >> 4) ^ (i >> 5) ^ (i >> 6) ^ (i >> 7)) & 1;
                }
            }

            int compute8(int v) const { return v_[v]; }
            int compute8(int v0, int v1) const { return v_[v0] ^ v_[v1]; }
            int compute8(int v0, int v1, int v2) const { return v_[v0] ^ v_[v1] ^ v_[v2]; }
        };

        constexpr ParityTable __not_in_flash_func(parityTable_);
    }

    void
    DataPacket::computeHeaderParity()
    {
        header[3] = encodeBCH3(header.data());
    }

    void
    DataPacket::computeSubPacketParity(int i)
    {
        subPacket[i][7] = encodeBCH7(subPacket[i].data());
    }

    void
    DataPacket::computeParity()
    {
        computeHeaderParity();
        computeSubPacketParity(0);
        computeSubPacketParity(1);
        computeSubPacketParity(2);
        computeSubPacketParity(3);
    }

    namespace
    {
        // TERC4 (1 char)
        constexpr uint16_t __not_in_flash_func(TERC4Syms_)[16] = {
            0b1010011100,
            0b1001100011,
            0b1011100100,
            0b1011100010,
            0b0101110001,
            0b0100011110,
            0b0110001110,
            0b0100111100,
            0b1011001100,
            0b0100111001,
            0b0110011100,
            0b1011000110,
            0b1010001110,
            0b1001110001,
            0b0101100011,
            0b1011000011,
        };

        constexpr uint32_t makeTERC4x2Char(int i0, int i1) { return TERC4Syms_[i0] | (TERC4Syms_[i1] << 10); }
        constexpr uint32_t makeTERC4x2Char(int i) { return TERC4Syms_[i] | (TERC4Syms_[i] << 10); }
        constexpr uint32_t TERC4_0x2CharSym_ = makeTERC4x2Char(0);

        // Data Gaurdband (lane 1, 2)
        constexpr uint32_t __not_in_flash_func(dataGaurdbandSym_) = 0b0100110011'0100110011;

        // default data packet
        constexpr uint32_t __not_in_flash_func(defaultDataPacket12_)[N_DATA_ISLAND_WORDS] = {
            dataGaurdbandSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            TERC4_0x2CharSym_,
            dataGaurdbandSym_,
        };

        template <bool v, bool h>
        constexpr std::array<uint32_t, N_DATA_ISLAND_WORDS> makeDefaultDataPacket0()
        {
            constexpr int base = (v ? 2 : 0) | (h ? 1 : 0);
            return {
                makeTERC4x2Char(0b1100 | base),
                makeTERC4x2Char(0b0000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1000 | base),
                makeTERC4x2Char(0b1100 | base),
            };
        }

        constexpr std::array<uint32_t, N_DATA_ISLAND_WORDS> __not_in_flash_func(defaultDataPackets0_)[4] = {
            makeDefaultDataPacket0<false, false>(),
            makeDefaultDataPacket0<false, true>(),
            makeDefaultDataPacket0<true, false>(),
            makeDefaultDataPacket0<true, true>(),
        };
    }

    const uint32_t *getDefaultDataPacket12()
    {
        return defaultDataPacket12_;
    }

    const uint32_t *getDefaultDataPacket0(bool vsync, bool hsync)
    {
        return defaultDataPackets0_[(vsync << 1) | hsync].data();
    }

    void
    DataPacket::dump() const
    {
        printf("HB: %02x %02x %02x:%02x\n", header[0], header[1], header[2], header[3]);
        for (int i = 0; i < 4; ++i)
        {
            printf("SP%d: %02x %02x %02x %02x %02x %02x %02x:%02x\n", i,
                   subPacket[i][0], subPacket[i][1], subPacket[i][2], subPacket[i][3],
                   subPacket[i][4], subPacket[i][5], subPacket[i][6], subPacket[i][7]);
        }
    }

    void
    DataPacket::encodeHeader(uint32_t *dst, int hv, bool firstPacket) const
    {
        auto hv1 = hv | 8;
        if (!firstPacket)
        {
            hv = hv1;
        }
        for (int i = 0; i < 4; ++i)
        {
            auto h = header[i];
            dst[0] = makeTERC4x2Char(((h << 2) & 4) | hv,
                                     ((h << 1) & 4) | hv1);
            dst[1] = makeTERC4x2Char((h & 4) | hv1,
                                     ((h >> 1) & 4) | hv1);
            dst[2] = makeTERC4x2Char(((h >> 2) & 4) | hv1,
                                     ((h >> 3) & 4) | hv1);
            dst[3] = makeTERC4x2Char(((h >> 4) & 4) | hv1,
                                     ((h >> 5) & 4) | hv1);
            dst += 4;
            hv = hv1;
        }
    }

    void
    DataPacket::encodeSubPacket(uint32_t *dst1, uint32_t *dst2) const
    {
        for (int i = 0; i < 8; ++i)
        {
            uint32_t v = (subPacket[0][i] << 0) |
                         (subPacket[1][i] << 8) |
                         (subPacket[2][i] << 16) |
                         (subPacket[3][i] << 24);

            auto t = (v ^ (v >> 7)) & 0x00aa00aa;
            v = v ^ t ^ (t << 7);
            t = (v ^ (v >> 14)) & 0x0000cccc;
            v = v ^ t ^ (t << 14);
            // 01234567 89abcdef ghijklmn opqrstuv
            // 08go4cks 19hp5dlt 2aiq6emu 3bjr7fnv
            dst1[0] = makeTERC4x2Char((v >> 0) & 15, (v >> 16) & 15);
            dst1[1] = makeTERC4x2Char((v >> 4) & 15, (v >> 20) & 15);
            dst2[0] = makeTERC4x2Char((v >> 8) & 15, (v >> 24) & 15);
            dst2[1] = makeTERC4x2Char((v >> 12) & 15, (v >> 28) & 15);
            dst1 += 2;
            dst2 += 2;
        }
    }

    void
    DataPacket::test() const
    {
        uint8_t t1[32];
        uint8_t t2[32];
        auto *p1 = t1;
        auto *p2 = t2;
        for (int i = 0; i < 8; ++i)
        {
            uint32_t v = (subPacket[0][i] << 0) |
                         (subPacket[1][i] << 8) |
                         (subPacket[2][i] << 16) |
                         (subPacket[3][i] << 24);

            auto t = (v ^ (v >> 7)) & 0x00aa00aa;
            v = v ^ t ^ (t << 7);
            t = (v ^ (v >> 14)) & 0x0000cccc;
            v = v ^ t ^ (t << 14);
            // 01234567 89abcdef ghijklmn opqrstuv
            // 08go4cks 19hp5dlt 2aiq6emu 3bjr7fnv
            p1[0] = (v >> 0) & 15;
            p1[1] = (v >> 16) & 15;
            p1[2] = (v >> 4) & 15;
            p1[3] = (v >> 20) & 15;
            p2[0] = (v >> 8) & 15;
            p2[1] = (v >> 24) & 15;
            p2[2] = (v >> 12) & 15;
            p2[3] = (v >> 28) & 15;
            p1 += 4;
            p2 += 4;
        }

        printf("L1:");
        for (int i = 0; i < 32; ++i)
        {
            printf("%x", t1[i]);
        }
        printf("\n");
        printf("L2:");
        for (int i = 0; i < 32; ++i)
        {
            printf("%x", t2[i]);
        }
        printf("\n");
        for (int i = 0; i < 2; ++i)
        {
            auto t = i == 0 ? t1 : t2;
            printf("Lane %d\n", i);
            for (int j = 0; j < 4; ++j)
            {
                printf("%d: ", j);
                for (int k = 0; k < 32; ++k)
                {
                    printf("%d", t[k] & (1 << j) ? 1 : 0);
                }
                printf("\n");
            }
        }
    }

    void encode(DataIslandStream &dst, const DataPacket &packet, bool vsync, bool hsync)
    {
        int hv = (vsync ? 2 : 0) | (hsync ? 1 : 0);
        dst[0][0] = makeTERC4x2Char(0b1100 | hv);
        dst[1][0] = dataGaurdbandSym_;
        dst[2][0] = dataGaurdbandSym_;

        packet.encodeHeader(&dst[0][1], hv, true);
        packet.encodeSubPacket(&dst[1][1], &dst[2][1]);

        dst[0][N_DATA_ISLAND_WORDS - 1] = makeTERC4x2Char(0b1100 | hv);
        dst[1][N_DATA_ISLAND_WORDS - 1] = dataGaurdbandSym_;
        dst[2][N_DATA_ISLAND_WORDS - 1] = dataGaurdbandSym_;
    }

    void
    DataPacket::setNull()
    {
        memset(this, 0, sizeof(*this));
    }

    void
    DataPacket::setAudioClockRegeneration(int CTS, int N)
    {
        header[0] = 1;
        header[1] = 0;
        header[2] = 0;
        computeHeaderParity();

        subPacket[0][0] = 0;
        subPacket[0][1] = CTS >> 16;
        subPacket[0][2] = CTS >> 8;
        subPacket[0][3] = CTS;
        subPacket[0][4] = N >> 16;
        subPacket[0][5] = N >> 8;
        subPacket[0][6] = N;
        computeSubPacketParity(0);

        subPacket[1] = subPacket[0];
        subPacket[2] = subPacket[0];
        subPacket[3] = subPacket[0];
    }

    int
    DataPacket::setAudioSample(const std::array<int16_t, 2> *p, int n, int frameCt)
    {
        constexpr int layout = 0;
        const int samplePresent = (1 << n) - 1;
        const int B = frameCt < 4 ? 1 << frameCt : 0;
        header[0] = 2;
        header[1] = (layout << 4) | samplePresent;
        header[2] = B << 4;
        computeHeaderParity();

        for (int i = 0; i < n; ++i)
        {
            const auto l = (*p)[0];
            const auto r = (*p)[1];
            const auto vuc = 1; // valid
            auto &d = subPacket[i];
            d[0] = 0;
            d[1] = l;
            d[2] = l >> 8;
            d[3] = 0;
            d[4] = r;
            d[5] = r >> 8;
            auto pl = parityTable_.compute8(d[1], d[2], vuc);
            auto pr = parityTable_.compute8(d[4], d[5], vuc);
            d[6] = (vuc << 0) | (pl << 3) | (vuc << 4) | (pr << 7);
            computeSubPacketParity(i);
            ++p;
            // channel status 真面目に扱う必要があるだろうか?
        }
        memset(&subPacket[n], 0, 8 * (4 - n));
        //        dump();

        frameCt -= n;
        if (frameCt < 0)
        {
            frameCt += 192;
        }
        return frameCt;
    }

    void
    DataPacket::computeInfoFrameCheckSum()
    {
        int s = 0;
        for (int i = 0; i < 3; ++i)
        {
            s += header[i];
        }
        int n = header[2] + 1;
        for (int j = 0; j < 4; ++j)
        {
            for (int i = 0; i < 7 && n; ++i, --n)
            {
                s += subPacket[j][i];
            }
        }
        subPacket[0][0] = -s;
    }

    void
    DataPacket::setAudioInfoFrame(int freq)
    {
        setNull();
        header[0] = 0x84;
        header[1] = 1;  // version
        header[2] = 10; // len

        const int cc = 1; // 2ch
        const int ct = 1; // IEC 60958 PCM
        const int ss = 1; // 16bit
        const int sf = freq == 48000 ? 3 : (freq == 44100 ? 2 : 0);
        const int ca = 0;  // FR, FL
        const int lsv = 0; // 0db
        const int dm_inh = 0;
        subPacket[0][1] = cc | (ct << 4);
        subPacket[0][2] = ss | (sf << 2);
        subPacket[0][4] = ca;
        subPacket[0][5] = (lsv << 3) | (dm_inh << 7);

        computeInfoFrameCheckSum();
        computeParity();
    }

    void
    DataPacket::setAVIInfoFrame(ScanInfo s, PixelFormat y, Colorimetry c,
                                PixtureAspectRatio m, ActiveFormatAspectRatio r,
                                RGBQuantizationRange q, VideoCode vic)
    {
        setNull();
        header[0] = 0x82;
        header[1] = 2;  // version
        header[2] = 13; // len

        // int sc = 0;
        int sc = 3; // scale hv

        subPacket[0][1] = static_cast<int>(s) |
                          (r == ActiveFormatAspectRatio::NO_DATA ? 0 : 16) |
                          (static_cast<int>(y) << 5);

        subPacket[0][2] = static_cast<int>(r) | (static_cast<int>(m) << 4) | (static_cast<int>(c) << 6);
        subPacket[0][3] = sc | (static_cast<int>(q) << 2);
        subPacket[0][4] = static_cast<int>(vic);

        computeInfoFrameCheckSum();
        computeParity();
    }
}
