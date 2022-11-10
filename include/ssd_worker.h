#ifndef MIX_SSD_WORKER_H
#define MIX_SSD_WORKER_H

#include "mix_queue.h"
#include "mix_task.h"
#include "ssd.h"

#include <stdatomic.h>

ssd_info_t* mix_ssd_worker_init(unsigned int, unsigned int);

int mix_post_task_to_ssd(io_task_t*);

atomic_int mix_get_completed_ssd_write_block_num();

atomic_bool mix_ssd_queue_is_empty(int idx);

#endif  // MIX_SSD_QUEUE_H