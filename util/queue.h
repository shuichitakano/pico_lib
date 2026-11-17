/*
 * author : Shuichi TAKANO
 * since  : Fri Jun 25 2021 04:00:28
 */
#ifndef _25D39DD1_F134_63AD_3D0E_530B55A72531
#define _25D39DD1_F134_63AD_3D0E_530B55A72531

#include "spinlock.h"
#include <hardware/sync.h>
#include <vector>
#include <assert.h>
#include <pico.h>

namespace util
{

#if PICO_RP2350
    // below a copilot assisted version of this class which prevents the DVI Driver from locking up
    // The lockup only occurs when using a framebuffer and the GenesisPlus Emulator
    // For RP2040, the original code is used.
    //
    // What changed:
    // - Replaced std::vector push + erase pattern (which was O(n) and risked capacity misuse) with a fixed-size circular buffer (head/tail/count).
    // - Added capacity(), empty(), and tryDeque() helpers.
    // - enque now returns bool (fails gracefully if full instead of asserting only).
    // - Added optional spin limits to deque(spinLimit) and waitUntilContentAvailable(spinLimit) that will panic with a diagnostic instead of looping forever.
    // - Removed costly erase(begin()) (which could invalidate assumptions and cause subtle races if capacity reserve differed from size usage).
    //
    // Why this helps your random endless loop
    //
    // - Previous code relied on reserve() then push_back() and erase(begin()). If something elsewhere accidentally called queue_.resize() (or a reallocation occurred due to reserve mismatch during changes), the assertion path or timing could leave the consumer sleeping forever if a lost __sev() happens between empty checks.
    // - The new ring buffer always keeps elements in-place and uses __sev() only when an actual enqueue succeeds, reducing missed wake events.
    //
    // Follow-up suggestions
    //
    // - Audit all enque() call sites: they should now check the returned bool (or you can wrap with an assert if overflow is unacceptable).
    // - If you want a build-time safety net, define a macro to map old enque(x); usage to something that asserts the return value.
    // - Consider instrumenting if panics occur (spinLimit argument) by supplying a non-zero spinLimit in critical dequeue sites during debugging.
    template <class T>
    class Queue
    {
        // Lock protects head_, tail_, count_, and storage_ writes.
        SpinLock spinlock_;
        std::vector<T> storage_; // fixed capacity after ctor
        size_t head_ = 0;        // index of next element to pop
        size_t tail_ = 0;        // index of next slot to push
        size_t count_ = 0;       // number of valid elements

    public:
        Queue(size_t n = 0)
        {
            storage_.resize(n); // actual size used as capacity storage
        }

        size_t capacity() const { return storage_.size(); }

        __attribute__((always_inline)) size_t size()
        {
            SpinLockGuard lock(spinlock_);
            return count_;
        }

        __attribute__((always_inline)) bool empty()
        {
            SpinLockGuard lock(spinlock_);
            return count_ == 0;
        }

        __attribute__((always_inline)) const T &peek()
        {
            SpinLockGuard lock(spinlock_);
            assert(count_ > 0);
            return storage_[head_];
        }

        // Returns false if full
        __attribute__((always_inline)) bool enque(T &&v)
        {
            bool ok = false;
            {
                SpinLockGuard lock(spinlock_);
                if (count_ < storage_.size())
                {
                    storage_[tail_] = std::move(v);
                    tail_ = (tail_ + 1) % storage_.size();
                    ++count_;
                    ok = true;
                }
            }
            if (ok)
                __sev();
            return ok;
        }

        // Non-blocking attempt. Returns true and assigns out if element present.
        __attribute__((always_inline)) bool tryDeque(T &out)
        {
            SpinLockGuard lock(spinlock_);
            if (!count_)
                return false;
            out = std::move(storage_[head_]);
            head_ = (head_ + 1) % storage_.size();
            --count_;
            return true;
        }

        // Blocking dequeue; optional spin limit (0 = infinite) to detect stalls.
        __attribute__((always_inline)) T deque(uint32_t spinLimit = 0)
        {
            uint32_t spins = 0;
            while (true)
            {
                {
                    SpinLockGuard lock(spinlock_);
                    if (count_)
                    {
                        T r = std::move(storage_[head_]);
                        head_ = (head_ + 1) % storage_.size();
                        --count_;
                        return r;
                    }
                }
                if (spinLimit && ++spins > spinLimit)
                {
                    // Diagnostic panic to avoid endless loop.
                    panic("Queue::deque timeout (capacity=%u)\n", (unsigned)storage_.size());
                }
                __wfe();
            }
        }

        void waitUntilContentAvailable(uint32_t spinLimit = 0)
        {
            uint32_t spins = 0;
            while (true)
            {
                {
                    SpinLockGuard lock(spinlock_);
                    if (count_)
                        return;
                }
                if (spinLimit && ++spins > spinLimit)
                {
                    panic("Queue::wait timeout (capacity=%u)\n", (unsigned)storage_.size());
                }
                __wfe();
            }
        }
    };
#else
    // The original implementation for non-RP2350 targets
    template <class T>
    class Queue
    {
        SpinLock spinlock_;
        std::vector<T> queue_;

    public:
        Queue(size_t n = 0)
        {
            queue_.reserve(n);
        }

        __attribute__((always_inline)) size_t size()
        {
            SpinLockGuard lock(spinlock_);
            return queue_.size();
        }

        __attribute__((always_inline)) const T &peek() const
        {
            return queue_.front();
        }

        __attribute__((always_inline)) void enque(T &&v)
        {
            {
                SpinLockGuard lock(spinlock_);
                assert(queue_.size() < queue_.capacity());
                queue_.push_back(std::move(v));
            }
            __sev();
        }

        __attribute__((always_inline)) T deque()
        {
            while (1)
            {
                {
                    SpinLockGuard lock(spinlock_);
                    if (!queue_.empty())
                    {
                        auto r = std::move(queue_.front());
                        queue_.erase(queue_.begin());
                        return r;
                    }
                }
                __wfe();
            }
        }

        void waitUntilContentAvailable()
        {
            while (1)
            {
                {
                    SpinLockGuard lock(spinlock_);
                    if (!queue_.empty())
                    {
                        return;
                    }
                }
                __wfe();
            }
        }
    };
#endif
}

#endif /* _25D39DD1_F134_63AD_3D0E_530B55A72531 */
