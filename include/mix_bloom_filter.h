#ifndef MIX_BLOOM_FILTER_H
#define MIX_BLOOM_FILTER_H

#include <stdint.h>
#include <unistd.h>

#include "mix_bitmap.h"

#ifdef __cplusplus
extern "C" {
#endif
//支持删除的bloom filter 参考https://github.com/bitly/dablooms

typedef struct {
    uint64_t id;
    uint32_t count;
    uint32_t _pad;
} mix_couting_bloom_header_t;

typedef struct {
    mix_couting_bloom_header_t* header;
    uint32_t capacity;
    long offset;
    uint32_t counts_per_func;
    uint32_t* hashes;
    size_t nfuncs;
    size_t size;
    size_t num_bytes;
    double error_rate;
    mix_bitmap_t* bitmap;
} mix_counting_bloom_filter_t;

mix_counting_bloom_filter_t* mix_counting_bloom_filter_init(uint32_t capacity,
                                                            double error_rate);

int mix_counting_bloom_filter_free(mix_counting_bloom_filter_t* bloom_filter);

int mix_counting_bloom_filter_add(mix_counting_bloom_filter_t* bloom_filter,
                                  uint32_t key);

int mix_counting_bloom_filter_remove(mix_counting_bloom_filter_t* bloom_filter,
                                     uint32_t key);

int mix_counting_bloom_filter_test(mix_counting_bloom_filter_t* bloom_filter,
                                   uint32_t key);

#ifdef __cplusplus
}
#endif

#endif