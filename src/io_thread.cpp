#include "include/io_thread.h"

#include <thread>

#include "include/io_request.h"
#include "include/io_channel.h"
#include "include/nvm_zone.h"
#include "include/nvm_buffer.h"

namespace uhs{

using namespace std;

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

IoThread::IoThread(IoChannel* channel, int idx) noexcept{
    this->channel_.reset(channel);
    this->buffer_ = make_unique<NVMBuffer>();
    this->zone_ = make_unique<NVMZone>();
    this->start_ = false;
    this->seq_ = 0;
    this->mask_ = 0x01;
    this->thread_ = new thread(&IoThread::loop,this);
}


IoRequest* IoThread::poll(){
    IoRequest* req = nullptr;
    if (nvmRound()) {
        req = channel_->obtainNvmIo();
    } else {
        req = channel_->obtainBufferIo();
    }
    return req;
}

void IoThread::loop(){
    //todo rebuild metadata
    struct sched_param param;
    param.sched_priority = 50;
    sched_setscheduler(thread_->native_handle(), SCHED_FIFO, &param);
    IoRequest* req = nullptr;
    while(likely(this->start())){
        req = poll();
        if(!req) continue;

        
    }

}

}; //namespace uhs