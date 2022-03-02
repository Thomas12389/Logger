
#include "common.h"
#include <endian.h>
#include <arpa/inet.h>

uint64_t my_ntoh64(uint64_t __net64) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    // uint64_t ret = 0;
    // uint32_t high, low;

    // low = __net64 & 0xFFFFFFFF;
    // high = (__net64 >> 32) & 0xFFFFFFFF;

    // low = ntohl(low);
    // high = ntohl(high);

    // ret = low;
    // ret <<= 32;
    // ret |= high;
    // return ret;
    return (((uint64_t)ntohl((uint32_t)((__net64 << 32) >> 32))) << 32) | (uint32_t)ntohl((uint32_t)(__net64 >> 32));
#else
    return __net64;
#endif
}

uint64_t my_hton64(uint64_t __host64) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    // uint64_t ret = 0;
    // uint32_t high, low;

    // low = __host64 & 0xFFFFFFFF;
    // high = (__host64 >> 32) & 0xFFFFFFFF;

    // low = htonl(low);
    // high = htonl(high);

    // ret = low;
    // ret <<= 32;
    // ret |= high;
    // return ret;
    return (((uint64_t)htonl((uint32_t)((__host64 << 32) >> 32))) << 32) | (uint32_t)htonl((uint32_t)(__host64 >> 32));
#else
    return __host64;
#endif
}
