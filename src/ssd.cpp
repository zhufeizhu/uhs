#include "ssd.h"

#define __USE_GNU 1
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "include/mixdk.h"

namespace uhs{

thread_local bool inited_ = false;

SSDDevice::SSDDevice(const string& name, const uint64_t len):Device(name,len){
    this->block_size_ = 1 << 12;

};

bool SSDDevice::submitIo(IoRequest* req){    
    size_t op_code = req->opcode & (MIX_READ | MIX_WRITE);
    switch (op_code) {
        case MIX_READ:{
            read(req->buf,req->len,req->offset);
            break;
        };
        case MIX_WRITE:{
            write(req->buf,req->len,req->offset);
            break;
        };
        default:{
            //todo 支持其他的操作 比如trim等
            return false;
        }
    }
    return true;
}

/**
 * 初始化设备 默认用设备文件来进行读写 后续可以使用spdk
*/
bool SSDDevice::initDevice(){
    int fd = open(this->dev_name_.c_str(),O_RDWR|O_DIRECT);
    if (fd < 0) {
        perror("Init SSD Device");
        return false;
    }
    this->fd_ = fd;
    return true;
}

SSDDevice::~SSDDevice(){
    ::close(this->fd_);
}

inline size_t SSDDevice::write(const void* src, size_t len, size_t offset){
    size_t off = offset * this->block_size_;
    size_t l = len * this->block_size_;
    pwrite(this->fd_, src,l,off);
    return l;
}

inline size_t SSDDevice::read(void* dst, size_t len, size_t offset){
    size_t off = offset * this->block_size_;
    size_t l = len * this->block_size_;
    pread(this->fd_, dst, l, off);
    return l;
}

}; //namespace uhs