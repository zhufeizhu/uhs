#pragma once

#include <vector>

#include "include/device.h"

namespace uhs{

using namespace std;

class SSDDevice:public Device{
public:
    SSDDevice(const string& name, const uint64_t len);

    bool initDevice() override;

    bool submitIo(IoRequest*) override;

    ~SSDDevice() override;

private:
    size_t read(void* src, size_t len, size_t offset);
    size_t write(const void* dst, size_t len, size_t offset);
    
    int fd_;
    size_t block_num_;
    uint32_t block_size_;
    vector<vector<char*>> read_buf_;
    vector<vector<int>> rad_idx_;
};

};//namespace uhs