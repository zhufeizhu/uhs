#include "mix_meta.h"

#include <stdlib.h>
#include <string.h>

#include "mix_hash.h"
#include "mix_log.h"
#include "mix_queue.h"
#include "mix_task.h"
#include "nvm.h"
#include "ssd.h"

#ifndef BUFFER_QUEUE_NUM
#define BUFFER_QUEUE_NUM 4
#endif

/**
 * @brief 申请并初始化segment
 *
 * @param size 每个segment的大小
 */
static bool mix_free_segment_init(free_segment_t* segment, uint32_t size) {
    if (segment == NULL)
        return false;
    segment->bitmap = mix_bitmap_init(size / BLOCK_SIZE);
    segment->block_num = size / BLOCK_SIZE;
    segment->size = size;
    segment->used_block_num = 0;
    segment->segment_lock = malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(segment->segment_lock,NULL);
    return true;
};

char migrate_buf[8][4096];

void migrate_from_buffer_to_ssd(uint32_t src, uint32_t dst,int idx) {
    mix_buffer_read(migrate_buf[idx],src,0);
    mix_ssd_write(migrate_buf[idx],1,dst,0);
    mix_buffer_clear(src);
}

atomic_int_fast8_t completed_migrate_thread_num[SEGMENT_NUM];

void* migrate(void* arg){
    migrate_thread_t* thread = (migrate_thread_t*)arg;
    mix_metadata_t* meta_data = thread->meta_data;
    int thread_idx = thread->thread_idx;
    printf("init thread id %d\n",thread_idx);
    int segment_idx = 0;
    pthread_mutex_t migrate_mutex;
    pthread_mutex_init(&migrate_mutex,NULL);
    uint8_t mask = 1 << thread_idx;

    while(1){
        //pthread_mutex_lock(&migrate_mutex);
        while(!meta_data->migration[thread_idx]); //pthread_cond_wait(meta_data->migrate_cond,&migrate_mutex);
        segment_idx = *(thread->segment_idx);
        //printf("begin migrate %d and thread is %d and pid is %ld\n",segment_idx,thread_idx,pthread_self());
        int migrate_idx = thread_idx;
        while(migrate_idx < meta_data->hash[segment_idx]->hash_size){
            mix_kv_t kv;
            while(1){
                kv = mix_hash_get_entry_by_idx(meta_data->hash[segment_idx],migrate_idx);
                if(kv.key == (uint32_t)-1 || kv.value == (uint32_t)-1) break;
                migrate_from_buffer_to_ssd(kv.value, kv.key,thread_idx);
                meta_data->segments[segment_idx].used_block_num--;
            }
            migrate_idx = migrate_idx + meta_data->migrate_thread_num;
        }
        //printf("end migrate %d and thread is %d and pid is %ld\n",segment_idx,thread_idx,pthread_self());
        //pthread_mutex_unlock(&migrate_mutex);
        completed_migrate_thread_num[segment_idx]++;
        meta_data->migration[thread_idx] = false;
    }
}

/**
 * @brief 申请并初始化metadata 包括全局的hash 全局的bloom_filter
 *全局的free_segment等
 *
 **/
mix_metadata_t* mix_metadata_init(uint32_t block_num) {
    pthread_t pid;
    mix_metadata_t* meta_data = malloc(sizeof(mix_metadata_t));
    if (meta_data == NULL) {
        mix_log("mix_init_metadata", "malloc for metadata failed");
        return NULL;
    }

    meta_data->block_num = block_num;
    meta_data->size = block_num * BLOCK_SIZE;
    meta_data->per_block_num = block_num / SEGMENT_NUM;
    meta_data->migrate_thread_num = 8;
    size_t per_segment_size = BLOCK_SIZE * meta_data->per_block_num;
    meta_data->migrate_mutex = malloc(sizeof(pthread_mutex_t));
    meta_data->migrate_cond = malloc(sizeof(pthread_cond_t));
    meta_data->migrate_segment_idx = malloc(sizeof(int));
    pthread_mutex_init(meta_data->migrate_mutex,NULL);
    pthread_cond_init(meta_data->migrate_cond, NULL);
    for (int i = 0; i < SEGMENT_NUM; i++) {
        meta_data->hash[i] = mix_hash_init(meta_data->per_block_num,100);
        meta_data->bloom_filter[i] =
            mix_counting_bloom_filter_init(meta_data->per_block_num, 0.01);
        if (!mix_free_segment_init(&(meta_data->segments[i]),
                                   per_segment_size)) {
            mix_log("mix_init_metadata", "init free segment failed");
            return NULL;
        }
    }
    migrate_thread_t* threads = malloc(sizeof(migrate_thread_t)*meta_data->migrate_thread_num);
    pthread_t pids[meta_data->migrate_thread_num];
    for(int i = 0; i < meta_data->migrate_thread_num; i++){
        threads[i].meta_data = meta_data;
        threads[i].segment_idx = meta_data->migrate_segment_idx;
        threads[i].thread_idx = i;
        if(pthread_create(pids + i, NULL,migrate,(void*)(threads + i))){
            mix_log("mix_init_metadata","init migrate threads failed");
            return NULL;
        }
    }
    

    return meta_data;
}

/**
 * @brief 释放segment申请的内存
 *
 * @param segment
 */
void mix_free_free_segment(free_segment_t* segment) {
    if (segment == NULL)
        return;
    mix_bitmap_free(segment->bitmap);
}

/**
 * @brief 释放meta_data申请的内存
 *
 * @param meta_data
 */
void mix_metadata_free(mix_metadata_t* meta_data) {
    for (int i = 0; i < SEGMENT_NUM; i++) {
        mix_free_free_segment(&(meta_data->segments[i]));
        mix_hash_free(meta_data->hash[i]);
        //mix_counting_bloom_filter_free(meta_data->bloom_filter[i]);
    }

    free(meta_data);
}

/**
 * @brief 为task获取free_segment中下一个空闲块 返回的值代表该块在buffer中的偏移
 *
 * @return 不存在空闲块时返回-1 存在时返回其在当前segment中的偏移
 **/
int mix_get_next_free_block(mix_metadata_t* meta_data, int idx) {
    if(meta_data->segments[idx].used_block_num == meta_data->per_block_num) return -1;

    //从free_segment中申请空闲块
    return mix_bitmap_next_zero_bit(meta_data->segments[idx].bitmap);
}

/**
 * @brief 整个流程包含3步来设置元数据
 * 1. 将对应的bitmap设置为dirty
 * 2. 将key-value保存到hash中
 * 3. 将key保存到bloom filter中
 * 通过自描述来保证原子性
 *
 * @return true表示当前idx已满
 *         false表示当前idx未满
 **/
bool mix_write_redirect_blockmeta(mix_metadata_t* meta_data,
                              int idx,
                              uint32_t offset,
                              int value) {
    uint32_t bit = (value - idx)/SEGMENT_NUM;
    //将对应的bitmap设置为dirty
    if (unlikely(!mix_bitmap_set_bit(meta_data->segments[idx].bitmap,bit))) {
        return false;
    }

    //将key-value保存到hash中
    mix_hash_put(meta_data->hash[idx], offset, value);
    //将key保存到bloom filter中
    // mix_counting_bloom_filter_add(meta_data->bloom_filter[idx], offset);

    meta_data->segments[idx].used_block_num++;
    return false;
}

/**
 * @brief 释放task占用的free_segment中块
 * 整个流程包含3步
 * 1. 将key从bloom filter中移除
 * 2. 将key-value从hash中移除
 * 3. 将对应的bitmap设置为clean
 * 需要保证以上为原子操作
 **/
void mix_clear_block(mix_metadata_t* meta_data, io_task_t* task, int idx) {
    for(int i = 0; i < task->len; i++){
        // 查询是否在hash中
        int value = mix_hash_get(meta_data->hash[idx], task->offset + i*BUFFER_QUEUE_NUM);
        if (value == -1) {
            return;
        }

        mix_hash_delete(meta_data->hash[idx], task->offset + i*BUFFER_QUEUE_NUM);

        //将对应的bitmap设置为clean
        int bit = value % meta_data->per_block_num;
        mix_bitmap_clear_bit(meta_data->segments[idx].bitmap, bit);
        mix_buffer_clear(idx*meta_data->per_block_num + bit);
        meta_data->segments[idx].used_block_num--;
        //printf("clear block %lld\n",task->offset + i);
    }
}

/**
 * @brief 判断offset指代的block是否在buffer中
 *
 * @return 如果在则返回对应的偏移 如果不在则返回-1
 **/
int mix_buffer_block_test(mix_metadata_t* meta_data, uint32_t offset, int idx) {
    int value = -1;
    value = mix_hash_get(meta_data->hash[idx], offset);
    if (value == -1) {
        return value;
    }

    int bit = value % meta_data->per_block_num;
    if (!mix_bitmap_test_bit(meta_data->segments[idx].bitmap, bit)) {
        value = -1;
        return value;
    }

    return value;
}

/**
 * @brief 判断当前segment是否还有空闲块
 *
 * @param idx segment的下标
 * @return true 有空闲块
 * @return false 无空闲块
 */
inline bool mix_in_migration(mix_metadata_t* meta_data, int idx) {
    return meta_data->segments[idx].migration = true;
}


/**
 * @brief 将指定idx中buffer的数据迁入到ssd中
 *
 * @param idx
 */
void mix_migrate(mix_metadata_t* meta_data, int idx) {
    meta_data->hash[idx]->hash_node_entry_idx = 0;
    // for (int i = 0; i < meta_data->per_block_num; i++) {
    //     mix_kv_t kv = mix_hash_get_entry(meta_data->hash[idx]);
    //     if (kv.key == (uint32_t)-1 && kv.value == (uint32_t)-1)
    //         continue;
    //     migrate_from_buffer_to_ssd(kv.value, kv.key);
    //     //mix_counting_bloom_filter_remove(meta_data->bloom_filter[idx] ,kv.key);
    //     meta_data->segments[idx].used_block_num--;
    // }
}

bool mix_buffer_rebuild(mix_metadata_t* meta_data,int idx){
    if(meta_data == NULL){
        mix_log("mix_rebuild","meta data is NULL");
        return false;
    }
    printf("rebuild %d segment start\n",idx);
    buffer_meta_t buffer_meta;
    int per_block_num = meta_data->per_block_num;
    memset(&buffer_meta,0,sizeof(buffer_meta_t));
    for(int off = 0; off < meta_data->per_block_num; off++){
        mix_buffer_get_meta(&buffer_meta,idx*per_block_num+off);
        if(buffer_meta.status&1){
            //表明当前块有效
            mix_bitmap_set_bit(meta_data->segments[idx].bitmap,off);
            meta_data->segments[idx].used_block_num++;
            mix_hash_put(meta_data->hash[idx],buffer_meta.offset,off);
        }
    }
    printf("rebuild %d segment finish\n",idx);
    return true;
}

bool mix_segment_clear(mix_metadata_t* meta_data,int idx){
    int per_block_num = meta_data->per_block_num;
    for(int i = 0; i < meta_data->per_block_num; i++){
        mix_buffer_clear(idx*per_block_num + i);
    }
}


inline bool mix_segment_migrating(mix_metadata_t* meta_data, int idx) {
    return meta_data->segments[idx].migration;
}

inline bool mix_buffer_migrating(mix_metadata_t* meta_data) {
    return meta_data->segments[0].migration &&
           meta_data->segments[1].migration &&
           meta_data->segments[2].migration && meta_data->segments[3].migration;
}

inline void mix_segment_migration_begin(mix_metadata_t* meta_data, int idx) {
    *(meta_data->migrate_segment_idx) = idx;
    atomic_exchange(completed_migrate_thread_num + idx,0);
    for(int i = 0; i < meta_data->migrate_thread_num; i++)
        meta_data->migration[i] = true;
    //pthread_cond_broadcast(meta_data->migrate_cond);
}

inline void mix_segment_migration_end(mix_metadata_t* meta_data, int idx) {
    while(completed_migrate_thread_num[idx] < meta_data->migrate_thread_num);
    atomic_exchange(completed_migrate_thread_num + idx,0);
    mix_bitmap_clear(meta_data->segments[idx].bitmap);
}

inline bool mix_has_free_block(mix_metadata_t* meta_data, int idx) {
    return meta_data->segments[idx].used_block_num <
           meta_data->segments[idx].block_num;
}