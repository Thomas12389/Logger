
#ifndef _FILE_OPT_H
#define _FILE_OPT_H

#include <string>

struct FileInfo {
    std::string file_name;
    long mod_time;
    long file_size;
};


int delete_file(unsigned long index);
// 绝对路径
int delete_file_by_name(const char * file_name);
// 使用绝对路径, 最后要带上 “/”
int get_all_file_info(const char *dir_path, const char *suffix);

#endif