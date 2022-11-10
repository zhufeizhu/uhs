#pragma once
#include <pthread.h>
#include <unistd.h>

namespace uhs{
class Bitmap{
    using byte          = char;
public:
    Bitmap()



private:
    byte*               bytes_;
    uint32_t            bit_num_;
    uint32_t            byte_num_;
    int                 next_bit_;

};
};  //namespace uhs


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* array;
    int bytes;
    int next_bit;
    size_t* nvm_offset;  //用来记录在nvm中的偏移 在启动时使用mmap映射到内存中
                         //暂时不使用
    pthread_rwlock_t* rwlock;
} mix_bitmap_t;

//常规的bitmap 用在记录fragment中的block是否被使用
mix_bitmap_t* mix_bitmap_init(int size);

int mix_bitmap_set_bit(mix_bitmap_t* bitmap, int nr);

int mix_bitmap_clear_bit(mix_bitmap_t* bitmap, int nr);

void mix_bitmap_clear(mix_bitmap_t* bitmap);

int mix_bitmap_test_bit(mix_bitmap_t* bitmap, int nr);

int mix_bitmap_free(mix_bitmap_t* bitmap);

int mix_bitmap_next_zero_bit(mix_bitmap_t* bitmap);

// couting bitmap 用在counting bloom filter中
//  reference from https://github.com/bitly/dablooms
int mix_counting_bitmap_increment(mix_bitmap_t* bitmap,
                                  unsigned int index,
                                  long offset);

int mix_counting_bitmap_decrement(mix_bitmap_t* bitmap,
                                  unsigned int index,
                                  long offset);

int mix_counting_bitmap_check(mix_bitmap_t* bitmap,
                              unsigned int index,
                              long offset);

#ifdef __cplusplus
}
#endif

#endif