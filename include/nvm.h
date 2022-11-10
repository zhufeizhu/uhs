#pragma once

#include "include/device.h"

#include <string>

namespace uhs{

using namespace std;

class NVMDevice:public Device{
public:
    NVMDevice(const string& name, const uint64_t len);

    bool submitIo(IoRequest*) override;

    bool initDevice() override;

    ~NVMDevice() override;

private:
    void* nvm_addr_;
    size_t block_num_;
    size_t per_block_num_;
    uint32_t block_size_;
    uint8_t queue_num_;
    size_t mapped_len_;

    size_t read(void* dst, size_t len, size_t offset);

    size_t write(const void* src, size_t len, size_t offset);
};

};//namespace uhs




#include <stdint.h>
typedef struct nvm_info {
    void* nvm_addr;  // nvm的内存起始地址
    size_t block_num;
    size_t per_block_num;
    size_t nvm_capacity;
    int block_size;
    uint8_t queue_num;
} nvm_info_t;

typedef struct buffer_info {
    void* buffer_addr;  // buffer的内存起始地址
    void* meta_addr;    // buffer的字描述块地址
    size_t block_num;
    size_t buffer_capacity;
    uint8_t segment_num;
} buffer_info_t;

typedef struct buffer_meta {
    size_t status;     //状态位
    size_t timestamp;  //时间戳
    size_t offset;     //目标地址
    size_t flags;      //保留位
} __attribute__((packed)) buffer_meta_t;

nvm_info_t* mix_nvm_init();

buffer_info_t* mix_buffer_init();

void mix_mmap(nvm_info_t* nvm_info, buffer_info_t* buffer_info);

size_t mix_buffer_write(void* src, size_t, size_t, size_t);

size_t mix_buffer_read(void* dst, size_t dst_block, size_t flags);

void mix_buffer_clear(size_t dst_block);

void mix_buffer_get_meta(buffer_meta_t*, int);

#endif