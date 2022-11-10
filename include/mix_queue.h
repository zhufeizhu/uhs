#ifndef MIX_QUEUE_H
#define MIX_QUEUE_H

#include <stdatomic.h>
#include <stdlib.h>
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

#define TASK_SIZE (sizeof(io_task_t))

typedef struct mix_queue {
    unsigned int in;
    unsigned int out;
    unsigned int mask;
    unsigned int esize;
    _Atomic void* data;
} mix_queue_t;

#define A 1

// typedef struct general_task{

//     char padding[9];//保证长度是2的整次幂
// } __attribute__((packed)) general_task_t;

mix_queue_t* mix_queue_init(unsigned int size, unsigned int esize);

unsigned int mix_enqueue(mix_queue_t* queue, const void* src, unsigned int len);

unsigned int mix_dequeue(mix_queue_t* queue, void* dst, unsigned int len);

void mix_queue_free(mix_queue_t* queue);

#endif  // MIXDK_MIX_QUEUE