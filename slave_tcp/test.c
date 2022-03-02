/*************************************************************************
	> File Name: test.c
	> Author: 
	> Mail: 
	> Created Time: Wed 29 Dec 2021 05:08:21 PM CST
 ************************************************************************/

#include<stdio.h>
#include "common.h"

int main() {
    uint64_t x = 0x1020304050607080;
    uint64_t y = 0x1122334455667788;
    uint64_t xx = my_hton64(x);
    printf("%llx\n", xx);
    uint64_t yy = my_ntoh64(y);
    printf("%llx\n", yy);
    return xx;
}
