#include "io_channel.h"

#include "include/io_request.h"

namespace uhs{

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

IoChannel::IoChannel(int thread_num, int entry_num){
    this->nvm_ring_ = new IoRing(entry_num,sizeof(IoRequest));
    this->buffer_ring_ = new IoRing(entry_num,sizeof(IoRequest));
    this->nvm_req_ = new IoRequest();
    this->buffer_req_ = new IoRequest();
}

inline bool IoChannel::submitNvmIo(IoRequest* req){
    int ret = this->nvm_ring_->ringOut(req);
    if(likely(ret > 0)){
        return true;
    }
    return false;
}

inline bool IoChannel::submitBufferIo(IoRequest* req){
    int ret = this->buffer_ring_->ringOut(req);
    if(likely(ret > 0)){
        return true;
    }
    return false;
}

inline IoRequest* IoChannel::obtainNvmIo(){
    this->nvm_ring_->ringOut(this->nvm_req_);
    return this->nvm_req_;
}

inline IoRequest* IoChannel::obtainBufferIo(){
    this->buffer_ring_->ringOut(this->buffer_req_);
    return this->buffer_req_;
}

};//namespace uhs