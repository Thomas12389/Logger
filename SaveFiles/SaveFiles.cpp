
#include <cstring>
#include <cstdio>
#include <cfloat>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "tbox/Common.h"

#include "SaveFiles/SaveFiles.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"

#include "Logger/Logger.h"

// 时间测试
// #define CHECH_TIME
// 使用定时器
#define SAVE_TIMER
#define C_STYLE

extern stu_SaveMessage g_stu_SaveMessage;
extern File_Save g_File_Info;

// 文件目录
std::string STORGE_DIR_NAME = "/media/TFM_DATA/";

// 缓冲区大小
static const uint32_t MSG_BUFFER_SIZE = 1024 * 1024 * 10;
// 缓冲区已写入的长度
static uint32_t BUFFER_LENGTH = 0;

#ifdef C_STYLE
// 缓冲区
static char MSG_BUFFER[MSG_BUFFER_SIZE + 5];
static char *buffer_ptr = MSG_BUFFER;
// 临时缓冲区，用于写入文件
static char MSG_BUFFER_TEMP[MSG_BUFFER_SIZE + 5];
static char *buffer_temp_ptr = MSG_BUFFER_TEMP;

#else
// 缓冲区
static std::vector<char> MSG_BUFFER(MSG_BUFFER_SIZE + 5, 0x00);
// 临时缓冲区，用于写入文件
static std::vector<char> MSG_BUFFER_TEMP(MSG_BUFFER_SIZE + 5, 0x00);
#endif

// 保存信号线程运行标志
static int runSaveThread = 0;
// 保存文件线程运行标志
static int runSaveFileThread = 0;

// 保存文件的编号
static const uint16_t MAX_FILE_NO = 2000;
static uint16_t nFileNO = 0x0001;

#ifdef SAVE_TIMER
// 信息保存 信号量
static sem_t gstc_save;
static unsigned char save_timer_id = TIMER_ID::SAVE_TIMER_ID;
#endif

int save_files(uint16_t FileNO, uint32_t Length) {
    // printf("write length = %u\n", Length);

#ifdef ONLY_CPU_TIME
    clock_t start = clock();
#else
    uint64_t start = getTimestamp(TIME_STAMP::US_STAMP);
#endif

    std::string ABSOULT_PATH_DIR_NAME = STORGE_DIR_NAME + g_File_Info.strLocalDirName;
    char old_path[256] = {0};
    getcwd(old_path, sizeof(old_path));
    DIR *pDir = opendir(ABSOULT_PATH_DIR_NAME.c_str());
    if (NULL == pDir) {
        XLOG_DEBUG("Directory {} doesn'e exist.", ABSOULT_PATH_DIR_NAME.c_str());
        int ret = 0;
        if ( (ret = mkdir(ABSOULT_PATH_DIR_NAME.c_str(), 0775)) == 0) {
            XLOG_INFO("Make directory {} to save data.", ABSOULT_PATH_DIR_NAME.c_str());
        } else {
            XLOG_ERROR("mkdir: {}.", strerror(errno));
            return -1;
        }
    } else {
        closedir(pDir);
    }
    
    chdir(ABSOULT_PATH_DIR_NAME.c_str());

    char file_name[256] = {0};
    // 限制文件个数
    if (FileNO > MAX_FILE_NO) {
        XLOG_ERROR("Capacity limit reached, stop save data.");
        FileNO = 0;
    }
    // 停止时保存
    if (FileNO == 0) {
        FileNO = nFileNO++;
        char *swap_ptr = buffer_ptr;
        buffer_ptr = buffer_temp_ptr;
        buffer_temp_ptr = swap_ptr;
    }

    if (Length == 0) {
        Length = BUFFER_LENGTH;
    }

    sprintf(file_name, "%04d", FileNO);

#ifdef C_STYLE
    FILE *pF = fopen(file_name, "wb+");
    if (NULL == pF) {
        XLOG_DEBUG("fopen: {}", strerror(errno));
        return -1;
    }
    
    // 不要使用 strlen， 否则遇到 "00" 会发生截断，导致数据丢失
    // 也不要使用 sizeof， 无法保证 恰好 写满了缓冲区
    size_t total_write_blocks = fwrite(buffer_temp_ptr, 1, Length, pF);
    if (total_write_blocks < Length) {
        XLOG_ERROR("Save data to file: {}", strerror(errno));
        fclose(pF);
        chdir(old_path);
        return -1;
    }
    fclose(pF);
#else
    std::ofstream pF(file_name, std::ios::out | std::ios::binary);
    if (!pF) {
        XLOG_ERROR("std::ofstream open: {}", strerror(errno));
        return -1;
    }

    pF.write((const char*)&MSG_BUFFER_TEMP[0], Length);
    pF.close();
#endif

#ifdef ONLY_CPU_TIME
    clock_t end = clock();
    XLOG_INFO("The time taken to save data file {}: {:.3f} ms.", file_name, 1000.0 * ( end - start) / CLOCKS_PER_SEC);
#else
    uint64_t end = getTimestamp(TIME_STAMP::US_STAMP);
    XLOG_INFO("The time taken to save data file {}: {:.3f} ms.", file_name, 1.0 * (end - start) / 1000 );
#endif
    

    chdir(old_path);
    return 0;
}

int save_last_file() {
    while (runSaveFileThread) {
        // 100 ms
        usleep(100000);
        XLOG_WARN("-- save last file, but pre save thread is still running, waitting...");
    }
    return save_files(0, 0);
}

#ifdef C_STYLE
void *runSaveFile(void *arg) {
    uint32_t length = *(uint32_t *)arg;
#else
void runSaveFile(uint32_t length) {
#endif

    runSaveFileThread = 1;
    
    save_files(nFileNO++, length);

    runSaveFileThread = 0;
    pthread_exit(NULL);
}

void *save_msg_thread(void *arg) {
    int singals_count = g_stu_SaveMessage.msg_struct.msg_list.size();
    // 总大小 = 8 *（信号个数 + 时间戳）
    char temp_signals[8 * (1 + singals_count)];

    while(runSaveThread) {
#ifdef SAVE_TIMER
        sem_wait(&gstc_save);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(g_File_Info.nSaveCycleMs));
        if (!runSaveThread) {
            break;
        }
#endif
        // 时间戳
        uint64_t time_stamp = getTimestamp(TIME_STAMP::MS_STAMP);
        memcpy(temp_signals, &time_stamp, sizeof(time_stamp));

        {
            std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
            LIST_Message::iterator ItrMsg = g_stu_SaveMessage.msg_struct.msg_list.begin();

            int signal_index = 0;
            while (ItrMsg != g_stu_SaveMessage.msg_struct.msg_list.end() && (signal_index < singals_count)) {
                
                double dval = (*ItrMsg).dPhyVal;
                // for debug
                // printf("save_name: %s, dval = %lf\n", (*ItrMsg).strName, dval);
                memcpy(temp_signals + sizeof(time_stamp) + sizeof(dval) * signal_index++, &dval, sizeof(dval));

                ItrMsg++;
            }
            if (signal_index < singals_count) {
                XLOG_ERROR("signal count ERROR");
            }
        }

        // 缓冲区写满，创建线程保存到文件
        if (BUFFER_LENGTH + sizeof(temp_signals) > MSG_BUFFER_SIZE) {
            while (runSaveFileThread) {
                usleep(1000);
                XLOG_WARN("-- buffer flow, but pre save is still running...");
            }
            // 保存已写入缓冲区的长度
            uint32_t write_length = BUFFER_LENGTH;
            BUFFER_LENGTH = 0;

#ifdef CHECH_TIME
            clock_t start = clock();
#endif

#ifdef C_STYLE
            
            char *swap_ptr = buffer_ptr;
            buffer_ptr = buffer_temp_ptr;
            buffer_temp_ptr = swap_ptr;

            pthread_t save_file_id_t;
            pthread_create(&save_file_id_t, NULL, runSaveFile, (void *)&write_length);
            pthread_detach(save_file_id_t);

    #if CHECH_TIME
            clock_t end1 = clock();
    #endif
            // 不需要清零，后续数据直接覆盖即可（10 M 空间清零需要 ms 级时间）
            // memset(buffer_ptr, 0, sizeof(MSG_BUFFER));
#else
            std::swap(MSG_BUFFER, MSG_BUFFER_TEMP);
            std::thread save_file_id_t(runSaveFile, write_length);
            save_file_id_t.detach();

    #ifdef CHECH_TIME
            clock_t end1 = clock();
    #endif
            // 不可使用 clear 方法， 该方法只是将 size 变为 0
            // MSG_BUFFER.clear();
            // MSG_BUFFER.assign(MSG_BUFFER.size(), 0x00);
#endif

#ifdef CHECH_TIME
            clock_t end2 = clock();
            printf("swap time: %.3f ms\n", 1000.0 * ( end1 - start) / CLOCKS_PER_SEC);
            printf("memset time: %.3f ms\n", 1000.0 * ( end2 - end1) / CLOCKS_PER_SEC);
#endif
        }

#ifdef C_STYLE            
        memcpy(buffer_ptr + BUFFER_LENGTH, &temp_signals, sizeof(temp_signals));
#else
        std::memcpy(&MSG_BUFFER[BUFFER_LENGTH], &temp_signals, sizeof(temp_signals));
#endif
        BUFFER_LENGTH += sizeof(temp_signals);
        
    }
    pthread_exit(NULL);
}

#ifdef SAVE_TIMER
void save_handler(unsigned char x) {
    // XLOG_DEBUG("save_handler.\n");
    sem_post(&gstc_save);
}
#endif

int save_init() {
    // 文件保存未启用，直接返回
    if (g_File_Info.bIsActive == false) {
        return 0;
    }

#ifdef SAVE_TIMER
    sem_init(&gstc_save, 0, 0);
#endif

    runSaveThread = 1;

    pthread_t tid;
	pthread_create(&tid, NULL, save_msg_thread, NULL);
    pthread_setname_np(tid, "msg save");
	pthread_detach(tid);

#ifdef SAVE_TIMER
    // 定时器
    OWASYS_GetTimer(&save_timer_id, (void ( *) ( unsigned char))&save_handler, g_File_Info.nSaveCycleMs / 1000,
					(g_File_Info.nSaveCycleMs % 1000) * 1000);
	OWASYS_StartTimer(save_timer_id, MULTIPLE_TICK);
#endif

    return 0;
}

int save_stop() {
    if (g_File_Info.bIsActive == false) {
        return 0;
    }
    // 停止并释放定时器
#ifdef SAVE_TIMER
    OWASYS_StopTimer(save_timer_id);
    OWASYS_FreeTimer(save_timer_id);
    sem_destroy(&gstc_save);
#endif
    // 停止信号保存线程
    runSaveThread = 0;
    return 0;
}

