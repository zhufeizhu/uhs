#pragma once

namespace uhs
{

class IoRequest;

class NVMBuffer{
public:
    NVMBuffer() = default;

    inline uint64_t submitIo(IoRequest*);
private:
    uint64_t read(IoRequest*);
    uint64_t write(IoRequest*);
    uint64_t clear(IoRequest*);

    
};
} // namespace uhs
