#ifndef __BLFCAN_H__
#define __BLFCAN_H__

#include "Vector/BLF.h"

class BlfCan {
private:
    // mutex
    std::mutex m_mutex;
    // object number 被改变
    std::condition_variable m_objectNumChanged;
    // 创建新的 blf 文件
    std::thread m_createNewBlfFileThread {};
    std::atomic<bool> m_createNewBlfFileThreadRunning {false};
    
    uint8_t canfd_dlc[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8,
                            12, 16, 20, 24, 32, 48, 64};

    // 基准时间戳 -- 2022.06.28
    int64_t m_baseTime {0};
    // 一个 blf 文件的默认条数 100，0000
    uint32_t defaultMaxObject {0x0F4240};
    uint32_t currentObjectNum {0};

    std::string originalBlfAbsoultePath {""};
    std::string currentBlfAbsoultePath {""};
    uint32_t blfFileNo {0};

    bool blfAvaliable {false};
    Vector::BLF::File *pBlfFile {nullptr};

    // 操作 currentObjectNum
    void increaseObjectNum();
    void resetObjectNum();
    uint32_t getObjectNum();
    // 操作 blfFileNo
    void increaseFileNO();
    uint32_t getFileNO();

    // 根据 length 获取 dlc
    uint8_t length2Dlc(uint8_t length);

    // 设置可用性
    void setBlfCanAvaliable(bool available);

    // 线程，开启新的 blf
    void startNextThread();

    bool startNext();
    // 开启新的 blf 文件
    bool startNewBlf(const std::string& trafficPath);

    // 向当前 blf 文件写入 object 记录
    void write(Vector::BLF::ObjectHeaderBase *ohb, int64_t ns_tsp);

    inline uint8_t CAN_MSG_DIR(uint8_t f) {
        return (uint8_t)(f & 0x0F);
    }

    inline uint8_t CAN_MSG_RTR(uint8_t f) {
        return (uint8_t)((f & 0x80) >> 7);
    }

    inline uint8_t CAN_MSG_WU(uint8_t f) {
        return (uint8_t)((f & 0x80) >> 6);
    }

    inline uint8_t CAN_MSG_NERR(uint8_t f) {
        return (uint8_t)((f & 0x80) >> 5);
    }

public:
    BlfCan();
    ~BlfCan();

    // 开启 blf 记录，需要使用绝对路径
    bool start(const std::string& trafficPath);
    // 停止 blf 记录
    void stop();

    // 获取可用性
    bool getBlfCanAvaliable();

    // 设置一个 blf 文件的最大记录数
    void setDefaultMaxObject(uint32_t numObject);
    // 获取一个 blf 文件的最大记录数
    uint32_t getDefaultMaxObject();

    // 向当前 blf 文件写入 can 消息
    void writeCanMessage(int channelNo, uint8_t dir, int64_t nsTimeStamp, struct can_frame canMessage);

    // 向当前 blf 文件写入 canfd 消息
    void writeCanFdMessage(int channelNo, uint8_t dir, int64_t nsTimeStamp, struct canfd_frame canFdMessage);
};

#endif
