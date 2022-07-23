
#ifndef __GET_STATUS_H__
#define __GET_STATUS_H__

#define SYS_STATUS_LEN		28

/* RAM 使用情况 */
typedef struct MEM_OCCUPY {
	float totalKB;
	float availableKB;
} MEM_OCCUPY_t;

/* 磁盘使用情况 */
typedef struct DISK_OCCUPY {
	float totalKB;
	float availableKB;
} DISK_OCCUPY_t;

extern int get_sysCpuUsage(float *cpuload);
extern int get_diskUsage(const char *path, DISK_OCCUPY_t *disk_occupy);
extern int get_memocupy(MEM_OCCUPY_t *memst);

#endif