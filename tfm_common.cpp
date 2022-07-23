
#include "tfm_common.h"
#include <endian.h>
#include <arpa/inet.h>
#include <signal.h>

typedef union FLOAT_CONV {
    float f;
    char c[4];
} FLOAT_CONV;


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

float my_ntohf(float __netf) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    FLOAT_CONV fc1, fc2;
    fc1.f = __netf;
    fc2.c[0] = fc1.c[3];
    fc2.c[1] = fc1.c[2];
    fc2.c[2] = fc1.c[1];
    fc2.c[3] = fc1.c[0];

    return fc2.f;
#else
    return __netf;
#endif
}

float my_htonf(float __hostf) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    FLOAT_CONV fc1, fc2;
    fc1.f = __hostf;
    fc2.c[0] = fc1.c[3];
    fc2.c[1] = fc1.c[2];
    fc2.c[2] = fc1.c[1];
    fc2.c[3] = fc1.c[0];

    return fc2.f;
#else
    return __hostf;
#endif
}

int get_process_status(const char* proname) {

	FILE *pFILE = NULL;
	char command[256] = {0};
	char buff[128] = {0};
	int original_count = 0;

	snprintf(command, sizeof(command), "ps -ef | grep -v \"grep\" | grep -wc %s ", proname); 

	pFILE = popen(command, "r");
	if (NULL == pFILE) {
		return -1;
	}

	if (NULL == fgets(buff, sizeof(buff), pFILE)) {
		fclose(pFILE);
		return -1;
	}

	int count = atoi(buff);
    DEBUG_PTINTF("count = %d\n", count);
	pclose(pFILE);
	pFILE = NULL;

	if (count == original_count) {
		return 0;
	}

	return 1;
}

int my_system(const char *cmd) {

    int ret = 0;
    // 设置 SIGCHLD 为默认处理方式，并保存原本的处理方式
    __sighandler_t old_handler = signal(SIGCHLD, SIG_DFL);
    ret = system(cmd);
    // 恢复 SIGCHLD 的处理方式
    signal(SIGCHLD, old_handler);

    return ret;
}