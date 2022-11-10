#include "mix_bloom_filter.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mix_log.h"

#define SALT_CONSTANT 0x97c29b3a
#define ERROR_TIGHTENING_RATIO 0.5

#define FORCE_INLINE inline static

FORCE_INLINE uint64_t rotl64(uint64_t x, int8_t r) {
    return (x << r) | (x >> (64 - r));
}

#define ROTL64(x, y) rotl64(x, y)

#define BIG_CONSTANT(x) (x##LLU)

#define getblock(x, i) (x[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

FORCE_INLINE uint64_t fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return k;
}

//-----------------------------------------------------------------------------

void MurmurHash3_x64_128(const void* key,
                         const int len,
                         const uint32_t seed,
                         void* out) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 16;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
    uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

    int i;

    //----------
    // body

    const uint64_t* blocks = (const uint64_t*)(data);

    for (i = 0; i < nblocks; i++) {
        uint64_t k1 = getblock(blocks, i * 2 + 0);
        uint64_t k2 = getblock(blocks, i * 2 + 1);

        k1 *= c1;
        k1 = ROTL64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = ROTL64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = ROTL64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = ROTL64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    //----------
    // tail

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (len & 15) {
        case 15:
            k2 ^= ((uint64_t)tail[14]) << 48;
        case 14:
            k2 ^= ((uint64_t)tail[13]) << 40;
        case 13:
            k2 ^= ((uint64_t)tail[12]) << 32;
        case 12:
            k2 ^= ((uint64_t)tail[11]) << 24;
        case 11:
            k2 ^= ((uint64_t)tail[10]) << 16;
        case 10:
            k2 ^= ((uint64_t)tail[9]) << 8;
        case 9:
            k2 ^= ((uint64_t)tail[8]) << 0;
            k2 *= c2;
            k2 = ROTL64(k2, 33);
            k2 *= c1;
            h2 ^= k2;

        case 8:
            k1 ^= ((uint64_t)tail[7]) << 56;
        case 7:
            k1 ^= ((uint64_t)tail[6]) << 48;
        case 6:
            k1 ^= ((uint64_t)tail[5]) << 40;
        case 5:
            k1 ^= ((uint64_t)tail[4]) << 32;
        case 4:
            k1 ^= ((uint64_t)tail[3]) << 24;
        case 3:
            k1 ^= ((uint64_t)tail[2]) << 16;
        case 2:
            k1 ^= ((uint64_t)tail[1]) << 8;
        case 1:
            k1 ^= ((uint64_t)tail[0]) << 0;
            k1 *= c1;
            k1 = ROTL64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
    }

    //----------
    // finalization

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    ((uint64_t*)out)[0] = h1;
    ((uint64_t*)out)[1] = h2;
}

static void hash_func(mix_counting_bloom_filter_t* bloom_filter,
                      int key,
                      __uint32_t* hashes) {
    int i;
    __uint32_t checksum[4];
    char keystring[20];
    memset(keystring, '0', 20);
    sprintf(keystring, "%d", key);

    MurmurHash3_x64_128(keystring, strlen(keystring), SALT_CONSTANT, checksum);
    __uint32_t h1 = checksum[0];
    __uint32_t h2 = checksum[1];

    for (int i = 0; i < bloom_filter->nfuncs; i++) {
        hashes[i] = (h1 + i * h2) % bloom_filter->counts_per_func;
    }
}

mix_counting_bloom_filter_t* mix_counting_bloom_filter_init(
    unsigned int capacity,
    double error_rate) {
    mix_counting_bloom_filter_t* bloom_filter = NULL;
    bloom_filter = malloc(sizeof(mix_counting_bloom_filter_t));

    if (bloom_filter == NULL) {
        mix_log("mix_init_counting_bloom_filter",
                "malloc for bloom filter failed");
        return NULL;
    }
    bloom_filter->capacity = capacity;
    bloom_filter->error_rate = error_rate;
    bloom_filter->offset = sizeof(mix_couting_bloom_header_t);
    bloom_filter->nfuncs = (int)ceil(log(1 / error_rate) / log(2));
    bloom_filter->counts_per_func =
        (int)ceil(capacity * fabs(log(error_rate)) /
                  (bloom_filter->nfuncs * pow(log(2), 2)));
    bloom_filter->size = bloom_filter->nfuncs * bloom_filter->counts_per_func;
    /* rounding-up integer divide by 2 of bloom->size */
    bloom_filter->num_bytes =
        ((bloom_filter->size + 1) / 2) + sizeof(mix_couting_bloom_header_t);
    bloom_filter->hashes = calloc(bloom_filter->nfuncs, sizeof(uint32_t));
    bloom_filter->bitmap = mix_bitmap_init(bloom_filter->num_bytes);
    bloom_filter->header =
        (mix_couting_bloom_header_t*)(bloom_filter->bitmap->array);
    return bloom_filter;
}

int mix_counting_bloom_filter_free(mix_counting_bloom_filter_t* bloom_filter) {
    if (bloom_filter == NULL)
        return 0;

    free(bloom_filter->hashes);
    bloom_filter->hashes = NULL;
    free(bloom_filter->bitmap);
    free(bloom_filter);
    bloom_filter = NULL;
    return 0;
}

int mix_counting_bloom_filter_add(mix_counting_bloom_filter_t* bloom_filter,
                                  uint32_t key) {
    uint32_t index, i, offset;
    uint32_t* hashes = bloom_filter->hashes;

    hash_func(bloom_filter, key, hashes);

    for (i = 0; i < bloom_filter->nfuncs; i++) {
        offset = i * bloom_filter->counts_per_func;
        index = hashes[i] + offset;
        mix_counting_bitmap_increment(bloom_filter->bitmap, index,
                                      bloom_filter->offset);
    }

    bloom_filter->header->count++;
    return 0;
}

int mix_counting_bloom_filter_remove(mix_counting_bloom_filter_t* bloom_filter,
                                     uint32_t key) {
    uint32_t index, i, offset;
    uint32_t* hashes = bloom_filter->hashes;

    hash_func(bloom_filter, key, hashes);

    for (i = 0; i < bloom_filter->nfuncs; i++) {
        offset = i * bloom_filter->counts_per_func;
        index = hashes[i] + offset;
        mix_counting_bitmap_decrement(bloom_filter->bitmap, index,
                                      bloom_filter->offset);
    }
    bloom_filter->header->count--;

    return 0;
}

/**
 * @brief 判断s是否在bloom_filter中 如果不在返回0 如果可能在则返回1
 *
 * @param bloom_filter
 * @param s 要检查的s
 * @param len s的长度
 * @return int 0:表示一定不在 1:表示可能在
 */
int mix_counting_bloom_filter_test(mix_counting_bloom_filter_t* bloom_filter,
                                   uint32_t key) {
    uint32_t index, i, offset;
    uint32_t* hashes = bloom_filter->hashes;

    hash_func(bloom_filter, key, hashes);

    for (i = 0; i < bloom_filter->nfuncs; i++) {
        offset = i * bloom_filter->counts_per_func;
        index = hashes[i] + offset;
        if (!mix_counting_bitmap_check(bloom_filter->bitmap, index,
                                       bloom_filter->offset)) {
            return 0;
        }
    }

    return 1;
}