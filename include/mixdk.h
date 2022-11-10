#ifndef MIX_MIXDK_H
#define MIX_MIXDK_H
#include <unistd.h>
#include <stdint.h>
#define MIX_READ 1
#define MIX_WRITE 1 << 2
#define MIX_LATENCY 1 << 3
#define MIX_THROUGHOUT 1 << 4
#define MIX_SYNC 1 << 5
#define MIX_CLEAR 1 << 6

#define NVM_BLOCK_SIZE (1<<12) //4096
#define SSD_BLOCK_SIZE (1<<12)  //4096

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

int mixdk_init(uint8_t submit_queue_num);

size_t mixdk_write(void* buf, size_t len, size_t offset, size_t flags, int idx);

size_t mixdk_read(void* buf, size_t len, size_t offset, size_t flags, int idx);

int mixdk_destroy();

void mixdk_flush();

size_t mix_completed_task_num();

void mix_segments_clear();

#endif  // MIX_MIXDK_H