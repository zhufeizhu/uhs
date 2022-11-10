#pragma once

#include "include/io_ring.h"

namespace uhs{


IoRing::IoRing(uint32_t size, uint32_t esize){
    assert(size > 2);
    size = size / esize;
    size = roundup_pow_of_two(size);
    this->in_ = 0;
    this->out_ = 0;
    this->data_ = new char[size];
    this->mask_ = size - 1;
};

uint32_t IoRing::ringInBurstReally(const void* src, uint32_t len){
    uint32_t size = this->mask_ + 1;
    uint32_t esize = this->esize_;
    uint32_t l;
    uint32_t off = this->in_;
    off &= this->mask_;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    ::memcpy(this->data_ + off, src, l);
    ::memcpy(this->data_, src + l, len - l);
    /*
    * make sure that the data in the fifo is up to date before
    * incrementing the fifo->in index counter
    */
    smp_wmb();
    return l;
}

uint32_t IoRing::ringOutBurstReally(void* dst, uint32_t len){
    uint32_t size = this->mask_ + 1;
    uint32_t esize = this->esize_;
    uint32_t l;
    uint32_t off = this->out_;
    off &= this->mask_;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);
    ::memcpy(dst, this->data_ + off, l);
    ::memcpy(dst + l, this->data_, len - l);
    /*
    * make sure that the data is copied before
    * incrementing the fifo->out index counter
    */
    smp_wmb();
    return l;
};
} // namespace uhs
