/*
 * author : Shuichi TAKANO
 * since  : Mon Jun 21 2021 01:55:06
 */

#include "tmds_encode.h"

#include <pico.h>
#include "hardware/interp.h"
#include <assert.h>

extern "C"
{
    void tmds_encode_loop_16bpp(const uint32_t *pixbuf, uint32_t *symbuf, size_t n_pix);
    void tmds_encode_loop_16bpp_leftshift(const uint32_t *pixbuf, uint32_t *symbuf, size_t n_pix, uint32_t leftshift);
}

namespace dvi
{
    namespace
    {
        constexpr uint32_t __scratch_x("tmds_table") tmdsTable_[] = {
#include "tmds_table.h"
        };

        int __not_in_flash_func(setupInterp)(interp_hw_t *interp, int bpp, int shift, int bits,
                                             const uint32_t *lut, int lutSizeInBits)
        {
            constexpr int indexShift = 2; // uint32_t
            int rshift = shift + bits - lutSizeInBits - indexShift;
            int lshift = 0;
            if (rshift < 0)
            {
                lshift = -rshift;
                rshift = 0;
            }

            int maskLSB = indexShift + lutSizeInBits - bits;
            int maskMSB = indexShift + lutSizeInBits - 1;

            int indexNextBit = indexShift + lutSizeInBits;
            {
                auto c = interp_default_config();
                interp_config_set_shift(&c, rshift);
                interp_config_set_mask(&c, maskLSB, maskMSB);
                interp_set_config(interp, 0, &c);
            }
            {
                auto c = interp_default_config();
                interp_config_set_shift(&c, rshift + bpp);
                interp_config_set_mask(&c, maskLSB, maskMSB);
                interp_config_set_cross_input(&c, true);
                interp_set_config(interp, 1, &c);
            }

            interp->base[0] = reinterpret_cast<uint32_t>(lut);
            interp->base[1] = reinterpret_cast<uint32_t>(lut);

            return lshift;
        }

        struct SaveInterp
        {
            interp_hw_save_t s;
            SaveInterp()
            {
                interp_save(interp0_hw, &s);
            }
            ~SaveInterp()
            {
                interp_restore(interp0_hw, &s);
            }
        };
    }

    void __not_in_flash_func(encodeTMDSChannel16bpp)(uint32_t *dstTMDS, const uint32_t *srcPixel, size_t n,
                                                     int shift, int bits)
    {
        SaveInterp saveInterp;

        int lshift = setupInterp(interp0_hw, 16, shift, bits, tmdsTable_, 6);
        if (lshift)
        {
            tmds_encode_loop_16bpp_leftshift(srcPixel, dstTMDS, n, lshift);
        }
        else
        {
            tmds_encode_loop_16bpp(srcPixel, dstTMDS, n);
        }
    }

    void __not_in_flash_func(encodeTMDS_RGB565)(uint32_t *dstTMDS, const uint16_t *srcPixel, size_t w)
    {
        assert((reinterpret_cast<uintptr_t>(srcPixel) & 3) == 0);
        auto *src = reinterpret_cast<const uint32_t *>(srcPixel);

        auto stride = w / 2;
        encodeTMDSChannel16bpp(dstTMDS + stride * 0, src, stride, 0, 5);
        encodeTMDSChannel16bpp(dstTMDS + stride * 1, src, stride, 5, 6);
        encodeTMDSChannel16bpp(dstTMDS + stride * 2, src, stride, 11, 5);
    }
}
