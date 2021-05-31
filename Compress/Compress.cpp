#include <stdio.h>
#include <time.h>
#include <string.h>
#include <zlib.h>

#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#include "Logger/Logger.h"

const int MAX_BUFFER_SIZE = 1024*1024*5;
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

    unsigned long total_read = 0;

    clock_t start = clock();

    // 读取文件夹下的所有数据文件
    struct dirent **namelist;
    int num_files = scandir(dir_name, &namelist, NULL, alphasort);
    if (num_files == -1) {
        XLOG_TRACE("{}.", strerror(errno));
        free(namelist);
        return -1;
    }
    
    int index = 0;
    while (index < num_files) {
        // 剔除 . 和 .. 文件夹
        if (strncmp(namelist[index]->d_name, ".", strlen(namelist[index]->d_name)) == 0 ||
            strncmp(namelist[index]->d_name, "..", strlen(namelist[index]->d_name)) == 0) {
                free(namelist[index++]);
                continue;
        }
        
        // 读取每一个文件并压缩进最终文件
        size_t num_read = 0;
        FILE *infile = fopen(namelist[index]->d_name,"rb");
        if(!infile) {
            XLOG_TRACE("{}.\n", strerror(errno));
            free(namelist[index++]);
            continue;
        }
        // printf("file name: %s\n", namelist[index]->d_name);
        XLOG_TRACE("compress file {}.\n", namelist[index]->d_name);

        while ((num_read = fread(DATA_BUFFER, 1, sizeof(DATA_BUFFER), infile)) > 0 ) {
            // printf("read %d bytes\n", num_read);
            total_read += num_read;
            gzwrite(outfile, DATA_BUFFER, num_read);
            memset(DATA_BUFFER, 0, sizeof(DATA_BUFFER));
        }
        fclose(infile);

        free(namelist[index++]);
    }
    free(namelist);

    clock_t end = clock();
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
