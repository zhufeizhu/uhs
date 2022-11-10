#include "mix_logging.h"

#include <stdlib.h>
#include <string.h>
#include "mix_task.h"

static inline int fls(int x) {
    int r;

    __asm__(
        "bsrl %1,%0\n\t"
        "jnz 1f\n\t"
        "movl $-1,%0\n"
        "1:"
        : "=r"(r)
        : "rm"(x));
    return r + 1;
}

static inline unsigned int roundup_pow_of_two(unsigned int x) {
    return 1UL << fls(x - 1);
}

mix_log_t* mix_log_init(unsigned int entry_num){
    if(entry_num <= 0 || entry_num != roundup_pow_of_two(entry_num)){
        return NULL;
    }

    mix_log_t* log = malloc(sizeof(mix_log_t));
    if(log == NULL){
        return NULL;
    }
    log->head = 0;
    log->tail = 0;
    log->capacity = entry_num;
    log->mask = log->capacity - 1;
    log->entry_size = sizeof(mix_log_entry_t);
    log->begin = malloc(entry_num * log->entry_size);
    if(log->begin == NULL){
        free(log);
        return NULL;
    }

    return log;
}

void mix_log_free(mix_log_t* log){
    if(log == NULL){
        return;
    }
    free(log->begin);
    free(log);
}

static inline unsigned int mix_log_unused(mix_log_t* log){
    return (log->mask+1) - (log->head - log->tail);
}

int mix_log_append(mix_log_t* log, io_task_t* task){
    
    unsigned int l = mix_log_unused(log);
    if(l == 0){
        return 0;
    }
}

int mix_log_release(mix_log_t* log){
    log->tail--;
}