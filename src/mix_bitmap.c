#include "mix_bitmap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mix_log.h"

#define BITS_PER_BYTE 8

mix_bitmap_t* mix_bitmap_init(int bytes) {
    mix_bitmap_t* bitmap = malloc(sizeof(mix_bitmap_t));
    if (bitmap == NULL) {
        mix_log("mix_bitmap_init", "malloc for bitmap failed");
        return NULL;
    }

    bitmap->bytes = bytes/BITS_PER_BYTE;
    bitmap->next_bit = 0;
    bitmap->nvm_offset = 0;
    bitmap->array = malloc(bytes * sizeof(char));
    if (bitmap->array == NULL) {
        mix_log("mix_bitmap_init", "malloc for bitmap->array failed");
        free(bitmap);
        return NULL;
    }
    memset(bitmap->array, 0, bytes * sizeof(char));
    bitmap->rwlock = malloc(sizeof(pthread_rwlock_t));
    if (bitmap->rwlock == NULL) {
        mix_log("mix_bitmap_init", "malloc for rwlock failed");
        free(bitmap->array);
        free(bitmap);
        return NULL;
    }
    pthread_rwlock_init(bitmap->rwlock, NULL);
    return bitmap;
}

int mix_bitmap_free(mix_bitmap_t* bitmap) {
    if (bitmap == NULL)
        return 0;

    free(bitmap->array);
    free(bitmap);
    bitmap = NULL;
    return 0;
}

/**
 * @brief 获取bitmap的下一个zero bit
 *
 * @param bitmap
 * @return size_t bitmap的bit位
 */
int mix_bitmap_next_zero_bit(mix_bitmap_t* bitmap) {
    int next_zero_bit = bitmap->next_bit;
    if (!mix_bitmap_test_bit(bitmap,next_zero_bit)) {
        if (!mix_bitmap_set_bit(bitmap, next_zero_bit)) {
            bitmap->next_bit =
                (next_zero_bit + 1) % (bitmap->bytes * BITS_PER_BYTE);
            return next_zero_bit;
        } else {
            mix_log("mix_bitmap_next_zero_bit", "set bit failed");
            return -1;
        }
    } else {
        while (mix_bitmap_test_bit(bitmap, next_zero_bit)) {
            next_zero_bit = (next_zero_bit + 1) % (bitmap->bytes * BITS_PER_BYTE);
        }
        if (!mix_bitmap_set_bit(bitmap,next_zero_bit)) {
            bitmap->next_bit =
                (next_zero_bit + 1) / (bitmap->bytes * BITS_PER_BYTE);
            return next_zero_bit;
        } else {
            mix_log("mix_bitmap_next_zero_bit", "set bit failed");
            return -1;
        }
    }
}

int mix_bitmap_set_bit(mix_bitmap_t* bitmap,int nr) {
    char mask, retval;
    char* addr = bitmap->array;

    addr += nr >> 3;          //得char的index
    mask = 1 << (nr & 0x07);  //得char内的offset
    retval = (mask & *addr) != 0;
    *addr |= mask;
    return (int)retval;  //返回置数值
}

void mix_bitmap_clear(mix_bitmap_t* bitmap){
    memset(bitmap->array,0,bitmap->bytes);
    bitmap->next_bit = 0;
}

int mix_bitmap_clear_bit(mix_bitmap_t* bitmap, int nr) {
    char mask, retval;
    char* addr = bitmap->array;
    addr += nr >> 3;
    mask = 1 << (nr & 0x07);
    retval = (mask & *addr) != 0;
    *addr &= ~mask;
    return (int)retval;
}

/**
 * @brief nr指定的bit位的值
 *
 * @param nr 要test的bit位
 * @param bitmap 要test的bitmap
 * @return int bit位的值
 */
int mix_bitmap_test_bit(mix_bitmap_t* bitmap, int nr) {
    char mask;
    char* addr = bitmap->array;
    addr += nr >> 3;
    mask = 1 << (nr & 0x07);
    int retval = (int)((mask & *addr) != 0);
    return retval;
}

/**
 * @brief 支持计数的bitmap 将每个byte分割成了2个"bit"位 每个"bit"占用了3位
 * 即支持最大的计数 大小为8
 * @param btimap
 * @param index
 * @param offset
 * @return int
 */
int mix_counting_bitmap_increment(mix_bitmap_t* bitmap,
                                  unsigned int index,
                                  long offset) {
    long access = index / 2 + offset;
    uint8_t temp;
    uint8_t n = bitmap->array[access];

    if (index % 2 != 0) {
        temp = (n & 0x0f);
        n = (n & 0xf0) + ((n & 0x0f) + 0x01);
    } else {
        temp = (n & 0xf0) >> 4;
        n = (n & 0x0f) + ((n & 0xf0) + 0x10);
    }

    if (temp == 0x0f) {
        mix_log("mix_counting_bitmap_increment", "4 bit int overflow");
        return -1;
    }

    bitmap->array[access] = n;
    return 0;
}

int mix_counting_bitmap_decrement(mix_bitmap_t* bitmap,
                                  unsigned int index,
                                  long offset) {
    long access = index / 2 + offset;
    uint8_t temp;
    uint8_t n = bitmap->array[access];

    if (index % 2 != 0) {
        temp = (n & 0x0f);
        n = (n & 0xf0) + ((n & 0x0f) - 0x01);
    } else {
        temp = (n & 0xf0) >> 4;
        n = (n & 0x0f) + ((n & 0xf0) - 0x10);
    }

    if (temp == 0x00) {
        mix_log("mix_counting_bitmap_decrement", "decrementing zero");
        return -1;
    }

    bitmap->array[access] = n;
    return 0;
}

int mix_counting_bitmap_check(mix_bitmap_t* bitmap,
                              unsigned int index,
                              long offset) {
    long access = index / 2 + offset;
    int retval = 0;
    if (index % 2 != 0) {
        retval = bitmap->array[access] & 0x0f;
    } else {
        retval = bitmap->array[access] & 0xf0;
    }
    return retval;
}