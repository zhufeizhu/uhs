#pragma once
#include <stdint.h>
#include <memory>

namespace uhs{

class IoRequest;
class IoChannel;
class NVMZone;
class NVMBuffer;
class std::thread;

class IoThread{
public:
    IoThread(IoChannel* channel, int idx);

    inline void run(){
        this->start_ = true;
    }

    inline bool nvmRound(){
        return (this->mask_ & this->seq_++);
    }

    inline IoRequest* poll();
private:
    using NVMZonePtr        = std::unique_ptr<NVMZone>;
    using NVMBufferPtr      = std::unique_ptr<NVMBuffer>;
    using IoChannelPtr      = std::unique_ptr<IoChannel>;

    inline bool start(){
        return start_;
    };

    void loop();

    bool                        start_;
    uint8_t                     idx_;
    thread*                     thread_;
    IoChannelPtr                channel_;
    NVMZonePtr                  zone_;
    NVMBufferPtr                buffer_;
    uint8_t                     seq_;
    uint8_t                     mask_;
};

}; //namespace uhs