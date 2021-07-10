/*
 * author : Shuichi TAKANO
 * since  : Mon Jan 14 2019 3:13:35
 */
#ifndef BC9A58A2_7134_1395_301B_CAE003D5ECB0
#define BC9A58A2_7134_1395_301B_CAE003D5ECB0

#include <assert.h>
#include <stdint.h>
#include <hardware/sync.h>

namespace util
{

    template <class T>
    class RingBuffer
    {
        typedef T value_type;

        T *buffer_;
        uint32_t size_;

        volatile uint32_t read_;
        volatile uint32_t write_;

    public:
        RingBuffer(T *p = 0, uint32_t size = 0) { setBuffer(p, size); }

        void setBuffer(T *p, uint32_t size)
        {
            assert((size & (size - 1)) == 0);
            buffer_ = p;
            size_ = size;
            read_ = 0;
            write_ = 0;
        }

        T *getBufferTop() const { return buffer_; }
        uint32_t getBufferSize() const { return size_; }

        uint32_t getWritableSize() const
        {
            __mem_fence_acquire();
            uint32_t rp = read_;
            uint32_t wp = write_;
            if (wp < rp)
                return rp - wp - 1;
            else
                return size_ - wp - (rp == 0 ? 1 : 0);
        }

        uint32_t getFullWritableSize() const
        {
            __mem_fence_acquire();
            uint32_t rp = read_;
            uint32_t wp = write_;
            if (wp < rp)
                return rp - wp - 1;
            else
                return size_ - wp + rp - 1;
        }

        uint32_t getReadableSize() const
        {
            __mem_fence_acquire();
            uint32_t wp = write_;
            uint32_t rp = read_;
            if (wp < rp)
                return size_ - rp;
            else
                return wp - rp;
        }

        uint32_t getFullReadableSize() const
        {
            __mem_fence_acquire();
            uint32_t wp = write_;
            uint32_t rp = read_;
            if (wp < rp)
                return size_ - rp + wp;
            else
                return wp - rp;
        }

        T *getWritePointer() { return buffer_ + write_; }

        const T *getReadPointer() const { return buffer_ + read_; }

        uint32_t getReadOffset() const { return read_; }

        uint32_t getWriteOffset() const { return write_; }

        void advanceWritePointer(uint32_t size)
        {
            write_ = (write_ + size) & (size_ - 1);
            __mem_fence_release();
        }

        void advanceReadPointer(uint32_t size)
        {
            read_ = (read_ + size) & (size_ - 1);
            __mem_fence_release();
        }

        void _setWriteOffset(uint32_t v)
        {
            write_ = v;
            __mem_fence_release();
        }

        void _setReadOffset(uint32_t v)
        {
            read_ = v;
            __mem_fence_release();
        }
    };

} // namespace util

#endif /* BC9A58A2_7134_1395_301B_CAE003D5ECB0 */
