
#ifndef __COMMON_H__
#define __COMMON_H__

#include <inttypes.h>

#define __DEBUG
#ifdef __DEBUG
#define DEBUG_PTINTF(format, ...) printf("[" __FILE__ "] [line: %d]\t" format, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PTINTF
#endif

uint64_t my_ntoh64(uint64_t __net64);
uint64_t my_hton64(uint64_t __host64);

#endif