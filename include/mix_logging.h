#ifndef MIX_LOGGING_T
#define MIX_LOGGING_T

#include "stdatomic.h"
#include "mix_task.h"

typedef struct mix_log_entry{
    unsigned int index;
    atomic_int_fast8_t flag;
    io_task_t* task;
}mix_log_entry_t;

typedef struct mix_log{
    void* begin;
    unsigned int capacity;
    unsigned int entry_size;
    unsigned int mask;
    atomic_int_fast16_t head;  //当前log的头部
    atomic_int_fast16_t tail;  //当前log的尾部
}mix_log_t;

mix_log_t* mix_log_init(unsigned int entry_num);

void mix_log_free(mix_log_t* mix_log);

int mix_log_append(mix_log_t* log, io_task_t* task);

int mix_log_release(mix_log_t* log);

#endif