#ifndef MIX_META_H
#define MIX_META_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "mix_bitmap.h"
#include "mix_bloom_filter.h"
#include "mix_hash.h"
#include "mix_task.h"
#include "mix_logging.h"

#define SEGMENT_NUM 4

typedef struct free_segment {
    size_t size;                         //当前segment的总体的大小
    uint32_t block_num;                  //当前segment的block的个数
    atomic_bool migration;               //是否在执行迁移动作
    atomic_int_fast32_t used_block_num;  //当前segment被使用的block的个数
    pthread_rwlock_t* segment_lock;
    // atomic_bool migration;               //是否正在迁移数据
    mix_bitmap_t* bitmap;  //描述当前segment的bitmap信息
} free_segment_t;

typedef struct mix_metadata {
    uint32_t offset;                //元数据区的偏移
    uint32_t size;                  //元数据区的大小
    uint32_t block_num;             //元数据区block的个数
    uint32_t per_block_num;         //元数据区每个segment的block的个数
    mix_hash_t* hash[SEGMENT_NUM];  //全局的hash
    pthread_cond_t* migrate_cond;
    atomic_bool migration[SEGMENT_NUM];
    mix_counting_bloom_filter_t*
        bloom_filter[SEGMENT_NUM];         //全局的bloom_filter
    free_segment_t segments[SEGMENT_NUM];  //四个free_segment
    mix_log_t* logs[SEGMENT_NUM];
    uint8_t migrate_thread_num;
    int* migrate_segment_idx;
    pthread_mutex_t* migrate_mutex;
} mix_metadata_t;

typedef struct migrate_thread{
    int thread_idx;
    int* segment_idx;
    mix_metadata_t* meta_data;
}migrate_thread_t;

mix_metadata_t* mix_metadata_init(uint32_t block_num);

void mix_metadata_free(mix_metadata_t* meta_data);

int mix_get_next_free_block(mix_metadata_t* meta_data, int idx);

void mix_clear_block(mix_metadata_t* meta_data, io_task_t* task, int idx);

int mix_buffer_block_test(mix_metadata_t* meta_data, uint32_t offset, int idx);

bool mix_write_redirect_blockmeta(mix_metadata_t* meta_data,
                              int idx,
                              uint32_t offset,
                              int bit);

bool mix_has_free_block(mix_metadata_t* meta_data, int idx);

void mix_migrate(mix_metadata_t* meta_data, int idx);

bool mix_segment_migrating(mix_metadata_t* meta_data, int idx);

bool mix_buffer_migrating(mix_metadata_t* meta_data);

void mix_segment_migration_begin(mix_metadata_t* meta_data, int idx);

void mix_segment_migration_end(mix_metadata_t* meta_data, int idx);

bool mix_buffer_rebuild(mix_metadata_t*, int );

bool mix_segment_clear(mix_metadata_t* ,int);

#endif