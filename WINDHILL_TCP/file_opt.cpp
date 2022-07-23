
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include <vector>
#include <algorithm>
#include <iostream>

#include "file_opt.h"
#include "../tfm_common.h"

typedef enum FILE_TYPE : uint8_t {
    LOG_TYPE,
    DATA_TYPE
} FILE_TYPE;

std::vector<FileInfo> vec_file_info;

// 排序函数，降序
bool less_sort(const FileInfo& a, const FileInfo& b) {
    return a.mod_time < b.mod_time;
}

// 判断文件后缀是否为指定格式
int decide_suffix(const char *path, const char *suffix) {
    // 最后一个出现'.'的位置
    char *ptr = strrchr(const_cast<char *>(path), '.');
    if (NULL == ptr) {
        return -1;
    }
    int pos = ptr - path;

    char file_suf[512] = {0};
    strncpy(file_suf, ptr + 1, strlen(path) - (pos + 1));

    if (0 != strcmp(file_suf, suffix)) {
        return -1;
    }
    return 0;
}

// 使用绝对路径
int get_file_info(const char *file_path, struct stat *statbuf) {

    if (-1 == stat(file_path, statbuf)) {
        perror("stat");
        return -1;
    }
    return 0;
}

// 使用绝对路径
int get_all_file_info(const char *dir_path, const char *suffix) {
    vec_file_info.clear();
    struct dirent *dir = NULL;
	DIR *dirp = opendir(dir_path);
	if (NULL == dirp) {
		// perror("opendir");exit(1);
        return -1;
	}

    int total_num = 0;
	while ((dir = readdir(dirp)) != NULL) {
		if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
			continue;
		}

        if (dir->d_type != DT_REG) continue;
		// 后缀符合，解析文件信息
        if (0 == decide_suffix(dir->d_name, suffix)) {
            total_num++;
            char name_file[512] = {0};
            sprintf(name_file, "%s", dir_path);
            // sprintf(name_file + strlen(dir_path), "%s", "/");
            sprintf(name_file + strlen(dir_path), "%s", dir->d_name);
            DEBUG_PTINTF("name_file: %s\n", name_file);
            struct stat file_info;
            if (-1 == get_file_info(name_file, &file_info)) {
                closedir(dirp);
                vec_file_info.clear();
                return -1;
            }
            vec_file_info.push_back({name_file, file_info.st_mtim.tv_sec, file_info.st_size});
        }
	}
    closedir(dirp);
    std::vector<FileInfo>::iterator itr = vec_file_info.begin();
    // printf("before\n");
    // for (; itr != vec_file_info.end(); itr++) {
    //     printf("%s   %ld\n", itr->file_name.c_str(), itr->mod_time);
    // }
    // printf("\n\n");

    std::sort(vec_file_info.begin(), vec_file_info.end(), less_sort);
#ifdef __DEBUG
    printf("after order\n");
    itr = vec_file_info.begin();
    for (; itr != vec_file_info.end(); itr++) {
        printf("%s  %lx   %lx\n", itr->file_name.c_str(), itr->file_size, itr->mod_time);
    }
#endif
    return 0;
}

int delete_file(unsigned long index) {
    unsigned long max_idx = vec_file_info.size();
    // 要删除的文件不存在
    if (index >= max_idx) return -1;

    return remove(vec_file_info[index].file_name.c_str());
}

int delete_file_by_name(const char * file_name) {

    if (0 == access(file_name, F_OK)) {
        return remove(file_name);
    }

    return 0;
}

