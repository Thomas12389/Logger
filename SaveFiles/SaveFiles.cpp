
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

extern stu_SaveMessage g_stu_SaveMessage;
extern File_Save g_File_Info;
extern std::vector<std::string> g_save_MsgName;

// 文件目录
std::string STORGE_DIR_NAME = "/media/data/";

// 缓冲区
static const uint32_t MSG_BUFFER_SIZE = 1024 * 1024 * 10;
static char MSG_BUFFER[MSG_BUFFER_SIZE + 5];
// 缓冲区已写入的长度
static uint32_t BUFFER_LENGTH = 0;
// 保存文件的编号
static const uint16_t MAX_FILE_NO = 500;
static uint16_t nFileNO = 0x0001;

// #define SAVE_TIMER

#ifdef SAVE_TIMER
// 信息保存 信号量
static sem_t gstc_save;
static unsigned char save_timer_id = 0;
#else
static int runSaveThread = 0;
#endif

int save_files(uint16_t FileNO, uint32_t Length, uint8_t IsStop) {
    // 停止并释放定时器
    if (IsStop) {
#ifdef SAVE_TIMER
        OWASYS_StopTimer(save_timer_id);
	    OWASYS_FreeTimer(save_timer_id);
#else
        runSaveThread = 0;
#endif
    }

    clock_t start = clock();

    std::string ABSOULT_PATH_DIR_NAME = STORGE_DIR_NAME + g_File_Info.strLocalDirName;
    char old_path[256] = {0};
    getcwd(old_path, sizeof(old_path));
    if (NULL == opendir(ABSOULT_PATH_DIR_NAME.c_str())) {
        XLOG_DEBUG("Directory {} doesn'e exist.", ABSOULT_PATH_DIR_NAME.c_str());
        int ret = 0;
        if ( (ret = mkdir(ABSOULT_PATH_DIR_NAME.c_str(), 0775)) == 0) {
            XLOG_DEBUG("Make directory {} to save data.", ABSOULT_PATH_DIR_NAME.c_str());
        } else {
            XLOG_DEBUG("mkdir: {}.", strerror(errno));
            return -1;
        }
    }
    
    chdir(ABSOULT_PATH_DIR_NAME.c_str());

    char file_name[256] = {0};
    // 限制文件个数
    if (FileNO > MAX_FILE_NO) {
        FileNO = 0;
    }
    if (FileNO == 0) {
        FileNO = nFileNO++;
    }
    sprintf(file_name, "%04d", FileNO);
    FILE *pF = fopen(file_name, "wb+");
    if (NULL == pF) {
        XLOG_DEBUG("fopen: {}", strerror(errno));
        return -1;
    }
    if (Length == 0) {
        Length = BUFFER_LENGTH;
    }
    // 不要使用 strlen(MSG_BUFFER)， 否则遇到 "00" 会发生截断，导致数据丢失
    // 也不要使用 sizeof(MSG_BUFFER)， 无法保证 恰好 写满了缓冲区
    size_t total_write_blocks = fwrite(MSG_BUFFER, 1, Length, pF);
    if (total_write_blocks < Length) {
        XLOG_DEBUG("fwrite: {}", strerror(errno));
        fclose(pF);
        chdir(old_path);
        return -1;
    }
    fclose(pF);
    clock_t end = clock();
    XLOG_DEBUG("The time taken to save file {:04d}:{:.3f} ms.", FileNO, 1000.0 * ( end - start) / CLOCKS_PER_SEC);

    chdir(old_path);
    return 0;
}

void *save_msg_thread(void *arg) {
    while(1) {
#ifdef SAVE_TIMER
        sem_wait(&gstc_save);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(g_File_Info.nSaveCycleMs));
        if (!runSaveThread) {
            break;
        }
#endif
        std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
        // 写入时间戳
        // uint64_t time_stamp = g_stu_SaveMessage.msg_struct.msg_time;
        uint64_t time_stamp = getTimestamp(3);
        if (BUFFER_LENGTH + sizeof(time_stamp) > MSG_BUFFER_SIZE) {
            if(0 != save_files(nFileNO++, BUFFER_LENGTH, 0)) {
                XLOG_DEBUG("Partial data loss.");
            }
            BUFFER_LENGTH = 0;
            memset(MSG_BUFFER, 0, sizeof(MSG_BUFFER));
        }
        memcpy(MSG_BUFFER + BUFFER_LENGTH, &time_stamp, sizeof(time_stamp));
        BUFFER_LENGTH += sizeof(time_stamp);

        std::vector<std::string>::iterator MsgNameItr = g_save_MsgName.begin();
        for (; MsgNameItr != g_save_MsgName.end(); MsgNameItr++) {
            MAP_Name_Message::iterator ItrMsg = g_stu_SaveMessage.msg_struct.msg_map.find(*MsgNameItr);
            // 没找到(说明该信号在通道配置中一定不存在)，则写入最大值
            if (ItrMsg == g_stu_SaveMessage.msg_struct.msg_map.end()) {
                float max_float = FLT_MAX;
                // 再写入将会超过 BUFFER 大小，发生越界
                if (BUFFER_LENGTH + sizeof(max_float) > MSG_BUFFER_SIZE) {
                    if(0 != save_files(nFileNO++, BUFFER_LENGTH, 0)) {
                        XLOG_DEBUG("Partial data loss.");
                    }
                    BUFFER_LENGTH = 0;
                    memset(MSG_BUFFER, 0, sizeof(MSG_BUFFER));
                }
        
                memcpy(MSG_BUFFER + BUFFER_LENGTH, &max_float, sizeof(max_float));
                BUFFER_LENGTH += sizeof(max_float);
                continue;
            }
            // 再写入将会超过 BUFFER 大小， 发生越界
            if (BUFFER_LENGTH + sizeof(ItrMsg->second.dPhyVal) > MSG_BUFFER_SIZE) {
                if(0 != save_files(nFileNO++, BUFFER_LENGTH, 0)) {
                    XLOG_DEBUG("Partial data loss.");
                }
                BUFFER_LENGTH = 0;
                memset(MSG_BUFFER, 0, sizeof(MSG_BUFFER));
            }

            memcpy(MSG_BUFFER + BUFFER_LENGTH, &(ItrMsg->second.dPhyVal), sizeof(ItrMsg->second.dPhyVal));
            BUFFER_LENGTH += sizeof(ItrMsg->second.dPhyVal);
        }
        
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
#ifdef SAVE_TIMER
    sem_init(&gstc_save, 0, 0);
#else
    runSaveThread = 1;
#endif
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

