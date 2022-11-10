#ifndef MIX_NVM_WORKER_H
#define MIx_NVM_WORKER_H

#include "mix_queue.h"
#include "mix_task.h"
#include "nvm.h"

#include <stdatomic.h>

nvm_info_t* mix_nvm_worker_init(unsigned int size, unsigned int esize);

buffer_info_t* mix_buffer_worker_init(unsigned int size, unsigned int esize);

void mix_nvm_mmap(nvm_info_t* nvm_info, buffer_info_t* buffer_info);

int mix_post_task_to_metadata(io_task_t* task);

int mix_post_task_to_data(io_task_t* task);

atomic_int mix_get_completed_nvm_write_block_num();

void mix_rebuild();

#endif  // MIX_NVM_WORKER_H