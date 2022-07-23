
#include <linux/can.h>

#include "BLF_LOG/BlfCan.h"

BlfCan::BlfCan() {
}

BlfCan::~BlfCan() {
    // stop();
}

void BlfCan::setBlfCanAvaliable(bool available) {
    /* mutex lock */
    std::lock_guard<std::mutex> lock(m_mutex);

    blfAvaliable = available;
}

bool BlfCan::getBlfCanAvaliable() {
    /* mutex lock */
    std::lock_guard<std::mutex> lock(m_mutex);

    return blfAvaliable;
}

void BlfCan::setDefaultMaxObject(uint32_t numObject) {
    /* mutex lock */
    std::lock_guard<std::mutex> lock(m_mutex);

    defaultMaxObject = numObject;
}

uint32_t BlfCan::getDefaultMaxObject() {
    /* mutex lock */
    std::lock_guard<std::mutex> lock(m_mutex);

    return defaultMaxObject;
}

void BlfCan::increaseObjectNum() {
    std::lock_guard<std::mutex> lock(m_mutex);
    currentObjectNum++;
}

void BlfCan::resetObjectNum() {
    std::lock_guard<std::mutex> lock(m_mutex);
    currentObjectNum = 0;
}

uint32_t BlfCan::getObjectNum() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return currentObjectNum;
}

void BlfCan::increaseFileNO() {
    std::lock_guard<std::mutex> lock(m_mutex);
    blfFileNo++;
}

uint32_t BlfCan::getFileNO() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return blfFileNo;
}

bool BlfCan::start(const std::string& trafficPath) {
    if (trafficPath.empty()) {
        return false;
    }

    pBlfFile = new (std::nothrow)Vector::BLF::File;
    if (nullptr == pBlfFile) {
        return false;
    }
    // 基准时间
    m_baseTime = pBlfFile->getOpenTimestamp();
    // 路径赋值
    originalBlfAbsoultePath = trafficPath;
    currentBlfAbsoultePath = trafficPath;
    // 启动创建线程
    m_createNewBlfFileThreadRunning = true;
    m_createNewBlfFileThread = std::thread(&BlfCan::startNextThread, this);
    pthread_setname_np(m_createNewBlfFileThread.native_handle(), "create blf");
    m_createNewBlfFileThread.detach();

    return startNewBlf(originalBlfAbsoultePath);
}

bool BlfCan::startNewBlf(const std::string& trafficPath){
    pBlfFile->open(trafficPath, std::ios_base::out);
    if (!pBlfFile->is_open()) {
        return false;
    }

    // currentObjectNum = 0;
    resetObjectNum();
    setBlfCanAvaliable(true);
    return true;
}

void BlfCan::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!blfAvaliable) {
        return;
    }

    blfAvaliable = false;

    // 停止创建线程
    m_createNewBlfFileThreadRunning = false;
    m_objectNumChanged.notify_all();
    
    if (pBlfFile) {
        pBlfFile->close();
        delete pBlfFile;
        pBlfFile = nullptr;
    }

    if (0 == currentObjectNum) {
        std::remove(currentBlfAbsoultePath.c_str());
    }
}

bool BlfCan::startNext() {

    stop();
    pBlfFile = new (std::nothrow)Vector::BLF::File;
    if (nullptr == pBlfFile) {
        return false;
    }

    // blfFileNo++;
    increaseFileNO();
    size_t index = originalBlfAbsoultePath.find_last_of(".");
    if (std::string::npos == index) {
        return false;
    }
    uint32_t idx = getFileNO();
    currentBlfAbsoultePath = originalBlfAbsoultePath.substr(0, index) + "_" + std::to_string(idx) + originalBlfAbsoultePath.substr(index);

    return startNewBlf(currentBlfAbsoultePath);
}

void BlfCan::startNextThread() {

// static int idx = 0;
// printf("a new m_createNewBlfFileThread.\n");
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_createNewBlfFileThreadRunning) {
        // 等待唤醒
        m_objectNumChanged.wait(lock);
// idx++;
// printf("m_createNewBlfFileThread get lock, idx = %d.\n", idx);
        if (false == m_createNewBlfFileThreadRunning) {
            break;
        }

        if (currentObjectNum < defaultMaxObject) {
            continue;
        }

        if (blfAvaliable) {
            blfAvaliable = false;
        }
        
        pBlfFile->close();
        delete pBlfFile;
        pBlfFile = nullptr;

        blfFileNo++;
        size_t index = originalBlfAbsoultePath.find_last_of(".");
        if (std::string::npos == index) {
            continue;
        }
        currentBlfAbsoultePath = originalBlfAbsoultePath.substr(0, index) + "_" + std::to_string(blfFileNo) + originalBlfAbsoultePath.substr(index);

        pBlfFile = new (std::nothrow)Vector::BLF::File(m_baseTime);
        if (nullptr == pBlfFile) {
            continue;
        }
        // startNewBlf(currentBlfAbsoultePath);
        pBlfFile->open(currentBlfAbsoultePath, std::ios_base::out);
        if (!pBlfFile->is_open()) {
            continue;
        }

        currentObjectNum = 0;
        blfAvaliable = true;
// printf("m_createNewBlfFileThread finish, idx = %d.\n", idx);
    }
}

uint8_t BlfCan::length2Dlc(uint8_t length) {
    uint8_t left = 0;
    uint8_t right = sizeof(canfd_dlc) - 1;
    uint8_t middle = 0;

    while (left <= right) {
        middle = left + (right - left) / 2;
        if (canfd_dlc[middle] < length) {
            left = middle + 1;
        } else if (canfd_dlc[middle] > length) {
            right = middle - 1;
        } else {
            return middle;
        }
    }

    return 0;
}

void BlfCan::write(Vector::BLF::ObjectHeaderBase *ohb, int64_t ns_tsp) {
    std::lock_guard<std::mutex> lock(m_mutex);

    pBlfFile->write(ohb, ns_tsp);
    currentObjectNum++;

    if (currentObjectNum >= defaultMaxObject) {
        // startNext();
        m_objectNumChanged.notify_all();
    }
}

void BlfCan::writeCanMessage(int channelNo, uint8_t dir, int64_t nsTimeStamp, struct can_frame canMessage) {

    uint8_t len = canMessage.can_dlc;
    uint32_t id = canMessage.can_id;
    int64_t relative_timestamp = 0;
    int64_t last_timestamp = nsTimeStamp;
    if (0 == nsTimeStamp) {
        last_timestamp = pBlfFile->getNowTimestamp();
        relative_timestamp = last_timestamp - m_baseTime;//pBlfFile->getOpenTimestamp();
    } else {
        relative_timestamp = nsTimeStamp - m_baseTime;//pBlfFile->getOpenTimestamp();
    }

    if (id & CAN_ERR_FLAG) {
        auto *BlfCanErrorFrame = new Vector::BLF::CanErrorFrame;
        BlfCanErrorFrame->objectVersion = 1;
        BlfCanErrorFrame->objectType = Vector::BLF::ObjectType::CAN_ERROR;
        BlfCanErrorFrame->channel = channelNo;
        BlfCanErrorFrame->objectTimeStamp = relative_timestamp;
        BlfCanErrorFrame->length = len;

        write(BlfCanErrorFrame, last_timestamp);
        
    } else {
        auto *BlfCanMessage = new Vector::BLF::CanMessage;
        BlfCanMessage->objectVersion = 1;
        BlfCanMessage->objectType = Vector::BLF::ObjectType::CAN_MESSAGE;
        BlfCanMessage->channel = channelNo;
        BlfCanMessage->objectTimeStamp = relative_timestamp;
        BlfCanMessage->dlc = len;
        BlfCanMessage->id = id;
        BlfCanMessage->flags = CAN_MSG_DIR(dir);
        
        // 远程帧
        if (id & CAN_RTR_FLAG) {
            BlfCanMessage->flags |= CAN_MSG_RTR(1);
        } else {
            for (uint8_t i = 0; i < len; i++) {
                BlfCanMessage->data[i] = canMessage.data[i];
            }
        }

        write(BlfCanMessage, last_timestamp);/*  */
    }

}

void BlfCan::writeCanFdMessage(int channelNo, uint8_t dir, int64_t nsTimeStamp, struct canfd_frame canFdMessage) {

    uint8_t len = canFdMessage.len;
    uint32_t id = canFdMessage.can_id;
    int64_t relative_timestamp = 0;
     int64_t last_timestamp = nsTimeStamp;
    if (0 == nsTimeStamp) {
        last_timestamp = pBlfFile->getNowTimestamp();
        relative_timestamp = last_timestamp - m_baseTime;//pBlfFile->getOpenTimestamp();
    } else {
        relative_timestamp = nsTimeStamp - m_baseTime;//pBlfFile->getOpenTimestamp();
    }

    if (id & CAN_ERR_FLAG) {
        auto *BlfCanFdErrorFrame64 = new Vector::BLF::CanFdErrorFrame64;
        BlfCanFdErrorFrame64->objectVersion = 1;
        BlfCanFdErrorFrame64->objectType = Vector::BLF::ObjectType::CAN_FD_ERROR_64;
        BlfCanFdErrorFrame64->channel = channelNo;
        BlfCanFdErrorFrame64->objectTimeStamp = relative_timestamp;
        
        BlfCanFdErrorFrame64->id = id;
        BlfCanFdErrorFrame64->dlc = length2Dlc(len);
        BlfCanFdErrorFrame64->validDataBytes = len;
        // 设置 FDF
        BlfCanFdErrorFrame64->extFlags = (uint16_t)1 << 0;
        if (canFdMessage.flags & CANFD_BRS) {
            BlfCanFdErrorFrame64->extFlags |= ((uint16_t)1 << 1);
        }

        // for (uint8_t i = 0; i < len; i++) {
        //     BlfCanFdErrorFrame64->data.push_back(canFdMessage.data[i]);
        // }
        BlfCanFdErrorFrame64->data.reserve(len);
        BlfCanFdErrorFrame64->data.assign(&canFdMessage.data[0], &canFdMessage.data[len]);

        write(BlfCanFdErrorFrame64, last_timestamp);

    } else {
        auto *BlfCanFdMessage64 = new Vector::BLF::CanFdMessage64;
        BlfCanFdMessage64->objectVersion = 1;
        BlfCanFdMessage64->objectType = Vector::BLF::ObjectType::CAN_FD_MESSAGE_64;
        BlfCanFdMessage64->channel = channelNo;
        BlfCanFdMessage64->objectTimeStamp = relative_timestamp;
        BlfCanFdMessage64->dir = dir;

        BlfCanFdMessage64->id = id;
        BlfCanFdMessage64->dlc = length2Dlc(len);
        BlfCanFdMessage64->validDataBytes = len;
        // 设置 FDF
        BlfCanFdMessage64->flags = (uint16_t)1 << 12;
        if (canFdMessage.flags & CANFD_BRS) {
            BlfCanFdMessage64->flags |= ((uint16_t)1 << 13);
        }

        // for (uint8_t i = 0; i < len; i++) {
        //     BlfCanFdMessage64->data.push_back(canFdMessage.data[i]);
        // }
        BlfCanFdMessage64->data.reserve(len);
        BlfCanFdMessage64->data.assign(&canFdMessage.data[0], &canFdMessage.data[len]);

        write(BlfCanFdMessage64, last_timestamp);
    }

}