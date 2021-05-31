#ifndef __SAVEFILES_H_
#define __SAVEFILES_H_

#include <cinttypes>

int save_init();
int save_stop();
int save_files(uint16_t FileNO, uint32_t Length, uint8_t IsStop);

#endif