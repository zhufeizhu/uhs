#ifndef MIX_LOG_H
#define MIX_LOG_H

#define MIX_DEBUG 1

#ifdef MIX_DEBUG
#include <stdio.h>
#endif

void mix_log(const char* func_name, const char* msg);

#endif