
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/vfs.h>

#include "tbox/Common.h"
#include "tbox/GSM.h"
#include "tbox/GPRS_iNet.h"
#include "tbox/CAN.h"
#include "tbox/GPS.h"
#include "tbox/Internal.h"
#include "Mosquitto/MOSQUITTO.h"
#include "Mosquitto/LoadJSON.h"

#include "DevConfig/ParseConfiguration.h"
#include "PublishSignals/PublishSignals.h"
#include "SaveFiles/SaveFiles.h"
#include "PowerManagement/PowerManagement.h"

#include "Logger/Logger.h"
#include "tbox/LED_Control.h"
#include "DevConfig/PhyChannel.hpp"

// #define TEST_LOG_DEBUG

sem_t simSem;

/* 其他文件需要用到，需使用 extern 显式说明， const 默认作用域为本文件 */
extern const std::string LOG_DIR_NAME = "/home/debian/log/";
const std::string deal_old_sh = "home/debian/deal_old_files.sh";

/* 配置文件 */
std::string config_abs_path = "/home/debian/WINDHILLRT/";
std::vector<std::string> vec_temp_config = {config_abs_path + "temp_hw.xml", config_abs_path + "temp_app.json", config_abs_path + "temp_info.json"};
std::vector<std::string> vec_config = {config_abs_path + "hw.xml", config_abs_path + "app.json", config_abs_path + "info.json"};
	// std::string config_temp_hw = config_abs_path + "temp_hw.xml";
	// std::string config_temp_app = config_abs_path + "temp_app.json";
	// std::string config_temp_info = config_abs_path + "temp_info.json";
	// std::string config_hw = config_abs_path + "hw.xml";
	// std::string config_app = config_abs_path + "app.json";
	// std::string config_info = config_abs_path + "info.json";

extern std::string STORGE_DIR_NAME;
extern int runGSMHandlerEvents;

extern Inside_MAP g_Inside_MAP;
extern Net_Info g_Net_Info;
extern Connection g_Connection;

void *Net_Active(void *arg)
{
	int retval = 0;
	int isActive = 0;
	unsigned char wGPRSRegistration = 0;
	char wRegistration = 0;
	
	while(runGSMHandlerEvents) {
		isActive = 0;
		usecsleep(1, 0);
		if (!runGSMHandlerEvents) break;
		// sem_wait(&simSem);

		iNet_IsActive(&isActive);		//Checks the correct working of the INET module.
		if (1 == isActive) continue;
		XLOG_WARN("Network interruption.");

		retval = GSM_GetRegistration(&wRegistration);
		if (NO_ERROR != retval) {	// Gets the current network status.
			XLOG_WARN("GSM_GetRegistration ERROR,{}.", Str_Error(retval));
			continue;
		}
		XLOG_INFO("GSM Network status {0.d}.", (int)wRegistration);
		if (NO_NETWORK == wRegistration) {
			XLOG_WARN("GSM Network interruption.");
			GSM_IsActive(&isActive);	//Checks if the GSM module is switched ON and active.
			if(1 == isActive) {
				XLOG_INFO("GSM module is still active, stop and reatsrt it.");
				gsmStop();
			}
			while (0 != gsmStart());
		}

		XLOG_INFO("Restart iNet module.");
		iNetStart(g_Connection.Modem_Conn.userName.c_str(), g_Connection.Modem_Conn.passwd.c_str(), g_Connection.Modem_Conn.APN.c_str());
	
	}
	XLOG_TRACE("Net_Active thread exit.");
	pthread_exit(NULL);
}

static inline int date_to_string(std::string& dataString) {

	time_t t = 0;
	int count = 0;
    while ((time_t)(-1) == time(&t) ) {
		// XLOG_DEBUG("Get timestamp in seconds ERROR.");
		count++;
		if (count > 3) {
			// XLOG_ERROR("Get timestamp in seconds ERROR.");
			break;	
		}
	}

    struct tm *ptm = localtime(&t);
    char strTime[100];
    strftime(strTime, sizeof(strTime), "%Y_%m_%d_%H_%M_%S", ptm);

	dataString = strTime;
	return 0;
}

static int check_sd() {
	int retval = -1;
	int count = 0;
	while (retval = access(STORGE_DIR_NAME.c_str(), R_OK || W_OK), retval && ++count) {
		sleep(1);
		if (count > 10) {
			break;
		}
	}
	return retval;
}

static int get_storage_info(const char* mountPoint, uint64_t *available_capactity, uint64_t *total_capactity) {
	struct statfs64 statfs_info;
	// uint64_t usedBytes = 0;
	uint64_t aviableBytes = 0;
	uint64_t totalBytes = 0;

	int retval = statfs64(mountPoint, &statfs_info);
	if (-1 == retval) {
		XLOG_DEBUG("statfs failed for path: {}. {}", mountPoint, strerror(errno));
		return -1;
	}

	totalBytes = statfs_info.f_blocks * statfs_info.f_frsize;
	aviableBytes = statfs_info.f_bavail * statfs_info.f_frsize;
	// usedBytes = totalBytes - freeBytes;

	*available_capactity = aviableBytes / 1024;
	*total_capactity = totalBytes / 1024;
	return 0;
}

int update_config(const char *temp_path, const char *target_path) {
	if (0 == access(temp_path, F_OK)) {
		int retval = rename(temp_path, target_path);
		if (0 != retval) {
			XLOG_DEBUG("Update {} ERROR, {}.", target_path, strerror(errno));
			return -1;
		} else {
			XLOG_INFO("Update {} success.", target_path);
		}
	}
	return 0;
}

int main()
{
	int retval = 0;
    
 	signal(SIGINT, handler_exit);
	signal(SIGUSR1, handler_exit);

	sem_init(&simSem,0,0);

	// RTU模块初始化
	retval = InitRTUModule();
	if (1 == retval) {
		DEVICE_STATUS_ERROR();
		// XLOG_CRITICAL("Problem with the system initialization.");
		return -1;
	} else if(-1 == retval) {
		DEVICE_STATUS_ERROR();
		// XLOG_CRITICAL("RTU module initialized ERROR.");
		return -1;
	} else {
		// XLOG_INFO("RTU module initialized successfully.");
	}
	
	//IO模块初始化
	if(0 != (retval = InitIOModule())) {
		DEVICE_STATUS_ERROR();
		// XLOG_CRITICAL("IO module initialized ERROR.");
		return -1;
	} else {
		// XLOG_INFO("IO module initialized successfully.");
	}

	// 日志文件初始化
	std::string strTime;
	date_to_string(strTime);
	std::string name = "log_" + strTime + ".log";
	XLogger::getInstance()->InitXLogger(LOG_DIR_NAME, name);

	XLOG_INFO("Device booting...");

	// 确保电源指示灯正常亮起
	do {
		unsigned char green_led_value = 0;
		if (NO_ERROR == DIGIO_Get_LED_SW1(&green_led_value)) {
			if (1 == green_led_value) {
				break;
			}
		}
		if(0 != (retval = DIGIO_Set_LED_SW1(1))) {
			XLOG_WARN("Power indicator failed.");
		}
	} while(0);

	// 检测 KL_15 是否上电
	unsigned char KL_15 = 2;
	int count = 0;
	while (retval = DIGIO_Get_DIN(DIGITAL_INPUT_0, &KL_15), !retval && !KL_15) {
		if (!count) {
			DEVICE_KL15_ERROR();
			XLOG_WARN("KL_15 is not powered on.");
		}
		
		count++;
		sleep(1);
		if (count >= 20) {
			break;
		}
	}

	if (count >= 20) {
		DEVICE_STATUS_ERROR();
		XLOG_CRITICAL("KL_15 ERROR. Enter STOP mode.");
		power_management_init(0);
	}

	// KL_15 已上电 
	// 设备状态指示灯 -- 开始启动
	DEVICE_STATUS_START();

	// 初始化电源管理
	if (0 != power_management_init(1)) {
		XLOG_ERROR("Power management initialized ERROR.");
	} else {
		XLOG_INFO("Power management initialized successfully.");
	}

	/* 更新配置文件 */
	for (uint8_t idx = 0; idx < vec_config.size(); idx++) {
		retval = update_config(vec_temp_config[idx].c_str(), vec_config[idx].c_str());
		if (0 != retval) {
			DEVICE_STATUS_ERROR();
			XLOG_CRITICAL("Update configuration ERROR.");
			return -1;
		}
	}
	/* parse configuration */
	if (0 != parse_configuration("/home/debian/WINDHILLRT/hw.xml", strTime) ) {
		DEVICE_STATUS_ERROR();
		XLOG_CRITICAL("Parse configuration file ERROR.");
		return -1;
	} else {
		XLOG_INFO("Parse configuration sucessfully.");
	}

	// 检测 SD 卡是否挂载成功
	retval = check_sd();
	if (retval) {
		DEVICE_STATUS_ERROR();
		XLOG_CRITICAL("Storage media execption: {}.", strerror(errno));
		// power_management_init(0);
		return -1;
	}
	// 剩余存储容量
	uint64_t availableSpace = 0;
	uint64_t totalSpace = 0;
	retval = get_storage_info(STORGE_DIR_NAME.c_str(), &availableSpace, &totalSpace);
	if (0 == retval) {
		XLOG_INFO("Free measurement space: {} / {} KB.", availableSpace, totalSpace);
		float avail = 1.0 * availableSpace / totalSpace;
		if ( avail < 0.05) {
			XLOG_WARN("Small storage space available.");
			// 报警
		} else if (avail <= 0) {
			DEVICE_STATUS_ERROR();
			XLOG_CRITICAL("Insufficient storage space.");
			// 对文件做处理
			return -1;
		}
	} else {
		XLOG_WARN("Get measurement space failed.");
	}

	// 处理旧文件(数据文件)
	if (0 == access(deal_old_sh.c_str(), X_OK)) {
		XLOG_DEBUG("Deal old files.");
		system(deal_old_sh.c_str());
    }

	/* GPS */
	if (g_Inside_MAP.count("GPS") && g_Inside_MAP["GPS"].isEnable) {
		retval = InitGPSModule();
		if (0 != retval) {
			XLOG_WARN("GPS moudle Initialization ERROR.");
		} else {
			XLOG_INFO("GPS module initialized successfully.");
		}
	}

	/* Internal signal */
	if (g_Inside_MAP.count("Internal") && g_Inside_MAP["Internal"].isEnable) {
		retval = Init_Internal();
		if (0 != retval) {
			XLOG_WARN("Internal moudle Initialization ERROR.");
		} else {
			XLOG_INFO("Internal module initialized successfully.");
		}
	}

	/* CAN */	
	if(0 != (retval = Can_Enable(ENABLE_CAN))) {
		DEVICE_STATUS_ERROR();
		XLOG_CRITICAL("CAN moudle Enable ERROR.");
		return -1;
	}
	Can_Start();	// 需要在解析完配置 xml 后再启动
	XLOG_INFO("Measurement start.");

	// 初始化发布线程
	publish_init();
	
	// 启动文件存储功能
	save_init();

	// TODO: 增加网络通信方式的配置文件
	// COMMUNICATION_MODE com_mode = COMMUNICATION_MODE::Modem;
	// if (com_mode == COMMUNICATION_MODE::Modem) {
	if (true == g_Connection.Modem_Conn.bActive) {
		XLOG_INFO("Waiting for GSM Initialization and Internet session(about 40s)...");
		
		// /* GSM */	
		if(-1 == gsmStart()) {
			XLOG_INFO("GSM moudle start ERROR! Retry...");
			if(-1 == gsmStart()) {
				XLOG_WARN("GSM start ERROR.");
			}
		}
		
		// /* iNet */
		iNetStart(g_Connection.Modem_Conn.userName.c_str(), g_Connection.Modem_Conn.passwd.c_str(), g_Connection.Modem_Conn.APN.c_str());
	}

	if (g_Connection.nWLAN) {
		system("/opt/TFM_scripts/WLAN_stop.sh");
		char start_WLAN[1000] = {0};
		sprintf(start_WLAN, "/opt/TFM_scripts/WLAN_start.sh %s %s", g_Connection.pWLAN_CONN->SSID.c_str(), g_Connection.pWLAN_CONN->passwd.c_str());

		// 多路 WLAN
/* 		sprintf(start_WLAN, "/opt/TFM_scripts/WLAN_start.sh %d ", g_Connection.nWLAN);

		for (int idx_WLAN = 0; idx_WLAN < g_Connection.nWLAN; idx_WLAN++) {
			std::strncat(start_WLAN, g_Connection.pWLAN_CONN->SSID.c_str(), g_Connection.pWLAN_CONN->SSID.length());
			std::strncat(start_WLAN, " ", std::strlen(" "));
			std::strncat(start_WLAN, g_Connection.pWLAN_CONN->passwd.c_str(), g_Connection.pWLAN_CONN->passwd.length());
			std::strncat(start_WLAN, " ", std::strlen(" "));
		} */

		system(start_WLAN);
	}

	/* mosquitto */
	if (g_Net_Info.mqtt_server.bIsActive == true) {

		do {
			// 读取 json 配置文件
			if (0 != load_all_json("/home/debian/WINDHILLRT") ) {
				XLOG_DEBUG("JSON ERROR.");
				break;
			}
			// MQTT 初始化
			if (-1 == Mosquitto_Init()) {
				XLOG_DEBUG("Mosquitto Init ERROR.");
				XLOG_WARN("Can't upload data to the server in real time.");
				break;
			}
			sleep(10); // 减少重连次数
			Mosquitto_Loop_Start();/*  */
			if(0 != Mosquitto_Connect()) {
				XLOG_WARN("Can't upload data to the server in real time.");
				break;
			}	
			
		} while(0);
	}

	// 设备指示灯 -- 正常
	DEVICE_STATUS_NORMAL();

	if (true == g_Connection.Modem_Conn.bActive) {
	/* reconnect thread */
		pthread_t network_detection_id;
		pthread_create(&network_detection_id, NULL, Net_Active, NULL);
		pthread_setname_np(network_detection_id, "net detection");
		// pthread_join(network_detection_id, NULL);
		pthread_detach(network_detection_id);
	}

#if 1
	// 创建 fifo，接收控制指令
	#define FIFO_REQ_PATH "/tmp/LoggerRequest"
	#define FIFO_RSP_PATH "/tmp/LoggerResponse"
	int ret = 0, r_fd = -1, w_fd = -1;
	ret = mkfifo(FIFO_REQ_PATH, 0666);
	if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
		// 日志
        return -1;
    }
	r_fd = open(FIFO_REQ_PATH, O_RDONLY);
	if (-1 == r_fd) {
        perror("open");
		// 日志
        return -1;
    }
/* 	ret = mkfifo(FIFO_RSP_PATH, 0666);
	if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
		// 日志
        return -1;
    }
	w_fd = open(FIFO_REQ_PATH, O_WRONLY);
	if (-1 == w_fd) {
        perror("open");
		// 日志
        return -1;
    } */

	typedef struct REQUEST_MSG {
		uint16_t m_act;
	} Req_Msg;

	typedef struct RESPONSE_MSG {
		uint16_t m_rsp;
	} Rsp_Msg;

	ssize_t read_bytes;
	while(1) {
		// sleep(1000);
		Req_Msg r_buf = {0};
		read_bytes = read(r_fd, &r_buf, sizeof(r_buf));
		if (read_bytes < 0) {
            perror("read");
            continue;
        }
		uint16_t act = r_buf.m_act;
		if (0x0002 == act) {
			close(r_fd);
			raise(SIGUSR1);
		}
	}
#else
	while (1) {
		sleep(10);
	}
#endif
	return 0;
}
