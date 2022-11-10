#include "mix_log.h"

inline void mix_log(const char* func_name, const char* msg) {
#ifdef MIX_DEBUG
    printf("[%s]:%s\n", func_name, msg);
#endif
    return;
}