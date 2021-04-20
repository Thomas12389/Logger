
#include <stdio.h>
#include <string.h>
#include "tbox/Common.h"
#include "MOSQUITTO.h"
#include "PublishSignals/PublishSignals.h"
#include "DevConfig/PhyChannel.hpp"

#include "Logger/Logger.h"

extern Hardware_Info g_Hardware_Info;
extern Net_Info g_Net_Info;
extern char *p_app_json;
extern char *p_info_json;

static struct mosquitto *mosq = NULL;

void GetMQTTTopic(char *pTopic, MQTT_TOPIC topic_type) {
	char topic[100] = "/";
	strncat(topic, g_Hardware_Info.SeqNum.c_str(), g_Hardware_Info.SeqNum.length());
	switch (topic_type) {
		case MQTT_TOPIC::app_json:
			strncat(topic, "/app_json", strlen("/app_json"));
			break;
		case MQTT_TOPIC::info_json:
			strncat(topic, "/info_json", strlen("/info_json"));
			break;
		case MQTT_TOPIC::kpiDashboard:
			strncat(topic, "/kpiDashboard", strlen("/kpiDashboard"));
			break;
		case MQTT_TOPIC::layout_json:
			strncat(topic, "/layout_json", strlen("/layout_json"));
			break;
		case MQTT_TOPIC::online:
			strncat(topic, "/online", strlen("/online"));
			break;
		case MQTT_TOPIC::setChannelValue:
			strncat(topic, "/setChannelValue", strlen("/setChannelValue"));
			break;
		case MQTT_TOPIC::signals:
			strncat(topic, "/signals", strlen("/signals"));
			break;
		case MQTT_TOPIC::syslog:
			strncat(topic, "/syslog", strlen("/syslog"));
			break;
		case MQTT_TOPIC::syslogRestart:
			strncat(topic, "/syslogRestart", strlen("/syslogRestart"));
			break;
		case MQTT_TOPIC::uplinkUpdateInterval:
			strncat(topic, "/uplinkUpdateInterval", strlen("/uplinkUpdateInterval"));
			break;
		case MQTT_TOPIC::uplinkVersion:
			strncat(topic, "/uplinkVersion", strlen("/uplinkVersion"));
			break;
		default:
			// TODO
			strncpy(topic, "", strlen(""));
			break;
	}
	
	strncpy(pTopic, topic, strlen(topic) + 1);
	
	return ;
}

int _publish_uplinkVersion() {
	char uplinkVersion_topic[100] = {0};
	GetMQTTTopic(uplinkVersion_topic, MQTT_TOPIC::uplinkVersion);
	// TODO, 是否在 APP 上显示 kpiDashboard 按钮
#if 0
	return mosquitto_publish(mosq, NULL, uplinkVersion_topic, strlen("1.8.1"), "1.8.1", UPLINK_QOS, true);
#else
	return mosquitto_publish(mosq, NULL, uplinkVersion_topic, strlen("1.6.1"), "1.6.1", UPLINK_QOS, true);
#endif
}

int _publish_uplinkUpdateInterval() {
	char uplinkUpdateInterval_topic[100] = {0};
	GetMQTTTopic(uplinkUpdateInterval_topic, MQTT_TOPIC::uplinkUpdateInterval);
	char update_interval[20];
	sprintf(update_interval, "%u", g_Net_Info.mqtt_server.nPublishMs);

	return mosquitto_publish(mosq, NULL, uplinkUpdateInterval_topic, strlen(update_interval), update_interval, UPLINK_QOS, true);
}

int _publish_online() {
	char online_topic[100] = {0};
	GetMQTTTopic(online_topic, MQTT_TOPIC::online);
	// 设备上线消息
	return mosquitto_publish(mosq, NULL, online_topic, strlen("true"), "true", ONLINE_QOS, true);
}

int _publish_app_json() {
	char app_json_topic[100] = {0};
	GetMQTTTopic(app_json_topic, MQTT_TOPIC::app_json);
	// 设备配置信息
	if (!p_app_json) {
		XLOG_TRACE("p_app_json ERROR.");
		return -1;
	}
	return mosquitto_publish(mosq, NULL, app_json_topic, strlen(p_app_json), p_app_json, APP_JSON_QOS, true);
}

int _publiah_info_json() {
	char info_json_topic[100] = {0};
	GetMQTTTopic(info_json_topic, MQTT_TOPIC::info_json);
	// 设备版本、时区配置
	if (!p_info_json) {
		XLOG_TRACE("p_info_json ERROR.");
		return -1;
	}
	return mosquitto_publish(mosq, NULL, info_json_topic, strlen(p_info_json), p_info_json, APP_JSON_QOS, true);
}

void connect_callback(struct mosquitto *mosq, void *obj, int rc) {
	_publish_uplinkVersion();
	_publish_uplinkUpdateInterval();
	_publish_online();
	_publish_app_json();
	_publiah_info_json();
}

int Mosquitto_Init()  {
	// libmosquitto 库初始化 
	mosquitto_lib_init(); 
	// Create a new mosquitto client instance
	char client_id[256] = "TBox_";
	// 保证 clientID 唯一，当有同名的客户端连接时，之前的客户端会被剔除
	strncat(client_id, g_Hardware_Info.SeqNum.c_str(), g_Hardware_Info.SeqNum.length());
	mosq = mosquitto_new(client_id, true, NULL); 
	if(!mosq) { 
		XLOG_ERROR("Create MQTT client failed.{}", client_id);
		mosquitto_lib_cleanup(); 
		return -1; 
	}
	mosquitto_connect_callback_set(mosq, connect_callback);
	// mosquitto_disconnect_callback_set(mosq, disconnect_callback);

	// 设置自动重联参数
	// delay=1, delay_max=64, exponential_backoff=True
	// Delays would be: 1, 2, 4, 8, 8， ...
	mosquitto_reconnect_delay_set(mosq, 1, 8, true);

	// 异常断开时发布, 遗嘱消息
	char online_topic[100];
	GetMQTTTopic(online_topic, MQTT_TOPIC::online);
	mosquitto_will_set(mosq, online_topic, strlen("false"),"false", WILL_QOS, true);

	// 设置用户名/密码
	int retval = mosquitto_username_pw_set(mosq, g_Net_Info.mqtt_server.strUserName.c_str(), g_Net_Info.mqtt_server.strPasswd.c_str());
	if (MOSQ_ERR_SUCCESS != retval) {
		XLOG_DEBUG("mosquitto_username_pw_set ERROR");
		return -1;
	}

	return 0;
}

// 设置 tls 相关选项
int Mosquitto_TLS_Setting() {
	// mosquitto_tls 系列函数
	return 0;
}

int Mosquitto_Reconnect() {
	// 重联次数
	unsigned char count = 1;
	for(; count <= MOSQUITTO_RECONNECT_COUUNT; count++) {
		XLOG_WARN("Try {:d}th reconnect after 2 seconds...", count);
		usecsleep(2, 0);		//间隔 2s 重连

		//reconnect server 
		if(MOSQ_ERR_SUCCESS == mosquitto_reconnect_async(mosq))	{
			break;
		}
	}
		
	if(count > MOSQUITTO_RECONNECT_COUUNT) {
		// mosquitto_destroy(mosq);	//free memory associated with a mosquitto client instance
		// mosquitto_lib_cleanup();
		XLOG_WARN("Unable to connect the server! Please check the network environment.");
		return -1;
	}

	return 0;
}

int Mosquitto_Connect()  {
	//Connect server
	if (NULL == mosq) {
		return -1;
	}

	int ret = mosquitto_connect_async(mosq, g_Net_Info.mqtt_server.strIP.c_str(), g_Net_Info.mqtt_server.nPort, KEEP_ALIVE);
	if(MOSQ_ERR_SUCCESS != ret) {
		XLOG_WARN("Mosquitto Connect:{}", mosquitto_strerror(ret));
		return -1;
	}
	XLOG_INFO("Connect the MQTT server success!");
	
	return 0;
}

int Mosquitto_Disconnect()  {
	if (NULL == mosq) return 0;

	char online_topic[100];
	GetMQTTTopic(online_topic, MQTT_TOPIC::online);
	mosquitto_publish(mosq, NULL, online_topic, strlen("false"),"false", ONLINE_QOS, true);

	int ret = mosquitto_disconnect(mosq);
	if(MOSQ_ERR_SUCCESS != ret) {
		XLOG_WARN("Mosquitto Disconnect:{}",mosquitto_strerror(ret));
		return -1;
	}
	XLOG_DEBUG("Mosquitto Disconnect ok.");	

	return 0;
}

int Mosquitto_Loop_Start() {
	//开启一个 network_thread 线程，在线程里不停的调用 mosquitto_loop() 来处理网络信息 
	int loop = mosquitto_loop_start(mosq);
	if(loop != MOSQ_ERR_SUCCESS) { 
		XLOG_DEBUG("Mosquitto_Loop_Start:{}", mosquitto_strerror(loop));
		return -1; 
	}
	return 0;
}

int Mosquitto_Loop_Stop() {
	// mosquitto_loop_stop 会等待 network_thread 结束（需调用 mosquitto_disconnect）
	// 第二个参数为 true，可强制结束 network_thread
	int loop = mosquitto_loop_stop(mosq, true);
	if(loop != MOSQ_ERR_SUCCESS) {
		XLOG_DEBUG("Mosquitto_Loop_Stop:{}", mosquitto_strerror(loop));
		return -1; 
	}
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return 0;
}

int Mosquitto_Pub(const char *topic, int payloadlen,const void *payload) {
	/*int mosquitto_publish(struct mosquitto *mosq,	
							int *mid,				//消息id
							const char *topic,		//消息主题
							int payloadlen,			//消息长度
							const void *payload,	//消息内容
							int qos,			//消息质量，
												  0-至多一次，deliver and forget
												  1-至少一次，
												  2-只有一次
							bool retain); 		//消息是否驻留(只有一条)
	*/
	int ret = mosquitto_publish(mosq, NULL, topic, payloadlen, payload, SIGNALS_QOS, false);
	if(MOSQ_ERR_SUCCESS != ret) {
		XLOG_DEBUG("Mosquitto_Pub:{}", mosquitto_strerror(ret));
		return -1;
	}
		
	return 0;
}