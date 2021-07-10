/*
 * author : Shuichi TAKANO
 * since  : Fri Jun 25 2021 04:00:28
 */
#ifndef _25D39DD1_F134_63AD_3D0E_530B55A72531
#define _25D39DD1_F134_63AD_3D0E_530B55A72531

#include "spinlock.h"
#include <hardware/sync.h>
#include <vector>
#include <mutex>
#include <assert.h>

namespace util
{
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

        size_t size()
        {
            std::lock_guard lock(spinlock_);
            return queue_.size();
        }

        void enque(T &&v)
        {
            {
                std::lock_guard lock(spinlock_);
                assert(queue_.size() < queue_.capacity());
                queue_.push_back(std::move(v));
            }
            __sev();
        }

        T deque()
        {
            while (1)
            {
                {
                    std::lock_guard lock(spinlock_);
                    if (!queue_.empty())
                    {
                        auto r = queue_.front();
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
                    std::lock_guard lock(spinlock_);
                    if (!queue_.empty())
                    {
                        return;
                    }
                }
                __wfe();
            }
        }
    };
}

#endif /* _25D39DD1_F134_63AD_3D0E_530B55A72531 */
