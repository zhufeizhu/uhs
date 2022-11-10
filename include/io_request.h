#pragma once

#include <stdint.h>

namespace  uhs
{

enum OP{
    OP_READ = 1,
    OP_WRITE = 1 << 1,
    OP_CLEAR = 1 << 2
};

struct IoRequest {
    //存放读写的数据
    char* buf;
    //读写数据的长度
    uint64_t len;
    //读写数据的偏移   
    uint64_t offset;  
    //读写数据的偏移
    char*** read_buffer;
    int** read_index;
    //读写的结果
    int* inflight_buffer;     
    //读写的操作码
    OP opcode;  
    uint8_t type;  //读写的类型 如对NVM的读写还是对SSD的读写
    uint8_t queue_idx;  //标明当前任务所在队列的索引
    IoRequest* original_task;



} __attribute__((packed));
} // namespace  uhs
