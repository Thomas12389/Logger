
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include "../tfm_common.h"
#include "time_opt.h"

#define TIMEZONE_FILE		"/etc/timezone"

int get_timezone(char *pTimezone) {
	if (NULL == pTimezone) {
		DEBUG_PTINTF("pTimezone NULL.\n");
		return 1;
	}
	int fd = open(TIMEZONE_FILE, O_RDONLY);
	if (fd < 0) {
		DEBUG_PTINTF("Timezone -- get ERROR, {%s}.\n", strerror(errno));
		return 2;
	}

	ssize_t read_bytes = read(fd, pTimezone, MAXLEN_TIMEZONE);
	if (read_bytes < 0) {
		DEBUG_PTINTF("Timezone -- get ERROR, {%s}.\n", strerror(errno));
		close(fd);
		return 3;
	}
	// 去掉 Linux 文件行末尾的换行符（'\n'）
	if (pTimezone[read_bytes - 1] == '\n') pTimezone[read_bytes - 1] = '\0';
	close(fd);
	DEBUG_PTINTF("Timezone -- get ok, {%s}.\n", pTimezone);
	
	return 0;
}

int set_timezone(const char *pTimezone, const int len) {
	char *tz_buf = (char *)malloc(sizeof(char) * (len + 1));
	if (NULL == tz_buf) {
		DEBUG_PTINTF("malloc tz_buf ERROR, %s.\n", strerror(errno));
		return 1;
	}

	memcpy(tz_buf, pTimezone, len);
	DEBUG_PTINTF("set timezone: {%s}\n", tz_buf);

	// 保存原时区
	char original_timezone[MAXLEN_TIMEZONE] = {0};
	int ret = get_timezone(original_timezone);
	if (0 != ret) {
		DEBUG_PTINTF("Timezone -- save orignal timezone ERROR.\n");
		free(tz_buf);
		return 3;
	}
	// 要设置的时区与系统时区相同
	if (0 == strcmp(tz_buf, original_timezone)) {
		free(tz_buf);
		return 0;
	}
	// 实际时区文件目录
	char share_tz_file[100] = "/usr/share/zoneinfo/";
	strcat(share_tz_file, tz_buf);
	DEBUG_PTINTF("share_tz_file: {%s}\n", share_tz_file);

	if (0 != access(share_tz_file, F_OK)) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s} is not a valid timezone.\n", tz_buf);
		free(tz_buf);
		return 2;
	}

	int fd = open(TIMEZONE_FILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (fd < 0) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
		free(tz_buf);
		return 4;
	}
	// 写入文件前添加换行符
	tz_buf[strlen(tz_buf)] = '\n';
	ssize_t write_bytes = write(fd, tz_buf, strlen(tz_buf));
	if (write_bytes < 0) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
		close(fd);
		free(tz_buf);
		return 5;
	}
	close(fd);
	
	ret = access("/etc/localtime", F_OK);
	if (0 == ret) {
		ret = remove("/etc/localtime");
		if (-1 == ret) {
			DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
			free(tz_buf);
			return 6;
		}
	}
	ret = symlink(share_tz_file, "/etc/localtime");
	if (-1 == ret) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
		free(tz_buf);
		// 恢复修改前时区
		return 7;
	}
	
	tzset();	// 强制刷新时区
#ifdef __DEBUG
	tz_buf[strlen(tz_buf) - 1] = '\0';	// 方便调试打印
#endif
	DEBUG_PTINTF("Timezone -- set {%s} ok.\n", tz_buf);
	free(tz_buf);
	return 0;
}

int get_systime(struct tm *get_tm) {
	time_t t, ret_t;
	struct tm *ret_tm;
	ret_t = time(&t);
	if ((time_t)-1 == ret_t) {
		DEBUG_PTINTF("Time -- get ERROR, {%s}.\n", strerror(errno));
		return -1;
	}
	tzset();	// 强制刷新时区
	ret_tm = localtime_r(&t, get_tm);
	if (NULL == ret_tm) {
		DEBUG_PTINTF("Time -- get ERROR, {%s}.\n", strerror(errno));
		return -1;
	}
	return 0;
}

int set_systime(struct tm set_tm) {
	time_t set_time = mktime(&set_tm);
	if ((time_t)-1 == set_time) {
		DEBUG_PTINTF("Time -- set ERROR, {%s}.\n", strerror(errno));
		return 1;
	}

	struct timeval set_tv;
	set_tv.tv_sec = set_time;
	set_tv.tv_usec = 0;

	int ret = settimeofday(&set_tv, NULL);
	if (ret == -1) {
		DEBUG_PTINTF("Time -- set ERROR, {%s}.\n", strerror(errno));
		if (errno == EINVAL) {
			return 2;
		} 
		return 3;
	} 
	DEBUG_PTINTF("Time -- set ok, %d-%02d-%02d %02d:%02d:%02d\n", 
			set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday, set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);
	
	return 0;
}
