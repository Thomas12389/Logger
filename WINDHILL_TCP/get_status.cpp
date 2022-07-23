#include "get_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */

/* 与 /proc/meminfo 结构对应 */
typedef struct MEM_PACKED {
	char name[20];				// 条目名称
	unsigned long totalKB;		// 值
	char name2[20];				// 单位
} MEM_PACKED_t;

/* 与 /proc/stat 结构对应 */
typedef struct CPU_OCCUPY {
	char name[20];
	unsigned int user;
	unsigned int nice;
	unsigned int system;
	unsigned int idle;
	unsigned int iowait;
	unsigned int irq;
	unsigned int softirq;
} CPU_OCCUPY_t;

int get_memocupy(MEM_OCCUPY_t *memst) {
	if (NULL == memst) {
		return -1;
	}

	FILE *pFILE = NULL;
	char buff[256];
	MEM_PACKED_t *p_mem_packed = (MEM_PACKED_t *)malloc(sizeof(MEM_PACKED_t));
	MEM_OCCUPY_t *mem_occupy = memst;

	pFILE = fopen("/proc/meminfo", "r");
	if (NULL == pFILE) {
		free(p_mem_packed);
		return -1;
	}

	// 获取 RAM 总大小
	if (NULL == fgets(buff, sizeof(buff), pFILE)) {
		fclose(pFILE);
		free(p_mem_packed);
		return -1;
	}
	sscanf(buff, "%s  %lu  %s\n", p_mem_packed->name, &p_mem_packed->totalKB, p_mem_packed->name2);
	mem_occupy->totalKB = p_mem_packed->totalKB;

	// 获取 RAM 剩余大小
	if (NULL == fgets(buff, sizeof(buff), pFILE)) {
		fclose(pFILE);
		free(p_mem_packed);
		return -1;
	}
	// 获取 RAM 可用大小（应用角度）
	if (NULL == fgets(buff, sizeof(buff), pFILE)) {
		fclose(pFILE);
		free(p_mem_packed);
		return -1;
	}
	sscanf(buff, "%s  %lu  %s\n", p_mem_packed->name, &p_mem_packed->totalKB, p_mem_packed->name2);
	mem_occupy->availableKB = p_mem_packed->totalKB;

	fclose(pFILE);
	pFILE = NULL;
	free(p_mem_packed);
	return 0;
}

int get_diskUsage(const char *path, DISK_OCCUPY_t *disk_occupy) {
	if(-1 == access(path, F_OK)) {
		return -1;
	}
	if (NULL == disk_occupy) {
		return -1;
	}
	struct statfs disk_info;
	if (-1 == statfs(path, &disk_info)) {
		return -1;
	}

	__uint64_t block_size = disk_info.f_bsize;

	disk_occupy->totalKB = disk_info.f_blocks * block_size / 1024.0;
	disk_occupy->availableKB = disk_info.f_bavail * block_size / 1024.0;
	return 0;
}

float cal_cpuoccupy(CPU_OCCUPY_t *o, CPU_OCCUPY_t *n) {
	if ( (NULL == o) || (NULL == n) || (o == n) ) {
		return -1.0;
	}
	unsigned int od, nd;
	unsigned int id, sd;
	float cpu_use = 0;

	od = (unsigned long)(o->user + o->nice + o->system + o->idle);	// 第一次（用户 + 优先级 + 系统 + 空闲）的时间
	nd = (unsigned long)(n->user + n->nice + n->system + n->idle);	// 第二次（用户 + 优先级 + 系统 + 空闲）的时间

	id = (unsigned long)(n->user - o->user);		// 用户第一次和第二次的时间之差
	sd = (unsigned long)(n->system - o->system);	// 系统第一次和第二次的时间之差

	if (od != nd) {
		cpu_use = (float)((sd + id) * 100.0) / (nd - od);
	}

	return cpu_use;
}

int get_cpuoccupy(CPU_OCCUPY_t *cpust) {
	FILE *pFILE = NULL;
	char buff[256];
	CPU_OCCUPY_t *cpu_occupy = cpust;

	pFILE = fopen("/proc/stat", "r");
	if (NULL == pFILE) {
		return -1;
	}

	if (NULL == fgets(buff, sizeof(buff), pFILE)) {
		fclose(pFILE);
		return -1;
	}

	sscanf(buff, "%s %u %u %u %u %u %u %u", cpu_occupy->name, &cpu_occupy->user, &cpu_occupy->nice,
		&cpu_occupy->system, &cpu_occupy->idle, &cpu_occupy->iowait, &cpu_occupy->irq, &cpu_occupy->softirq);
	fclose(pFILE);
	pFILE = NULL;

	return 0;
}

int get_sysCpuUsage(float *cpuload) {
	CPU_OCCUPY_t cpu_stat1, cpu_stat2;

	if (-1 == get_cpuoccupy(&cpu_stat1)) return -1;
	// sleep(1);
	usleep(500000);
	if (-1 == get_cpuoccupy(&cpu_stat2)) return -1;
	// 计算 CPU 使用率
	*cpuload = cal_cpuoccupy(&cpu_stat1, &cpu_stat2);
	
	return 0;
}
