#include "include/nvm_buffer.h"

#include <cstdlib>

#include "include/io_request.h"

namespace uhs{

uint64_t NVMBuffer::submitIo(IoRequest* req){
    switch (req->opcode) {
        case OP_WRITE:{
            

            break;
        }
        case OP_READ:{
            
            break;
        }
        case OP_CLEAR:{
            
            break;
        };
        default:{
            //impossible run here
            exit(-1);
        }
    }
}

};