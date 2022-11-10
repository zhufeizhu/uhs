#pragma once

#include <vector>
#include <stdint.h>

#include "include/io_ring.h"


namespace uhs{

class IoRequest;

class IoChannel{
public:
    IoChannel(int thread_num, int entry_num);

    inline bool submitNvmIo(IoRequest* req);
    inline bool submitBufferIo(IoRequest* req);

    inline IoRequest* obtainNvmIo();
    inline IoRequest* obtainBufferIo();

private:
    IoRing*                 nvm_ring_;      //ring for nvm
    IoRing*                 buffer_ring_;   //ring for buffer
    IoRequest*              nvm_req_;
    IoRequest*              buffer_req_;
};

}; //namespace uhs