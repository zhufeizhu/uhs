#ifndef MIX_SCHEDULER_H
#define MIX_SCHEDULER_H

#include "mix_queue.h"

#include <pthread.h>
#include <stdatomic.h>

#include "mix_task.h"
#include "nvm.h"
#include "ssd.h"

#define READ_AHEAD 0
#define WRITE_AHEAD 1

typedef struct scheduler_ctx {
    mix_queue_t** submit_queue;
    uint8_t max_current;
    ssd_info_t* ssd_info;  // ssd的信息
    nvm_info_t* nvm_info;  // nvm的信息
    buffer_info_t* buffer_info;
} scheduler_ctx_t;

int mix_init_scheduler(unsigned int, unsigned int, uint8_t);

int mix_post_task_to_io(io_task_t*);

size_t get_completed_task_num();

#endif  // MIX_SCHEDULER_H