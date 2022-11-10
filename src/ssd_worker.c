#include "ssd_worker.h"

#include <assert.h>
#i  
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mixdk.h"
#include "ssd.h"
#include "mix_meta.h"

#define SSD_QUEUE_NUM 4
#define SSD_WORKER_NUM 4

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

static mix_queue_t** ssd_queue;
static atomic_bool queue_empty[SSD_QUEUE_NUM];
//设置队列任务的大小 测试发现当block的大小超过12k时 性能将不再随着block的
//大小而增加 后面需要再详细测试一下
static const int stripe_size = 4;
static atomic_int completed_ssd_write_block_num = 0;
static io_task_t ssd_task[SSD_QUEUE_NUM];
static pthread_mutex_t ssd_mutex[SSD_QUEUE_NUM];
static io_task_t post_task;

/**
 * 从ssd中读数据
 **/
static inline int mix_read_from_ssd(void* dst,
                                    size_t len,
                                    size_t offset,
                                    size_t flags) {
    return mix_ssd_read(dst, len, offset, flags);
}

/**
 * 向ssd中写数据
 **/
static inline int mix_write_to_ssd(void* src,
                                   size_t len,
                                   size_t offset,
                                   size_t flags) {
    return mix_ssd_write(src, len, offset, flags);
}

static int mix_wait_for_task_completed(io_task_t* task) {
    while(*(task->inflight_buffer) > 0);
    return 0;
}

atomic_int_fast32_t ssd_task_num = 0;
extern mix_metadata_t* meta_data;

static char* read_buffer[4][512];
static int read_index[4][512];
static __thread bool inited = false;
int mix_post_task_to_ssd(io_task_t* task) {
    if(unlikely(!inited)){
        for(int i = 0; i < 4; i++){
            for(int j = 0; j < 512; j++){
                read_index[i][j] = -1;
                read_buffer[i][j] = malloc(SSD_BLOCK_SIZE);
            }
        }
        inited = true;
    }

    while(unlikely(mix_buffer_migrating(meta_data)));
    int ret;
    size_t op_code = task->opcode & (MIX_READ | MIX_WRITE);
    if(unlikely(op_code == MIX_READ)){
        task->read_buffer = (char***)read_buffer;
        task->read_index = (int**)read_index;
        ret = mix_read_from_ssd(task->buf, task->len, task->offset,
                                    task->opcode);
        // mix_wait_for_task_completed(task);
        // for(int i = 0; i < 4; i++){
        //     for(int j = 0; j < 512; j++){
        //         int idx = read_index[i][j];
        //         if(unlikely(idx != -1)){
        //             memcpy(task->buf + idx*SSD_BLOCK_SIZE,read_buffer[i][j],SSD_BLOCK_SIZE);
        //             read_index[i][j] = -1;
        //         }else{
        //             break;
        //         }
        //     }
        // }
    }else if(op_code == MIX_WRITE){
        ret = mix_write_to_ssd(task->buf, task->len, task->offset,
                                    task->opcode);
        //printf("write\n");
    }
    return ret;
}

ssd_info_t* mix_ssd_worker_init(unsigned int size, unsigned int esize) {
    ssd_info_t* ssd_info = NULL;

    pthread_t pid = 0;
    if ((ssd_info = mix_ssd_init()) == NULL) {
        return NULL;
    }
    
    return ssd_info;
}