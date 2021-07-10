/*
 * author : Shuichi TAKANO
 * since  : Sun Jun 20 2021 05:19:13
 */
#ifndef FA371169_5134_63A8_4F38_6BE873D291AD
#define FA371169_5134_63A8_4F38_6BE873D291AD

#include <stdint.h>

namespace dvi
{
    struct Config
    {
        int pinTMDS[3]{};
        int pinClock{};
        bool invert = false;
    };

}

#endif /* FA371169_5134_63A8_4F38_6BE873D291AD */
