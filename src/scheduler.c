#include "scheduler.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mix_log.h"
#include "mix_meta.h"
#include "mix_task.h"
#include "mixdk.h"
#include "nvm_worker.h"
#include "ssd_worker.h"

//数据定义区
static scheduler_ctx_t* sched_ctx = NULL;
static const size_t threshold = 1928765;

#ifndef NVM_QUEUE_NUM
#define NVM_QUEUE_NUM 4
#endif

static io_task_t* p_task;
static atomic_int_fast64_t completed_read_task = 0;

static int mix_wait_for_task_completed(io_task_t* task) {
    while(*(task->inflight_buffer) > 0);
    return 0;
}

static atomic_int task_num = 1;
static atomic_int retry_time = 0;

int mix_post_task_to_io(io_task_t* task) {
    if ((size_t)(task->offset + task->len) < threshold) {
        task->type = NVM_TASK;
        task->queue_idx = task->offset%NVM_QUEUE_NUM;
        mix_post_task_to_metadata(task);
        return task->len;
    }else{
        task->offset -= threshold;
        task->type = SSD_TASK;
        mix_post_task_to_data(task);
        return task->len;
    }
}

/**
 * @brief scheduler初始化函数 包括
 *
 * @param size
 * @param esize
 * @param max_current
 * @return int
 */
int mix_init_scheduler(unsigned int size, unsigned int esize, uint8_t submit_queue_num) {
    pthread_t scheduler_thread = 0;
    ssd_info_t* ssd_info = NULL;
    nvm_info_t* nvm_info = NULL;
    buffer_info_t* buffer_info = NULL;

    sched_ctx = malloc(sizeof(scheduler_ctx_t));
    if (sched_ctx == NULL) {
        mix_log("mix_init_scheduler", "malloc for sched_ctx failed");
        return -1;
    }

    sched_ctx->max_current = submit_queue_num;
    ssd_info = mix_ssd_worker_init(size, esize);
    if (ssd_info == NULL) {
        return -1;
    }
    buffer_info = mix_buffer_worker_init(size, esize);
    if (buffer_info == NULL) {
        return -1;
    }
    nvm_info = mix_nvm_worker_init(size, esize);
    if (nvm_info == NULL) {
        return -1;
    }

    mix_nvm_mmap(nvm_info, buffer_info);

    mix_rebuild();

    sched_ctx->ssd_info = ssd_info;
    sched_ctx->nvm_info = nvm_info;
    sched_ctx->buffer_info = buffer_info;
    return 0;
}