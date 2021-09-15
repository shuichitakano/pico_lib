/*
 * author : Shuichi TAKANO
 * since  : Sat Sep 04 2021 18:12:32
 */
#ifndef D49253E1_C134_64DA_1148_8904E12518DB
#define D49253E1_C134_64DA_1148_8904E12518DB

#include <stdint.h>
//#include <functional>

namespace util
{
    // using WorkMeterEnumFunc = std::function<void(int timing,
    //                                              int span,
    //                                              uint32_t tag)>;
    using WorkMeterEnumFunc = void (*)(int timing, int span, uint32_t tag);

    void _workMeterReset();
    void _workMeterMark(uint32_t tag);
    void _workMeterEnum(int scale, int div,
                        const WorkMeterEnumFunc &func);
    uint32_t _workMeterGetCounter();

#ifndef NDEBUG
    inline void WorkMeterReset()
    {
        _workMeterReset();
    }
    inline void WorkMeterMark(uint32_t tag = ~0u)
    {
        _workMeterMark(tag);
    }
    inline void WorkMeterEnum(int scale, int div,
                              const WorkMeterEnumFunc &func)
    {
        _workMeterEnum(scale, div, func);
    }
    inline uint32_t WorkMeterGetCounter()
    {
        return _workMeterGetCounter();
    }
#else
    inline void WorkMeterReset()
    {
    }
    inline void WorkMeterMark(uint32_t = 0) {}
    inline void WorkMeterEnum(uint32_t, uint32_t,
                              const WorkMeterEnumFunc &func) {}
    inline uint32_t WorkMeterGetCounter() { return 0; }
#endif

}
#endif /* D49253E1_C134_64DA_1148_8904E12518DB */
