/*
 * author : Shuichi TAKANO
 * since  : Sat Aug 28 2021 16:05:40
 */
#ifndef BCDDAE47_A134_6476_F4FB_7AB320F8E3EC
#define BCDDAE47_A134_6476_F4FB_7AB320F8E3EC

#include <hardware/sync.h>
#include <atomic>

namespace util
{
    class ExclusiveProc
    {
        using Func = void (*)();
        std::atomic<Func> func_;

    public:
        bool isExist() const
        {
            return func_.load();
        }

        void processOrWaitIfExist()
        {
            // 待ち状態にない最後のコアに実行させたいが、
            // 2コアしかないから待ってないコアにきたときに実行すればよい
            if (func_.load())
            {
                auto irq = save_and_disable_interrupts();
                func_.load()();
                func_ = nullptr;
                restore_interrupts(irq);
                __sev();
            }
        }

        void __not_in_flash_func(setProcAndWait)(void (*f)())
        {
            func_ = f;
            auto irq = save_and_disable_interrupts();
            while (func_.load())
            {
                __wfe();
            }
            restore_interrupts(irq);
        }
    };

}

#endif /* BCDDAE47_A134_6476_F4FB_7AB320F8E3EC */
