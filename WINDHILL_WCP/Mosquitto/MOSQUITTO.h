
#ifndef __MOSQUITTO_H
#define __MOSQUITTO_H

#include <mosquitto.h>
	
typedef enum MQTT_TOPIC {
	app_json,
	info_json,
	kpiDashboard,
	layout_json,
	limits,
	online,
	setChannelValue,
	signals,
	syslog,
	syslogRestart,
	uplinkUpdateInterval,
	uplinkVersion,
} MQTT_TOPIC;

	#define SIGNALS_QOS		0
	#define WILL_QOS		1
	#define ONLINE_QOS		1
	#define APP_JSON_QOS	1
	#define INFO_JSON_QOS	1
	#define UPLINK_QOS		1
	#define KEEP_ALIVE		20
	#define MSG_MAX_SIZE	512 

	#define MOSQUITTO_RECONNECT_COUUNT	10	
	
	void GetMQTTTopic(char *pTopic, MQTT_TOPIC topic_type);

	int Mosquitto_Init();
	int Mosquitto_Connect();
	int Mosquitto_Disconnect();
	int Mosquitto_Loop_Start();
	int Mosquitto_Loop_Stop();
	int Mosquitto_Pub(const char *topic, int payloadlen, const void *payload);
	
#endif