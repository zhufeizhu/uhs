#include "nvm.h"

#include <assert.h>
#include <emmintrin.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libpmem.h>
#include <stdatomic.h>
#include "mix_log.h"

#define BLOCK_SIZE 4096
#define META_SIZE sizeof(buffer_meta_t)


namespace uhs{

NVMDevice::NVMDevice(const string& name, const uint64_t len)
    :Device(name,len)
{
    this->block_size_ = 1<<12;
}


bool NVMDevice::initDevice(){
    void* mmap_addr = NULL;
    size_t mapped_len = 0;
    int is_pmem = 0;
    if ((mmap_addr = pmem_map_file(this->dev_name_.c_str(), this->dev_capacity_, PMEM_FILE_CREATE,
                0666, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        return false;
    }

    if(!is_pmem){
        printf("not pmem\n");
        return false;
    }
    this->nvm_addr_ = mmap_addr;
    this->mapped_len_ = mapped_len;
    // buffer_info->meta_addr = mmap_addr + nvm_info->nvm_capacity;
    // buffer_info->buffer_addr =
    // buffer_info->meta_addr + buffer_info->block_num * sizeof(buffer_meta_t);
    return true;
}

/**
 * read data from NVM at given offset
*/
size_t NVMDevice::read(void* dst, size_t len, size_t offset){
    size_t l = len * this->block_size_;
    size_t off = offset * this->block_size_;
    memcpy(dst, this->nvm_addr_ + off, l);
    return l;
}

/**
 * write data to NVM at given offset
*/
size_t NVMDevice::write(const void* src, size_t len, size_t offset) {
    size_t l = len * this->block_size_;
    size_t off = offset * BLOCK_SIZE;
    pmem_memcpy(this->nvm_addr_ + off,src,l,PMEM_F_MEM_NONTEMPORAL);
    return l;
}

NVMDevice::~NVMDevice(){
    ::pmem_unmap(this->nvm_addr_,this->mapped_len_);
}

}; //namespace uhs

nvm_info_t* nvm_info;
static buffer_info_t* buffer_info;

void mix_nvm_free(nvm_info_t* nvm_info) {
    if (nvm_info == NULL)
        return;
    free(nvm_info);
}

void mix_buffer_free(buffer_info_t* buffer_info) {
    if (buffer_info == NULL)
        return;
    free(buffer_info);
}

const char* nvm = "/mnt/pmem1/myfile";

#define PMEM_LEN (size_t)16*1024*1024*1024
void mix_mmap(nvm_info_t* nvm_info, buffer_info_t* buffer_info) {
    void* mmap_addr = NULL;
    size_t mapped_len = 0;
    int is_pmem = 0;
    if ((mmap_addr = pmem_map_file(nvm, PMEM_LEN, PMEM_FILE_CREATE,
                0666, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }

    if(!is_pmem){
        printf("not pmem\n");
        exit(1);
    }
    nvm_info->nvm_addr = mmap_addr;
    buffer_info->meta_addr = mmap_addr + nvm_info->nvm_capacity;
    buffer_info->buffer_addr =
        buffer_info->meta_addr + buffer_info->block_num * sizeof(buffer_meta_t);
    return;
}

nvm_info_t* mix_nvm_init() {
    nvm_info = malloc(sizeof(nvm_info_t));
    if (nvm_info == NULL) {
        return NULL;
    }

    nvm_info->block_size = BLOCK_SIZE;
    nvm_info->block_num = 3 * 1024 * 1024;
    nvm_info->queue_num = 4;
    nvm_info->per_block_num = nvm_info->block_num / nvm_info->queue_num;
    nvm_info->nvm_capacity = nvm_info->block_num * BLOCK_SIZE;  // nvm 12G
    nvm_info->nvm_addr = NULL;
    return nvm_info;
}

#define SEGMENT_BLOCK_NUM 512
#define SEGMENT_NUM 4
buffer_info_t* mix_buffer_init() {
    buffer_info = malloc(sizeof(buffer_info_t));
    if (buffer_info == NULL) {
        mix_log("mix_buffer_init", "malloc for buffer info failed");
        return NULL;
    }
    buffer_info->segment_num = SEGMENT_NUM;
    buffer_info->block_num = SEGMENT_NUM * SEGMENT_BLOCK_NUM;  //总共分成4块 一块有空间 一共占用8m
    buffer_info->buffer_capacity =
        (size_t)buffer_info->block_num * (BLOCK_SIZE + sizeof(buffer_meta_t));
    buffer_info->meta_addr = NULL;
    buffer_info->buffer_addr = NULL;
    return buffer_info;
}
// inline void pmem_memcpy(void* pmem,void* src,size_t len){
//     memcpy(pmem, src,len);
//     pmem_persist(pmem,len);
// }
atomic_int_fast64_t time = 0;
size_t mix_nvm_read(void* dst, size_t len, size_t offset, size_t flags) {
    // if((++time)%1000 == 0){
    //     printf("time is %ld\n",time);
    // }
    memcpy(dst, nvm_info->nvm_addr + offset * BLOCK_SIZE,len * BLOCK_SIZE);
    return len;
}

size_t mix_nvm_write(void* src, size_t len, size_t offset, size_t flags) {
    size_t l = len * BLOCK_SIZE;
    size_t off = offset * BLOCK_SIZE;
    // if((++time)%2000 == 0){
    //     printf("time is %ld\n",time);
    // }
    pmem_memcpy(nvm_info->nvm_addr + off,src,l,PMEM_F_MEM_NONTEMPORAL);
    return len;
}

size_t mix_buffer_read(void* src, size_t dst_block, size_t flags) {

    memcpy(src, buffer_info->buffer_addr + dst_block * BLOCK_SIZE,
                    BLOCK_SIZE);
    return 1;
}

size_t mix_buffer_write(void* src,
                        size_t src_block,
                        size_t dst_block,
                        size_t flags) {
    // struct timespec start, end;
    // clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    buffer_meta_t meta;
    meta.flags = flags;
    meta.status = 1;
    meta.timestamp = 0;  //暂时不用
    meta.offset = src_block;

    pmem_memcpy(buffer_info->buffer_addr + dst_block * BLOCK_SIZE, src,
                    BLOCK_SIZE,PMEM_F_MEM_NONTEMPORAL);
    pmem_memcpy(buffer_info->meta_addr + META_SIZE * dst_block, &meta,
                    META_SIZE,PMEM_F_MEM_NONTEMPORAL);
    return 1;
}

void mix_buffer_clear(size_t dst_block) {
    size_t status = 0;
    pmem_memcpy(buffer_info->meta_addr + dst_block * META_SIZE, &status,
                    sizeof(size_t),PMEM_F_MEM_NONTEMPORAL);
}

void mix_buffer_get_meta(buffer_meta_t* meta, int dst_block) {
    memcpy(meta, buffer_info->meta_addr + META_SIZE * dst_block,
                    META_SIZE);
}