/*
 * author : Shuichi TAKANO
 * since  : Mon Jun 21 2021 01:56:13
 */
#ifndef _2E1B48B1_8134_63A9_17D5_7F58FD21FD21
#define _2E1B48B1_8134_63A9_17D5_7F58FD21FD21

#include <stdint.h>
#include <stdlib.h>

namespace dvi
{
    void encodeTMDSChannel16bpp(uint32_t *dstTMDS, const uint32_t *srcPixel, size_t n,
                                int shift, int bits);

    void encodeTMDS_RGB565(uint32_t *dstTMDS, const uint16_t *srcPixel, size_t w);
}

#endif /* _2E1B48B1_8134_63A9_17D5_7F58FD21FD21 */
