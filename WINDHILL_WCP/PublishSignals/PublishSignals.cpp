
#include <string.h>
#include <time.h>
#include <float.h>
#include <semaphore.h>
#include <stdlib.h>

#include <queue>
#include <chrono>
#include <thread>

#include "PublishSignals/PublishSignals.h"
#include "ConvertData/ConvertData.h"
#include "Mosquitto/MOSQUITTO.h"
#include "DevConfig/PhyChannel.hpp"

#include "tbox/Common.h"
#include "tbox/GPS.h"
#include "tbox/DevInfo.h"
#include "datatype.h"

#include "CJson/cJSON.h"
#include "Logger/Logger.h"

extern stu_OutMessage g_stu_OutMessage;

extern Hardware_Info g_Hardware_Info;
extern Net_Info g_Net_Info;

#define PUB_TIMER

#ifdef PUB_TIMER
static sem_t g_sem_publish;
// 定时器
static unsigned char publish_timer_id = TIMER_ID::PUBLISH_TIMER_ID;
#endif

static unsigned char MQTT_pulish_run = 1;
// 发送线程运行标志
static unsigned char runPublishThread = 0;
// 发送队列
typedef struct{
    std::mutex queue_lock;
    std::queue<stu_Message> queue_msg;
} PUBLISH_QUEUE;
static PUBLISH_QUEUE publish_queue;

static const uint64_t FIXED_HEAD = 0x0000000100000000;

static const int PUBLISH_QUEUE_MAX_SIZE = 1000;

void *push_to_publish_queue(void *arg) {
    while (1) {
#ifdef PUB_TIMER
        sem_wait(&g_sem_publish);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(g_Net_Info.mqtt_server.nPublishMs));
#endif
        if (!MQTT_pulish_run) {
            XLOG_DEBUG("MQTT_pulish_run = {}", MQTT_pulish_run);
            break;
        }
        
        stu_Message publish_message;
        {
            std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
            LIST_Message::iterator ItrMsg = g_stu_OutMessage.msg_struct.msg_list.begin();

            // 获取 ms 级时间戳
            uint64_t time_stamp = getTimestamp(TIME_STAMP::MS_STAMP);
            publish_message.msg_time = time_stamp;

            for (; ItrMsg != g_stu_OutMessage.msg_struct.msg_list.end(); ItrMsg++) {
                Out_Message temp_message{(*ItrMsg).strName, (*ItrMsg).dPhyVal, (*ItrMsg).strPhyUnit, (*ItrMsg).strPhyFormat};
                publish_message.msg_list.push_back(temp_message);
                (*ItrMsg).dPhyVal = SIGNAL_NAN;     // TODO:debug, 压入发送队列后将值设为 NAN
            }
        }

        // 入队, 达到限制则丢掉最旧的
        std::lock_guard<std::mutex> q_lock(publish_queue.queue_lock);
        if (publish_queue.queue_msg.size() >= PUBLISH_QUEUE_MAX_SIZE) {
            publish_queue.queue_msg.pop();
        }
        publish_queue.queue_msg.push(publish_message);

        XLOG_TRACE("publish_queue size: {}", publish_queue.queue_msg.size());
    }
    pthread_exit(NULL);
}

void *publish_signals_thread(void *arg) {
    
    // int is_iNET_active = 0;
    int singals_count = g_stu_OutMessage.msg_struct.msg_list.size();
    char temp_signals[8 * (2 + singals_count)];

    char signal_topic[100] = {0};
    GetMQTTTopic(signal_topic, MQTT_TOPIC::signals);
    
    while (1) {
        // 50 ms 检测一次
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        if (!MQTT_pulish_run || !runPublishThread) {
            XLOG_DEBUG("MQTT_pulish_run = {}, runPublishThread = {}", MQTT_pulish_run, runPublishThread);
            break;
        }
        
        // 检查网络是否正常
        // if (0 == iNet_IsActive(&is_iNET_active)) {
        //     // 网络不正常
        //     if (is_iNET_active == 0) {
        //         XLOG_DEBUG("Network error.");
        //         continue;
        //     }
        // } else {
        //     XLOG_DEBUG("iNet_IsActive error.");
        //     continue;
        // }

        // 获取发送队列的锁
        std::lock_guard<std::mutex> q_lock(publish_queue.queue_lock);
        // XLOG_TRACE("publish_queue size: {}", publish_queue.queue_msg.size());
        while (!publish_queue.queue_msg.empty() ) {
            // TODO：结构体操作？
            stu_Message temp_message = publish_queue.queue_msg.front();
            memset(temp_signals, 0, sizeof(temp_signals));
            // 固定头
            memcpy(temp_signals, &FIXED_HEAD, sizeof(FIXED_HEAD));
            // 时间戳
            uint64_t time_stamp = temp_message.msg_time;
            memcpy(temp_signals + sizeof(FIXED_HEAD), &time_stamp, sizeof(time_stamp));

            LIST_Message::iterator ItrMsg = temp_message.msg_list.begin();
            int signal_index = 0;
            while (ItrMsg != temp_message.msg_list.end()
                    && (signal_index < singals_count)) {
                
                double dval = (*ItrMsg).dPhyVal;
                memcpy(temp_signals + sizeof(FIXED_HEAD) + sizeof(time_stamp)
                        + sizeof(dval) * signal_index++, &dval, sizeof(dval));

                ItrMsg++;
            }

            int retval = Mosquitto_Pub(signal_topic, sizeof(temp_signals), temp_signals);
            
            if (0 == retval) {
                // 发送成功则出队
                publish_queue.queue_msg.pop();
            } else {
                break;
            }
            
        }
    }
    pthread_exit(NULL);
}

#ifdef PUB_TIMER
void push_handler() {
    // XLOG_TRACE("call function push_handler.");
    sem_post(&g_sem_publish);
    return ;
}
#endif

int publish_init() {
    // MQTT 未激活，直接返回
    if (g_Net_Info.mqtt_server.bIsActive == false) {
        return 0;
    }
#ifdef PUB_TIMER
    // 信号量初始化
    sem_init(&g_sem_publish, 0, 0);
#endif

    // msg 入队线程
    pthread_t push_queue_thread_id;
    pthread_create(&push_queue_thread_id, NULL, push_to_publish_queue, NULL);
    pthread_setname_np(push_queue_thread_id, "msg push");
	pthread_detach(push_queue_thread_id);

#ifdef PUB_TIMER
    // 优化显示效果
    uint64_t temp;
    clock_t start = clock();
    while (temp = getTimestamp(TIME_STAMP::MS_STAMP), temp % 1000 > 200 );
    clock_t end = clock();
    XLOG_TRACE("The time taken to wait timestamp {:.3f} ms.\n", 1000.0 * ( end - start) / CLOCKS_PER_SEC);
    XLOG_TRACE("timestamp = {}\n", temp);
    // 定时器
    OWASYS_GetTimer(&publish_timer_id, (void (*)(unsigned char)) & push_handler, g_Net_Info.mqtt_server.nPublishMs / 1000,
                    (g_Net_Info.mqtt_server.nPublishMs % 1000) * 1000);
    OWASYS_StartTimer(publish_timer_id, MULTIPLE_TICK);
#endif

    // 创建发送线程
    runPublishThread = 1;
    pthread_t publish_thread_id;
	pthread_create(&publish_thread_id, NULL, publish_signals_thread, NULL);
    pthread_setname_np(publish_thread_id, "msg publish");
	pthread_detach(publish_thread_id);

    XLOG_DEBUG("publish_init ok.");
    return 0;
}

int publish_stop() {
    if (g_Net_Info.mqtt_server.bIsActive == false) {
        return 0;
    }
    MQTT_pulish_run = 0;
    runPublishThread = 0;

#ifdef PUB_TIMER
    OWASYS_StopTimer(publish_timer_id);
    OWASYS_FreeTimer(publish_timer_id);
    sem_destroy(&g_sem_publish);
#endif

    if (-1 == Mosquitto_Disconnect()) {
		XLOG_WARN("Mosquitto Disconnect ERROR.");
	} else {
		XLOG_INFO("Mosquitto Disconnect successfully.");
	}

    Mosquitto_Loop_Stop();
    return 0;
}
