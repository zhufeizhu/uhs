#pragma once

#include "singleton.h"

namespace uhs{
class IoThreadPool: SingleTon<IoThreadPool>{
public:
    static IoThreadPool* getInstance(){
        return &(SingleTon::getInstance());
    }

    bool startThreadPool(int);

    static int getIoThreadNum(){
        return io_thread_num_;
    }

private:
    static int io_thread_num_;
    IoThreadPool() = default;
    ~IoThreadPool() = default;
    IoThreadPool(const IoThreadPool&) = default;
    IoThreadPool(IoThreadPool&&) = default;
    IoThreadPool& operator=(const IoThreadPool&) = default;
    IoThreadPool& operator=(IoThreadPool&&) = default;
};
}




