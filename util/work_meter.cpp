/*
 * author : Shuichi TAKANO
 * since  : Sat Sep 04 2021 18:13:23
 */

#include <util/work_meter.h>
#include <vector>
#include <hardware/structs/systick.h>
#include <pico.h>
#include <hardware/divider.h>

namespace util
{

    namespace
    {
        struct Manager
        {
            struct Unit
            {
                uint32_t counter;
                uint32_t tag;
            };

            std::vector<Unit> units_;
            uint32_t startCounter_{};

        public:
            Manager()
            {
                systick_hw->csr = 0x5;
                systick_hw->rvr = 0x00FFFFFF;
            }

            void __not_in_flash_func(reset)()
            {
                //                startCounter_ = systick_hw->cvr;
                units_.clear();

                // カウンタは減っていく
            }

            void __not_in_flash_func(enumUnits)(int scale, int div,
                                                const WorkMeterEnumFunc &func)
            {
                auto preCounter = systick_hw->cvr;

                uint32_t prevCt = 0;
                for (auto &u : units_)
                {
                    auto ct = (startCounter_ - u.counter) & 0xffffff;
#if 0
                    auto t = ct * scale / div;
                    auto s = (ct - prevCt) * scale / div;
#elif 0
                    auto t = hw_divider_s32_quotient_inlined(ct * scale, div);
                    auto s = hw_divider_s32_quotient_inlined((ct - prevCt) * scale, div);
#else
                    auto t = (ct * scale) >> 16;
                    auto s = ((ct - prevCt) * scale) >> 16;
#endif
                    func(t, s, u.tag);
                    prevCt = ct;
                }

                //
                startCounter_ = preCounter;
            }
        };

        Manager manager_;
    }

    void __not_in_flash_func(_workMeterReset)()
    {
        manager_.reset();
    }

    void __not_in_flash_func(_workMeterMark)(uint32_t tag)
    {
        Manager::Unit unit;
        unit.counter = systick_hw->cvr;
        unit.tag = tag;

        manager_.units_.push_back(unit);
    }

    void __not_in_flash_func(_workMeterEnum)(int scale, int div,
                                             const WorkMeterEnumFunc &func)
    {
        return manager_.enumUnits(scale, div, func);
    }

    uint32_t __not_in_flash_func(_workMeterGetCounter)()
    {
        return (manager_.startCounter_ - systick_hw->cvr) & 0xffffff;
    }
}
