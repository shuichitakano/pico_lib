/*
 * author : Shuichi TAKANO
 * since  : Sat Jul 03 2021 19:56:19
 */
#ifndef _355AAF3D_8134_6412_12A7_A288F70C7D25
#define _355AAF3D_8134_6412_12A7_A288F70C7D25

#include <array>
#include <stdint.h>
#include "defines.h"

namespace dvi
{
    using DataIslandStream = std::array<std::array<uint32_t, N_DATA_ISLAND_WORDS>, 3>;

    enum class ScanInfo
    {
        NO_DATA,
        OVERSCAN,
        UNDERSCAN,
    };

    enum class PixelFormat
    {
        RGB,
        YCBCR422,
        YCBCR444,
    };

    enum class Colorimetry
    {
        NO_DATA,
        ITU601,
        ITU709,
        EXTENDED,
    };

    enum class PixtureAspectRatio
    {
        NO_DATA,
        _4_3,
        _16_9,
    };

    enum class ActiveFormatAspectRatio
    {
        NO_DATA = -1,
        SAME_AS_PAR = 8,
        _4_3,
        _16_9,
        _14_9,
    };

    enum class RGBQuantizationRange
    {
        DEFAULT,
        LIMITED,
        FULL,
    };

    enum VideoCode
    {
        _640x480P60 = 1,
        _720x480P60 = 2,
        _1280x720P60 = 4,
        _1920x1080I60 = 5,
    };

    struct DataPacket
    {
        std::array<uint8_t, 4> header;
        std::array<uint8_t, 8> subPacket[4];

    public:
        void computeHeaderParity();
        void computeSubPacketParity(int i);
        void computeParity();
        void computeInfoFrameCheckSum();

        void encodeHeader(uint32_t *dst, int hv, bool firstPacket) const;
        void encodeSubPacket(uint32_t *dst1, uint32_t *dst2) const;

        void setNull();
        void setAudioClockRegeneration(int CTS, int N);
        int setAudioSample(const std::array<int16_t, 2> *p, int n, int frameCt);
        void setAudioInfoFrame(int freq);
        void setAVIInfoFrame(ScanInfo s, PixelFormat y, Colorimetry c,
                             PixtureAspectRatio m, ActiveFormatAspectRatio r,
                             RGBQuantizationRange q, VideoCode vic);

        void dump() const;
        void test() const;
    };

    const uint32_t *getDefaultDataPacket12();
    const uint32_t *getDefaultDataPacket0(bool vsync, bool hsync);

    void encode(DataIslandStream &dst, const DataPacket &packet, bool vsync, bool hsync);
}

#endif /* _355AAF3D_8134_6412_12A7_A288F70C7D25 */
