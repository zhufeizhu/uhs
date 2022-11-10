#include "mix_queue.h"
#include <assert.h>
#include <stdatomic.h>
#include <string.h>

static unsigned int min(unsigned int a, unsigned int b) {
    return (a > b) ? b : a;
}

static inline unsigned int __mix_queue_unused(mix_queue_t* queue) {
    return (queue->mask + 1) - (queue->in - queue->out);
}

static inline int fls(int x) {
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

static inline unsigned int roundup_pow_of_two(unsigned int x) {
    return 1UL << fls(x - 1);
}

static inline int __mix_queue_init(mix_queue_t* queue,
                                   void* buffer,
                                   unsigned int size,
                                   unsigned int esize) {
    size = size / esize;

    size = roundup_pow_of_two(size);

    queue->in = 0;
    queue->out = 0;
    queue->data = buffer;
    queue->esize = esize;
    if (size < 2) {
        queue->mask = 0;
        return EINVAL;
    }
    queue->mask = size - 1;
    return 0;
}

static inline unsigned int __mix_enqueue(mix_queue_t* queue,
                                         const void* src,
                                         unsigned int len,
                                         unsigned int off) {
    unsigned int size = queue->mask + 1;
    unsigned int esize = queue->esize;
    unsigned int l;

    off &= queue->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    memcpy(queue->data + off, src, l);
    memcpy(queue->data, src + l, len - l);
    /*
     * make sure that the data in the fifo is up to date before
     * incrementing the fifo->in index counter
     */
    smp_wmb();

    return l;
}

static inline void __mix_dequeue(mix_queue_t* queue,
                                 void* dst,
                                 unsigned int len,
                                 unsigned int off) {
    unsigned int size = queue->mask + 1;
    unsigned int esize = queue->esize;
    unsigned int l;

    off &= queue->mask;
    if (esize != 1) {
        off *= esize;
        size *= esize;
        len *= esize;
    }
    l = min(len, size - off);

    memcpy(dst, queue->data + off, l);
    memcpy(dst + l, queue->data, len - l);
    /*
     * make sure that the data is copied before
     * incrementing the fifo->out index counter
     */
    smp_wmb();
}

mix_queue_t* mix_queue_init(unsigned int size, unsigned int esize) {
    assert(size > 0 && esize > 0);

    mix_queue_t* queue = malloc(sizeof(mix_queue_t));
    assert(queue != NULL);
    void* buffer = malloc(size);
    assert(buffer != NULL);
    if (__mix_queue_init(queue, buffer, size, esize) == EINVAL) {
        return NULL;
    }

    return queue;
}

/**
 *
 **/
unsigned int mix_enqueue(mix_queue_t* queue,
                         const void* src,
                         unsigned int len) {
    unsigned int l;

    l = __mix_queue_unused(queue);
    if (len > l)
        len = l;

    __mix_enqueue(queue, src, len, queue->in);
    queue->in += len;
    return len;
}

unsigned int mix_enqueue_lockfree(mix_queue_t* queue,
                                  const void* src,
                                  unsigned int len) {
    unsigned int l;

    l = __mix_queue_unused(queue);
    if (len > l)
        len = l;

    unsigned int in_ = 0;
    do {
        in_ = queue->in;
    } while (!atomic_compare_exchange_weak(&(queue->in), &in_, in_ + len));

    __mix_enqueue(queue, src, len, in_);

    // queue->in += len;

    return len;
}

/**
 * @param queue: 操作的队列
 * @param dst:出队结果所在的内存
 * @param len:出队的task的个数
 **/
unsigned int mix_dequeue(mix_queue_t* queue, void* dst, unsigned int len) {
    unsigned int l;

    l = queue->in - queue->out;

    if (l == 0) {
        return l;
    }

    if (len > l)
        len = l;

    __mix_dequeue(queue, dst, len, queue->out);
    queue->out += len;
    return len;
}

unsigned int mix_dequeue_lockfree(mix_queue_t* queue,
                                  void* dst,
                                  unsigned int len) {
    unsigned int l;

    l = queue->in - queue->out;

    if (l == 0) {
        return l;
    }

    if (len > l)
        len = l;

    unsigned int out_;

    do {
        out_ = queue->out;
    } while (!atomic_compare_exchange_weak(&(queue->out), &out_, out_ + len));

    __mix_dequeue(queue, dst, len, out_);

    // queue->out += len;
    return len;
}

void mix_queue_free(mix_queue_t* mix_queue) {
    assert(mix_queue != NULL);
    free(mix_queue->data);
    mix_queue->data = NULL;
    mix_queue->esize = 0;
    mix_queue->in = 0;
    mix_queue->out = 0;
    mix_queue->mask = 0;
}