#pragma once 

namespace uhs{

template<typename T>
class SingleTon{
public:
    template<typename ...Args>
    static T& getInstance(Args... args){
        /*after c++11, variable with static is thread safe*/
        static T instance(args)
        return instance;
    }

    /*delete*/
    SingleTon(const SingleTon&) = delete;
    SingleTon(SingleTon&&) = delete;
    SingleTon& operator=(const SingleTon&) = delete;
protected:
    SingleTon() = default;
    virtual ~SingleTon() = default;
};

};  //namespace uhs