#include <stdio.h>
#include <time.h>
#include <string.h>
#include <zlib.h>

#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#include "Logger/Logger.h"

const int MAX_BUFFER_SIZE = 1024*1024*4;
unsigned char DATA_BUFFER[MAX_BUFFER_SIZE];

static long fileSize(const char *filename) {
    FILE *pFile = fopen(filename, "rb");
    fseek(pFile, 0, SEEK_END);
    long size = ftell(pFile);
    fclose(pFile);
    return size;
}

int compress_gz(const char *dir_name, char *outfilename) {
    if (NULL == dir_name) {
        XLOG_TRACE("dir can't NULL.");
        return -1;
    }

    // 检查文件夹是否合法
    struct stat info_dir;
    lstat(dir_name, &info_dir);
    if (!S_ISDIR(info_dir.st_mode)) {
        XLOG_TRACE("{} is not a valid directory.", dir_name);
        return -1;
    }

    DIR *dir = opendir(dir_name);
    if (NULL == dir) {
        XLOG_TRACE("can't open directory {}", dir_name);
        return -1;
    }

    char old_path[256] = {0};
    getcwd(old_path, sizeof(old_path));

    // 创建压缩文件
    char gz_path_name[256] = {0};
    strcpy(gz_path_name, dir_name);
    const char *gz_file_path = dirname(gz_path_name);
    // printf("parent_name: %s\n", parent_name);
    chdir(gz_file_path);
    gzFile outfile = gzopen(outfilename, "wb");
    
    chdir(dir_name);

    struct dirent *fileinfo = NULL;
    unsigned long total_read = 0;

    clock_t start = clock();
    while ((fileinfo = readdir(dir)) != NULL) {
        // 剔除 . 和 .. 文件夹
        if (strncmp(fileinfo->d_name, ".", strlen(fileinfo->d_name)) == 0 ||
            strncmp(fileinfo->d_name, "..", strlen(fileinfo->d_name)) == 0) {
                continue;
        }

        // 读取每一个文件并压缩进最终文件
        size_t num_read = 0;
        FILE *infile = fopen(fileinfo->d_name,"rb");
        if(!infile) {
            XLOG_TRACE("file {} loss!\n", fileinfo->d_name);
            fclose(infile);
            continue;
        }

        while ((num_read = fread(DATA_BUFFER, 1, sizeof(DATA_BUFFER), infile)) > 0 ) {
            // printf("read %d bytes\n", num_read);
            total_read += num_read;
            gzwrite(outfile, DATA_BUFFER, num_read);
            memset(DATA_BUFFER, 0, sizeof(DATA_BUFFER));
        }
        fclose(infile);
    }

    clock_t end = clock();
    closedir(dir);
    gzclose(outfile);

    // 计算压缩后的文件大小
    chdir(gz_file_path);
    long gz_file_size = fileSize(outfilename);
    
    XLOG_DEBUG("The time taken to compress(ms):{:.3f}.",1000.0 * ( end - start) / CLOCKS_PER_SEC);
    if (total_read) {
        XLOG_DEBUG("Compression factor: {:.2f}%", (1.0 - gz_file_size * 1.0 / total_read) * 100.0);
    } else {
        XLOG_DEBUG("Nothing to compress.");
    }

    chdir(old_path);
    return 0;
}
