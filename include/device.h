#pragma once

#include <iostream>
#include <string>
#include <stdint.h>

#include "include/io_request.h"

namespace uhs{

using namespace std;

class Device{
public:
    Device(const string& name, const int len):dev_name_(name),dev_capacity_(len){
    };

    inline string getDeviceName(){
        return this->dev_name_;
    }

    inline size_t getDeviceCapacity(){
        return this->dev_capacity_;
    }

    virtual bool initDevice() = 0;

    virtual bool submitIo(IoRequest* req) = 0;

    virtual ~Device();

protected:
    string dev_name_;
    size_t dev_capacity_;
};

}; //namespace uhs