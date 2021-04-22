
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

extern std::vector<std::string> g_out_MsgName;
extern stu_OutMessage g_stu_OutMessage;
extern Hardware_Info g_Hardware_Info;
extern Net_Info g_Net_Info;

// #define PUB_TIMER

#ifdef PUB_TIMER
static sem_t g_sem_publish;
// 定时器
static unsigned char publish_timer_id = 1;
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
        // 获取 ms 级时间戳
        uint64_t time_stamp = getTimestamp(1);

        tPOSITION_DATA sCurPositionData;
        GetFullGPSInfo(&sCurPositionData);

        std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
        g_stu_OutMessage.msg_struct.msg_time = time_stamp;
        // TODO：包含 map 的结构体，如此操作是否合适？有更高效的办法？
        stu_Message publish_message = g_stu_OutMessage.msg_struct;

        // 按顺序添加信号值
        std::vector<std::string>::iterator out_MsgNameItr = g_out_MsgName.begin();
        for (; out_MsgNameItr != g_out_MsgName.end(); out_MsgNameItr++) {
            MAP_Name_Message::iterator ItrMsg = g_stu_OutMessage.msg_struct.msg_map.find(*out_MsgNameItr);
            Out_Message temp_message{SIGNAL_NAN, "", ""};
            // 没找到(说明该信号在通道配置中一定不存在)，则写入最大值，单位留空
            if (ItrMsg == g_stu_OutMessage.msg_struct.msg_map.end()) {
                publish_message.msg_map[*out_MsgNameItr] = temp_message;
            } else {
                temp_message.dPhyVal = (ItrMsg->second).dPhyVal;
                publish_message.msg_map[*out_MsgNameItr] = temp_message;
            }
        }
#if 0
        // add GPS info -- start
        if (sCurPositionData.PosValid && sCurPositionData.numSvs >= 4) {
            Out_Message temp_gps_msg;
            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = sCurPositionData.LatDecimal;
            temp_gps_msg.strPhyUnit = "°";
            temp_gps_msg.strPhyFormat = "%.6f";
            publish_message.msg_map["Latitude"] = temp_gps_msg;

            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = sCurPositionData.LonDecimal;
            temp_gps_msg.strPhyUnit = "°";
            temp_gps_msg.strPhyFormat = "%.6f";
            publish_message.msg_map["Longitude"] = temp_gps_msg;

            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = sCurPositionData.Speed;
            temp_gps_msg.strPhyUnit = "km/h";
            temp_gps_msg.strPhyFormat = "%.3f";
            publish_message.msg_map["Speed"] = temp_gps_msg;

            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = sCurPositionData.Altitude;
            temp_gps_msg.strPhyUnit = "m";
            temp_gps_msg.strPhyFormat = "%.3f";
            publish_message.msg_map["Altitude"] = temp_gps_msg;
        } else {
            Out_Message temp_gps_msg;
            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = SIGNAL_NAN;
            temp_gps_msg.strPhyUnit = "°";
            temp_gps_msg.strPhyFormat = "%.6f";
            publish_message.msg_map["Latitude"] = temp_gps_msg;

            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = SIGNAL_NAN;
            temp_gps_msg.strPhyUnit = "°";
            temp_gps_msg.strPhyFormat = "%.6f";
            publish_message.msg_map["Longitude"] = temp_gps_msg;

            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = SIGNAL_NAN;
            temp_gps_msg.strPhyUnit = "km/h";
            temp_gps_msg.strPhyFormat = "%.3f";
            publish_message.msg_map["Speed"] = temp_gps_msg;

            memset(&temp_gps_msg, 0, sizeof(temp_gps_msg));
            temp_gps_msg.dPhyVal = SIGNAL_NAN;
            temp_gps_msg.strPhyUnit = "m";
            temp_gps_msg.strPhyFormat = "%.3f";
            publish_message.msg_map["Altitude"] = temp_gps_msg;
        }
        // add GPS info -- end
        // add device info -- start
        float ad_v_in = 0.0f;
        if (0 == GetAD_V_IN(&ad_v_in)) {
            Out_Message temp_vin_msg;
            memset(&temp_vin_msg, 0, sizeof(temp_vin_msg));
            temp_vin_msg.dPhyVal = ad_v_in;
            temp_vin_msg.strPhyUnit = "V";
            temp_vin_msg.strPhyFormat = "%.6f";
            publish_message.msg_map["TBox_V_IN"] = temp_vin_msg;
        }

        int ad_temp = 0;
        if (0 == GetAD_TEMP(&ad_temp)) {
            Out_Message temp_msg;
            memset(&temp_msg, 0, sizeof(temp_msg));
            temp_msg.dPhyVal = ad_temp;
            temp_msg.strPhyUnit = "°C";
            temp_msg.strPhyFormat = "%d";
            publish_message.msg_map["TBox_Temp"] = temp_msg;
        }  
        // add device info -- end

        // test data
        {
            static double test_data = 0.0;
            Out_Message temp_msg;
            memset(&temp_msg, 0, sizeof(temp_msg));
            temp_msg.dPhyVal = test_data++;
            temp_msg.strPhyUnit = "x";
            temp_msg.strPhyFormat = "%d";
            publish_message.msg_map["A_COUNT_MESSAGE"] = temp_msg;
        }
#endif
        // 入队, 达到限制则丢掉最新的
        std::lock_guard<std::mutex> q_lock(publish_queue.queue_lock);
        if (publish_queue.queue_msg.size() < PUBLISH_QUEUE_MAX_SIZE) {
            publish_queue.queue_msg.push(publish_message);
        }
        XLOG_TRACE("publish_queue size: {}", publish_queue.queue_msg.size());
    }
    pthread_exit(NULL);
}

void *publish_signals_thread(void *arg) {
    
    int is_iNET_active = 0;
    int singals_count = g_out_MsgName.size();
    char temp_signals[8 * (2 + singals_count)];

    char signal_topic[100] = {0};
    GetMQTTTopic(signal_topic, MQTT_TOPIC::signals);
    
    while (1) {
        // 500 ms 检测一次
        // usecsleep(0, 500000);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (!MQTT_pulish_run || !runPublishThread) {
            XLOG_DEBUG("MQTT_pulish_run = {}, runPublishThread = {}", MQTT_pulish_run, runPublishThread);
            break;
        }
        
        // 检查网络是否正常
        if (0 == iNet_IsActive(&is_iNET_active)) {
            // 网络不正常
            if (is_iNET_active == 0) {
                XLOG_DEBUG("Network error.");
                continue;
            }
        } else {
            XLOG_DEBUG("iNet_IsActive error.");
            continue;
        }

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

            MAP_Name_Message::iterator ItrMsg = temp_message.msg_map.begin();
            int signal_index = 0;
            while (ItrMsg != temp_message.msg_map.end()
                    && (signal_index < singals_count)) {

                double dval = (ItrMsg->second).dPhyVal;
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
    XLOG_TRACE("call function push_handler.");
    sem_post(&g_sem_publish);
    return ;
}
#endif

int publish_init() {
#ifdef PUB_TIMER
    sem_init(&g_sem_publish, 0, 0);
#endif
    // msg 入队线程
    pthread_t push_queue_thread_id;
    pthread_create(&push_queue_thread_id, NULL, push_to_publish_queue, NULL);
    pthread_setname_np(push_queue_thread_id, "msg push");
	pthread_detach(push_queue_thread_id);

#ifdef PUB_TIMER
    // 定时器，使用定时器断网时可能会失效，目前原因不详
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
    MQTT_pulish_run = 0;
    runPublishThread = 0;

#ifdef PUB_TIMER
    OWASYS_StopTimer(publish_timer_id);
    OWASYS_FreeTimer(publish_timer_id);
#endif

    if (-1 == Mosquitto_Disconnect()) {
		XLOG_WARN("Mosquitto Disconnect ERROR.");
	} else {
		XLOG_INFO("Mosquitto Disconnect successfully.");
	}

    Mosquitto_Loop_Stop();
    return 0;
}
