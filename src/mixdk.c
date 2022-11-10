#include "mixdk.h"

#ifndef _WIN32
#include <unistd.h>
#endif
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "nvm_worker.h"
#include "mix_task.h"
#include "scheduler.h"
#include "nvm.h"

const static uint8_t max_submit_queue_num = 16;

static io_task_t tasks[16];
static atomic_int_fast32_t inflight_buffer[16];

int mixdk_init(uint8_t submit_queue_num) {
    if(submit_queue_num <= 0 || submit_queue_num > 16) return -1;
    //tasks =  (sizeof(io_task_t)*submit_queue_num);
    mix_init_scheduler(4096 * TASK_SIZE, TASK_SIZE, submit_queue_num);
    return 0;
}

inline void init_io_task(void* buf, size_t len, size_t offset, size_t opcode,int idx){
    tasks[idx].buf = buf;
    tasks[idx].len = len;
    tasks[idx].offset = offset;
    tasks[idx].opcode = opcode;
    inflight_buffer[idx] = len;
    tasks[idx].inflight_buffer = &inflight_buffer[idx];
    tasks[idx].read_buffer = NULL;
    tasks[idx].read_index = NULL;
    tasks[idx].queue_idx = idx;
}

size_t mixdk_write(void* src,
                   size_t len,
                   size_t offset, 
                   size_t flags,
                   int idx) {
    init_io_task(src,len,offset,flags | MIX_WRITE,idx);
    return mix_post_task_to_io(&tasks[idx]);
}

size_t mixdk_read(void* dst, size_t len, size_t offset, size_t flags, int idx) {
    tasks[idx].buf = dst;
    tasks[idx].len = len;
    tasks[idx].offset = offset;
    tasks[idx].opcode = MIX_READ | flags;
    inflight_buffer[idx] = len;
    tasks[idx].inflight_buffer = &inflight_buffer[idx];
    tasks[idx].read_buffer = NULL;
    tasks[idx].read_index = NULL;
    tasks[idx].queue_idx = idx;

    return mix_post_task_to_io(&tasks[idx]);
}

void mix_segments_clear(){
    mix_rebuild();
}