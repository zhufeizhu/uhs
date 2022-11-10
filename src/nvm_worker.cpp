#include "nvm_worker.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mix_meta.h"
#include "nvm.h"
#include "ssd_worker.h"

#define NVM_QUEUE_NUM 4
#define BUFFER_QUEUE_NUM 4

static const int block_size = 4096;
static const int threshold = 1;
static const int stripe_size = 4;
static mix_queue_t** nvm_queue = NULL;
static mix_queue_t** buffer_queue = NULL;
mix_metadata_t* meta_data = NULL;

static pthread_cond_t rebuild_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex[BUFFER_QUEUE_NUM];

static pthread_spinlock_t metadata_lock[NVM_QUEUE_NUM];
static pthread_spinlock_t buffer_lock[NVM_QUEUE_NUM];

static io_task_t migrate_task;
static io_task_t nvm_task[NVM_QUEUE_NUM];
static io_task_t buffer_task[NVM_QUEUE_NUM];
static atomic_bool rebuild = false;


atomic_int_fast32_t completed_nvm_write_block_num = 0;

void mix_migrate_segment(int idx);

static inline size_t mix_write_to_nvm(void* src,
                                      size_t len,
                                      size_t offset,
                                      size_t flags) {
    return mix_nvm_write(src, len, offset, flags);
}

static inline size_t mix_write_to_buffer(void* dst,
                                         size_t src_block,
                                         size_t dst_block,
                                         size_t flags) {
    return mix_buffer_write(dst, src_block, dst_block, flags);
}

static int mix_wait_for_task_completed(io_task_t* task) {
    while(*(task->inflight_buffer) > 0);
    return 0;
}

io_task_t* get_task_from_nvm_queue(int idx) {
    //pthread_spin_lock(&metadata_lock[idx]);
    int len = mix_dequeue(nvm_queue[idx], &nvm_task[idx], 1);
    //pthread_spin_unlock(&metadata_lock[idx]);
    if (len == 0) {
        return NULL;
    } else {
        return &nvm_task[idx];
    }
}

static atomic_int dequeue_task_num = 0;

io_task_t* get_task_from_buffer_queue(int idx) {
    //if (pthread_mutex_trylock(&mutex[idx])) {
        //return NULL;
    //}
    int len = mix_dequeue(buffer_queue[idx], &buffer_task[idx], 1);
    if (len == 0) {
        //pthread_mutex_unlock(&mutex[idx]);
        return NULL;
    } else {
        return &buffer_task[idx];
    }
}

static atomic_int task_num = 0;

//根据逻辑地址从ssd中读取数据 这是一个阻塞的动作
static size_t redirect_read(io_task_t* task, int idx) {
    // 检查每一个块 判断是否在buffer中
    io_task_t* original_task = task->original_task;
    while(original_task->read_buffer == NULL || original_task->read_index == NULL);
    int offset = -1;
    size_t ret = 0;
    int num = 0;
    for(int i = 0; i < task->len; i++){
        if((offset = mix_buffer_block_test(meta_data,task->offset + i*SSD_BLOCK_SIZE,idx)!=-1)){
            ret += mix_buffer_read((char*)(original_task->read_buffer+idx*512+num), offset + idx * meta_data->per_block_num,
                    task->opcode);
            *(int*)(original_task->read_index+idx*512 + num) = task->offset + i * BUFFER_QUEUE_NUM;
        }
    }
    atomic_fetch_sub(original_task->inflight_buffer,task->len);
}
        

/**
 * @brief 执行ssd重定向写
 * 小写: 将内容写到buffer中 并在元数据中添加对应的信息
 * 大写: 将任务转发到ssd_queue中 并清楚元数据中对应的信息
 *
 * @param task 要执行重定向写的task
 * @param idx you
 * @return int
 */
static int redirect_write(io_task_t* task, int idx) {
    int offset = mix_buffer_block_test(meta_data, task->offset, idx);
    if (likely(offset == -1)) {
        //如果不在buffer中
        offset = mix_get_next_free_block(meta_data, idx);
        if (unlikely(offset == -1)) {
            // printf("id is %lld\n",task->offset);
            // struct timespec start,end;
            // clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            mix_migrate_segment(idx);
            // clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            // size_t time = (end.tv_sec - start.tv_sec) * 1000000 +
            //                        (end.tv_nsec - start.tv_nsec) / 1000;
            // printf("time is %llu us\n", time);
            offset = mix_get_next_free_block(meta_data, idx);
        }
        mix_write_redirect_blockmeta(meta_data, idx, task->offset, offset);
    }
    mix_write_to_buffer(task->buf, task->offset, idx * meta_data->per_block_num + offset,task->opcode);
    return task->len;
}

/**
 * @brief 将满了的segment中的数据迁移到对应的ssd中
 * 在正式开始迁移前需要保证buffer_queue中的task都执行完成
 *
 * @param idx 需要执行迁移的segment的下标
 */
void mix_migrate_segment(int idx) {
    int segment_idx = idx;
    pthread_mutex_lock(meta_data->migrate_mutex);
    mix_segment_migration_begin(meta_data, segment_idx);
    //等待ssd_queue中的所有任务执行完毕
    //mix_migrate(meta_data, segment_idx);
    mix_segment_migration_end(meta_data, segment_idx);
    pthread_mutex_unlock(meta_data->migrate_mutex);
}



char* buf = NULL;

static atomic_int_fast8_t rebuild_num = 0;

static void nvm_worker(void* arg) {
    int idx = *(int*)arg;  //当前线程对应的队列序号
    printf("[nvm worker %d init]\n", idx);
    pthread_mutex_lock(&mutex[idx]);
    while(!rebuild) pthread_cond_wait(&rebuild_cond,&mutex[idx]);
    if(mix_buffer_rebuild(meta_data,idx)){
        rebuild_num++;
    }
    pthread_mutex_unlock(&mutex[idx]);

    struct sched_param param;
    //Set scheduler and priority.
    param.sched_priority = 50;
    sched_setscheduler(0, SCHED_FIFO, &param);
    int len = 0;
    int ret = 0;
    uint8_t seq = 0;
    uint8_t mask = 0x01;
    io_task_t* task = NULL;
    while (1) {
        if (seq & mask)
            task = get_task_from_nvm_queue(idx);
        else {
            task = get_task_from_buffer_queue(idx);
        }

        seq++;
        if (task == NULL) {
            continue;
        }

        size_t op_code = task->opcode & (MIX_READ | MIX_WRITE | MIX_CLEAR);
        if (task->type == NVM_TASK) {
            switch (op_code) {
                case MIX_READ: {
                    ret = mix_nvm_read(task->buf, task->len, task->offset,
                                            task->opcode);
                    atomic_fetch_sub(task->inflight_buffer,ret);
                    break;
                };
                case MIX_WRITE: {
                    ret = mix_nvm_write(task->buf, task->len, task->offset,
                                           task->opcode);
                    atomic_fetch_sub(task->inflight_buffer,ret);
                    break;
                }
                default: {
                    ret = 0;
                    break;
                }
            }
        } else if (task->type == SSD_TASK) {
            switch (op_code) {
                case MIX_CLEAR:{
                    mix_clear_block(meta_data,task,idx);
                    break;
                }
                case MIX_READ: {
                    ret = redirect_read(task, idx);
                    break;
                }
                case MIX_WRITE: {
                    ret = redirect_write(task, idx);
                    atomic_fetch_sub(task->inflight_buffer,ret);
                    break;
                }
                default: {
                    ret = 0;
                    break;
                }
            }
        }
    }
    return;
}

int indexs[NVM_QUEUE_NUM];

nvm_info_t* mix_nvm_worker_init(unsigned int size, unsigned int esize) {
    nvm_info_t* nvm_info = NULL;
    pthread_t pid[NVM_QUEUE_NUM];

    if ((nvm_info = mix_nvm_init()) == NULL) {
        return NULL;
    }

    nvm_queue = malloc(NVM_QUEUE_NUM * sizeof(mix_queue_t*));
    if (nvm_queue == NULL) {
        perror("alloc memory for mix queue failed\n");
        return NULL;
    }

    for (int i = 0; i < NVM_QUEUE_NUM; i++) {
        nvm_queue[i] = mix_queue_init(size, esize);
        if (nvm_queue[i] == NULL) {
            return NULL;
        }
    }

    for (int i = 0; i < NVM_QUEUE_NUM; i++)
        indexs[i] = i;

    for (int i = 0; i < NVM_QUEUE_NUM; i++) {
        if (pthread_create(&pid[i], NULL, (void*)nvm_worker,
                           (void*)(indexs + i))) {
            printf("create ssd queue failed\n");
            free(nvm_info);
            return NULL;
        }
    }
    return nvm_info;
}

buffer_info_t* mix_buffer_worker_init(unsigned int size, unsigned int esize) {
    buffer_info_t* buffer_info = NULL;
    if ((buffer_info = mix_buffer_init()) == NULL) {
        return NULL;
    }
    meta_data = mix_metadata_init(buffer_info->block_num);
    if (meta_data == NULL) {
    }

    buffer_queue = malloc(sizeof(mix_queue_t*) * BUFFER_QUEUE_NUM);
    for (int i = 0; i < BUFFER_QUEUE_NUM; i++) {
        pthread_mutex_init(&mutex[i], NULL);
        pthread_spin_init(&metadata_lock[i],0);
        pthread_spin_init(&buffer_lock[i],0);
        buffer_queue[i] = mix_queue_init(size, esize);
        if (buffer_queue == NULL) {
            mix_metadata_free(meta_data);
            return NULL;
        }
    }

    return buffer_info;
}

inline void mix_nvm_mmap(nvm_info_t* nvm_info, buffer_info_t* buffer_info) {
    mix_mmap(nvm_info, buffer_info);
}

int pre_nvm_ind = 0;

static atomic_int retry_time = 0;

/**
 * task入队到nvm_queue中 nvm_queue的个数在编译器确定
 * 目前入队的策略是循环遍历
 **/

int mix_post_task_to_metadata(io_task_t* task) {
    // struct timespec start,end;
    // if((local_time)%100 == 0)
    //     clock_gettime(CLOCK_MONOTONIC_RAW,&start);
    int idx = task->offset&(NVM_QUEUE_NUM-1);
    pthread_spin_lock(&metadata_lock[idx]);
    mix_enqueue(nvm_queue[idx], task, 1);
    pthread_spin_unlock(&metadata_lock[idx]);
    mix_wait_for_task_completed(task);
    // if((local_time++)%100 == 0)
    //     clock_gettime(CLOCK_MONOTONIC_RAW,&end);

    // size_t time = (end.tv_sec - start.tv_sec) * 1000000 +
    //                                (end.tv_nsec - start.tv_nsec) / 1000;
    // printf("time is %llu us\n", time);
    return 1;
}

static atomic_int buffer_task_num = 0;

int mix_post_task_to_buffer(io_task_t* task) {
    int offset = task->offset;
    int mask = BUFFER_QUEUE_NUM - 1;
    int end = task->offset + task->len;
    int idx = (task->offset / stripe_size) % BUFFER_QUEUE_NUM;
    io_task_t post_task;
    post_task.inflight_buffer = task->inflight_buffer;
    post_task.original_task = task;
    post_task.opcode = task->opcode;
    post_task.type = task->type;
    int len = (end - offset)/BUFFER_QUEUE_NUM;
    for(int i = 0; i < BUFFER_QUEUE_NUM; i++){
        post_task.offset = offset+i;
        post_task.len = len + ((((end-offset)&mask) > i)?1:0);
        if(post_task.len == 0) break;
        pthread_spin_lock(&buffer_lock[idx]);
        while (!mix_enqueue(buffer_queue[idx], &post_task, 1));
        pthread_spin_unlock(&buffer_lock[idx]);
        idx = (idx + 1)&mask;
    }
}

int mix_post_small_task_to_buffer(io_task_t* task){
    int idx = task->offset&(3);
    pthread_spin_lock(&buffer_lock[idx]);
    while (!mix_enqueue(buffer_queue[idx],task, 1));
    pthread_spin_unlock(&buffer_lock[idx]);
    mix_wait_for_task_completed(task);
}


int mix_post_task_to_data(io_task_t* task){
    size_t op_code = task->opcode & (MIX_READ | MIX_WRITE);
    //struct timespec start,end;
    if(op_code == MIX_WRITE && task->len <= threshold){
        mix_post_small_task_to_buffer(task);
        return task->len;
    }else if(op_code == MIX_WRITE && task->len > threshold){
        task->opcode |= MIX_CLEAR;
        //clock_gettime(CLOCK_MONOTONIC_RAW,&start);
        mix_post_task_to_buffer(task);
        mix_post_task_to_ssd(task);
        //clock_gettime(CLOCK_MONOTONIC_RAW,&end);
        // printf("time is %ld\n",(end.tv_sec - start.tv_sec) * 1000000 +
        //     (end.tv_nsec - start.tv_nsec) / 1000);
        return task->len;
    }else if(op_code == MIX_READ){
        //mix_post_task_to_buffer(task);
        mix_post_task_to_ssd(task);
        return task->len;
    }
}

void mix_rebuild(){
    for(int i = 0; i < SEGMENT_NUM; i++){
        mix_segment_clear(meta_data,i);
    }
    rebuild = true;
    pthread_cond_broadcast(&rebuild_cond);
    while(rebuild_num < SEGMENT_NUM);
}