/*
 * author : Shuichi TAKANO
 * since  : Fri Jul 16 2021 03:05:23
 */
#ifndef DF59A650_4134_6411_2E8F_9300D104CED7
#define DF59A650_4134_6411_2E8F_9300D104CED7

#include <stdio.h>

namespace util
{
    inline void
    dumpMemory(const void *p, size_t size)
    {
        auto *pp = static_cast<const uint8_t *>(p);
        while (size)
        {
            if ((size & 15) == 0)
            {
                printf("%p: ", pp);
            }
            printf("%02x ", *pp);
            ++pp;
            --size;
            if ((size & 15) == 0)
            {
                printf("\n");
            }
        }
    }
}

#endif /* DF59A650_4134_6411_2E8F_9300D104CED7 */
