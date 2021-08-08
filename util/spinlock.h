/*
 * author : Shuichi TAKANO
 * since  : Fri Jun 25 2021 02:02:44
 */
#ifndef _856CBF49_9134_63AD_1EDF_7C2C21E2134C
#define _856CBF49_9134_63AD_1EDF_7C2C21E2134C

#include <stdint.h>
#include "hardware/sync.h"

namespace util
{

    class SpinLock
    {
        uint32_t idx_;
        uint32_t irqState_{};

    public:
        SpinLock()
        {
            idx_ = spin_lock_claim_unused(true);
            //            next_striped_spin_lock_num();
        }

        ~SpinLock()
        {
            spin_lock_unclaim(idx_);
        }

        __attribute__((always_inline)) auto *get()
        {
            return spin_lock_instance(idx_);
        }

        __attribute__((always_inline)) void lock()
        {
            irqState_ = spin_lock_blocking(get());
            //spin_lock_unsafe_blocking(get());
        }

        __attribute__((always_inline)) void unlock()
        {
            spin_unlock(get(), irqState_);
            //spin_unlock_unsafe(get());
        }
    };
}

#endif /* _856CBF49_9134_63AD_1EDF_7C2C21E2134C */
