#pragma once

#include "include/io_thread_pool.h"

#include <iostream>
#include <pthread.h>
#include <assert.h>

namespace uhs{

bool IoThreadPool::startThreadPool(int thread_num){
    assert(thread_num > 0);
    this->io_thread_num_ = thread_num;

    

}

}; //namespace uhs
