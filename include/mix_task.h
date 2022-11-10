#ifndef MIX_TASK_H
#define MIX_TASK_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include "mixdk.h"

#define NVM_TASK 1
#define SSD_TASK 2
#define BUFFER_TASK 3
#define CLEAR_TASK 4

#define TASK_SUCCEED 0
#define TASK_FAILED 1

#define BLOCK_SIZE 4096  //定义全局的block的大小
#define BLOCK_SIZE_BIT 12

// 对用户读写请求的抽象
typedef struct io_task {
    char* buf;      //存放读写的数据
    size_t len;     //读写数据的长度
    size_t offset;  //读写数据的偏移
    char*** read_buffer;
    int** read_index;
    atomic_int_fast32_t* inflight_buffer;     //读写的结果
    size_t opcode;  //读写的操作码
    __uint8_t type;  //读写的类型 如对NVM的读写还是对SSD的读写
    __uint8_t queue_idx;  //标明当前任务所在队列的索引
    struct io_task* original_task;
} __attribute__((packed)) io_task_t;

// 对任务进行处理后得到的任务数组
typedef struct io_tasks {
    io_task_t tasks[1024];  //当前的任务集合
    int task_num;           //当前任务的个数
} io_task_v;

#endif