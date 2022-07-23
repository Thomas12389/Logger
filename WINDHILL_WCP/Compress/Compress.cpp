#include <stdio.h>
#include <time.h>
#include <string.h>
#include <zlib.h>

#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#include "tbox/Common.h"
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

    if (0 != access(dir_name, F_OK)) {
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
    gzFile gz_outfile = gzopen(outfilename, "wb");
    
    chdir(dir_name);

    unsigned long total_read = 0;

#ifdef ONLY_CPU_TIME
    clock_t start = clock();
#else
    uint64_t start = getTimestamp(TIME_STAMP::US_STAMP);
#endif

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
            XLOG_ERROR("Open date file {} ERROR, {}. data in it will be discarded.", namelist[index]->d_name, strerror(errno));
            free(namelist[index++]);
            continue;
        }
        // printf("file name: %s\n", namelist[index]->d_name);
        XLOG_TRACE("compress file {}.", namelist[index]->d_name);

        while ((num_read = fread(DATA_BUFFER, 1, sizeof(DATA_BUFFER), infile)) > 0 ) {
            // printf("read %d bytes\n", num_read);
            if (num_read < sizeof(DATA_BUFFER)) {
                if (ferror(infile)) {
                    XLOG_ERROR("Read data file {} ERROR, data in it will be discarded.", namelist[index]->d_name);
                    clearerr(infile);
                    continue;
                }
            }
            gzseek(gz_outfile, 0, SEEK_END);
            off_t cur_pos = gztell(gz_outfile);
            int ret = gzwrite(gz_outfile, DATA_BUFFER, num_read);
            if (ret <= 0) {
                int gz_errno = Z_OK;
                XLOG_ERROR("Compress data file {} ERROR, {}. data in it will be discarded.", namelist[index]->d_name, gzerror(gz_outfile, &gz_errno));
                // 指针恢复到出错前
                gzseek(gz_outfile, cur_pos, SEEK_SET);
                continue;
            }
            total_read += num_read;
            // memset(DATA_BUFFER, 0, sizeof(DATA_BUFFER));
        }

        fclose(infile);

        free(namelist[index++]);
    }
    free(namelist);

#ifdef ONLY_CPU_TIME
    clock_t end = clock();
#else
    uint64_t end = getTimestamp(TIME_STAMP::US_STAMP);
#endif
    
    gzclose(gz_outfile);

    // 计算压缩后的文件大小
    chdir(gz_file_path);
    long gz_file_size = fileSize(outfilename);

#ifdef ONLY_CPU_TIME
    XLOG_DEBUG("The time taken to compress all data files: {:.3f} ms.",1000.0 * ( end - start) / CLOCKS_PER_SEC);
#else
    XLOG_DEBUG("The time taken to compress all data files: {:.3f} ms.", 1.0 * (end - start) / 1000 );
#endif

    if (total_read) {
        XLOG_DEBUG("Compression factor: {:.2f}%", (1.0 - gz_file_size * 1.0 / total_read) * 100.0);
    } else {
        XLOG_DEBUG("Nothing to compress.");
    }

    chdir(old_path);
    return 0;
}
