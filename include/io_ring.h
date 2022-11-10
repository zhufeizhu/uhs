#pragma once

#include <stdint.h>
#include <assert.h>
#include <string.h>
#define EINVAL -1
#if 1
#define smp_wmb() __sync_synchronize()
#else
#define smp_wmb() wmb()

#ifdef CONFIG_UNORDERED_IO
#define wmb() asm volatile("sfence" ::: "memory")
#else
#define wmb() asm volatile("" ::: "memory")
#endif
#endif

namespace uhs
{
class IoRing{
public:
    IoRing(uint32_t size, uint32_t esize);

    inline uint32_t ringIn(const void* src){
        uint32_t len = 1;
        uint32_t l = unused();
        
        if (len > l)
            len = l;
        
        l = ringInBurstReally(src,len);
        this->in_ += l;
        return l;
    };

    inline uint32_t ringOut(void* dst){
        uint32_t len = 1;
        uint32_t l = used();

        if(l == 0)
            return l;
        
        if(len > l)
            return len;

        l = ringOutBurstReally(dst,len);
        this->out_ += l;
        return l;
    };

    ~IoRing(){
        delete this->data_;
        this->data_ = NULL;
    };

private:
    uint32_t in_;
    uint32_t out_;
    uint32_t mask_;
    uint32_t esize_;
    void* data_;

    uint32_t ringInBurstReally(const void* src, uint32_t len);
    uint32_t ringOutBurstReally(void* dst, uint32_t len);

    inline unsigned int roundup_pow_of_two(unsigned int x) {
        return 1UL << fls(x - 1);
    }

    inline int fls(int x) {
        int r;
        __asm__(
            "bsrl %1,%0\n\t"
            "jnz 1f\n\t"
            "movl $-1,%0\n"
            "1:"
            : "=r"(r)
            : "rm"(x));
        return r + 1;
    }

    inline uint32_t min(unsigned int a, unsigned int b) {
        return (a > b) ? b : a;
    }

    inline uint32_t unused() {
        return (this->mask_ + 1) - (this->in_ - this->out_);
    }

    inline uint32_t used(){
        return this->in_ - this->out_;
    }
};

} // namespace uhs