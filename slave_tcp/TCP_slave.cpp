/*************************************************************************
	> File Name: TCP_slave.cpp
	> Author: 
	> Mail: 
	> Created Time: Mon 08 Nov 2021 05:59:09 PM CST
 ************************************************************************/

#include "common.h"
#include "Package.h"
#include "file_opt.h"
#include "CTimer.h"
// #include "IPC.h"
#include "../Logger/Logger.h"

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/wait.h>

#include <vector>

// #define PC_TEST

#ifdef PC_TEST
const std::string DATA_PATH = "/home/windhill/Desktop/Remote_Recoder/slave_tcp";
const std::string USER_LOG_PATH = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/log";
const std::string TEMP_HW_XML = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/temp_hw.xml";
const std::string TEMP_APP_JSON = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/temp_app.json";
const std::string TEMP_INFO_JSON = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/temp_info.json";
const std::string REAL_HW_XML = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/hw.xml";
const std::string REAL_APP_JSON = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/app.json";
const std::string REAL_INFO_JSON = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/info.json";
const std::string REAL_LICENSE = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/config/license";
const std::string REAL_FIREWARE = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/fireware.img";
const std::string SYS_LOG_PATH = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/";
const std::string SYS_LOG_NAME = "sys.log";

const std::string START_SCRIPT = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/test.sh";
const std::string UPDATE_SCRIPT = "/home/windhill/Desktop/Remote_Recoder/slave_tcp/test_shell/update_fireware.sh";
#else
const std::string DATA_PATH = "/media/TFM_DATA";
const std::string USER_LOG_PATH = "/home/debian/log";
const std::string TEMP_HW_XML = "/home/debian/WINDHILLRT/temp_hw.xml";
const std::string TEMP_APP_JSON = "/home/debian/WINDHILLRT/temp_app.json";
const std::string TEMP_INFO_JSON = "/home/debian/WINDHILLRT/temp_info.json";
const std::string REAL_HW_XML = "/home/debian/WINDHILLRT/hw.xml";
const std::string REAL_APP_JSON = "/home/debian/WINDHILLRT/app.json";
const std::string REAL_INFO_JSON = "/home/debian/WINDHILLRT/info.json";
const std::string REAL_LICENSE = "/home/debian/WINDHILLRT/license";
const std::string REAL_FIREWARE = "/opt/DL_Fireware/Fireware.img";
const std::string SYS_LOG_PATH = "/opt/DL_Manager/";
const std::string SYS_LOG_NAME = "sys.log";

const std::string START_SCRIPT = "/home/debian/TBox.sh";
const std::string UPDATE_SCRIPT = "/opt/DL_Manager/update_fireware.sh";
#endif

typedef struct VERSION_INFO {
	uint8_t maj_version;
	uint8_t min_version;
	uint8_t revision_number;
	uint8_t reserved;
} VERSION_INFO;

#define FIFO_REQ_PATH "/tmp/LoggerRequest"
#define FIFO_RSP_PATH "/tmp/LoggerResponse"

typedef struct REQUEST_MSG {
	uint16_t m_act;
} Req_Msg;

typedef struct RESPONSE_MSG {
	uint16_t m_rsp;
} Rsp_Msg;

int r_fifo_fd = -1, w_fifo_fd = -1;

const uint8_t PRO_VERSION = 0x02;
#define HEADER_LENGTH	8
#define PORT			14500
#define LISTEN_BACKLOG 	1

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

extern std::vector<FileInfo> vec_file_info;

// Logger_Ctrl m_Logger_Ctrl;

uint8_t rcv_buf[BUFSIZ] = {0};
uint32_t buf_cnt = 0;
// 本机主机名/序列号
char serial[16] = {0};
#define SERIAL_BYTES	6
// 保存客户端相关信息
struct Client_Info {
	int cfd;
};
Client_Info t_client = {-1};

int my_system(const char *cmd) {

    int ret = 0;
    // 设置 SIGCHLD 为默认处理方式，并保存原本的处理方式
    __sighandler_t old_handler = signal(SIGCHLD, SIG_DFL);
    ret = system(cmd);
    // 恢复 SIGCHLD 的处理方式
    signal(SIGCHLD, old_handler);

    return ret;
}

void handler_request_timeout() {
	DEBUG_PTINTF("timeout handler.\n");
}

int DL_Error_NO = 0;
int set_error_no(int err_no) {
	DL_Error_NO = err_no;
	return 0;
}


#define TIMEZONE_FILE		"/etc/timezone"
#define	MAXLEN_TIMEZONE		30
int get_timezone(char *pTimezone) {
	if (NULL == pTimezone) {
		DEBUG_PTINTF("pTimezone NULL.\n");
		return 1;
	}
	int fd = open(TIMEZONE_FILE, O_RDONLY);
	if (fd < 0) {
		DEBUG_PTINTF("Timezone -- get ERROR, {%s}.\n", strerror(errno));
		return 2;
	}

	ssize_t read_bytes = read(fd, pTimezone, MAXLEN_TIMEZONE);
	if (read_bytes < 0) {
		DEBUG_PTINTF("Timezone -- get ERROR, {%s}.\n", strerror(errno));
		close(fd);
		return 3;
	}
	// 去掉 Linux 文件行末尾的换行符（'\n'）
	if (pTimezone[read_bytes - 1] == '\n') pTimezone[read_bytes - 1] = '\0';
	close(fd);
	DEBUG_PTINTF("Timezone -- get ok, {%s}.\n", pTimezone);
	
	return 0;
}

int set_timezone(const char *pTimezone, const int len) {
	char *tz_buf = (char *)malloc(sizeof(char) * (len + 1));
	if (NULL == tz_buf) {
		DEBUG_PTINTF("malloc tz_buf ERROR, %s.\n", strerror(errno));
		return 1;
	}

	memcpy(tz_buf, pTimezone, len);
	DEBUG_PTINTF("set timezone: {%s}\n", tz_buf);

	// 保存原时区
	char original_timezone[MAXLEN_TIMEZONE] = {0};
	int ret = get_timezone(original_timezone);
	if (0 != ret) {
		DEBUG_PTINTF("Timezone -- save orignal timezone ERROR.\n");
		free(tz_buf);
		return 3;
	}
	// 要设置的时区与系统时区相同
	if (0 == strcmp(tz_buf, original_timezone)) {
		free(tz_buf);
		return 0;
	}
	// 实际时区文件目录
	char share_tz_file[100] = "/usr/share/zoneinfo/";
	strcat(share_tz_file, tz_buf);
	DEBUG_PTINTF("share_tz_file: {%s}\n", share_tz_file);

	if (0 != access(share_tz_file, F_OK)) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s} is not a valid timezone.\n", tz_buf);
		free(tz_buf);
		return 2;
	}

	int fd = open(TIMEZONE_FILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (fd < 0) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
		free(tz_buf);
		return 4;
	}
	// 写入文件前添加换行符
	tz_buf[strlen(tz_buf)] = '\n';
	ssize_t write_bytes = write(fd, tz_buf, strlen(tz_buf));
	if (write_bytes < 0) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
		close(fd);
		free(tz_buf);
		return 5;
	}
	close(fd);
	
	ret = access("/etc/localtime", F_OK);
	if (0 == ret) {
		ret = remove("/etc/localtime");
		if (-1 == ret) {
			DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
			free(tz_buf);
			return 6;
		}
	}
	ret = symlink(share_tz_file, "/etc/localtime");
	if (-1 == ret) {
		DEBUG_PTINTF("Timezone -- set ERROR, {%s}.\n", strerror(errno));
		free(tz_buf);
		// 恢复修改前时区
		return 7;
	}
	
	tzset();	// 强制刷新时区
#ifdef __DEBUG
	tz_buf[strlen(tz_buf) - 1] = '\0';	// 方便调试打印
#endif
	DEBUG_PTINTF("Timezone -- set {%s} ok.\n", tz_buf);
	free(tz_buf);
	return 0;
}

int get_systime(struct tm *get_tm) {
	time_t t, ret_t;
	struct tm *ret_tm;
	ret_t = time(&t);
	if ((time_t)-1 == ret_t) {
		DEBUG_PTINTF("Time -- get ERROR, {%s}.\n", strerror(errno));
		return 1;
	}
	tzset();	// 强制刷新时区
	ret_tm = localtime_r(&t, get_tm);
	if (NULL == ret_tm) {
		XLOG_ERROR("Time -- get ERROR, {%s}.\n", strerror(errno));
		return 2;
	}
	return 0;
}

int set_systime(struct tm set_tm) {
	time_t set_time = mktime(&set_tm);
	if ((time_t)-1 == set_time) {
		DEBUG_PTINTF("Time -- set ERROR, {%s}.\n", strerror(errno));
		return 1;
	}

	struct timeval set_tv;
	set_tv.tv_sec = set_time;
	set_tv.tv_usec = 0;

	int ret = settimeofday(&set_tv, NULL);
	if (ret == -1) {
		DEBUG_PTINTF("Time -- set ERROR, {%s}.\n", strerror(errno));
		if (errno == EINVAL) {
			return 2;
		} 
		return 3;
	} 
	DEBUG_PTINTF("Time -- set ok, %d-%02d-%02d %02d:%02d:%02d\n", 
			set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday, set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);
	
	return 0;
}

char *create_rsp_general(uint8_t pro_version, CMD_ID cmd_id, uint32_t len_ex_rsp_id, RESPONSE_ID rsp_id, char *response) {
	
	char *r = response;
	if (NULL != response) {
		Package_Header header;
		memset(&header, 0, sizeof(header));
		header.u8_pro_version = pro_version;
		header.u8_inv_version = ~pro_version;
		header.u16_cmd_id = (CMD_ID)htons(cmd_id);
		header.u32_data_len = htonl(sizeof(RESPONSE_ID) + len_ex_rsp_id);
		memcpy(response, &header, HEADER_LENGTH);
		uint16_t rsp = htons(rsp_id);
		memcpy(response + HEADER_LENGTH, &rsp, sizeof(rsp));
	}
	return r;
}

ssize_t recvn(const int fd, void *buf, size_t count) {
	size_t n_left = count;
	ssize_t n_recv = 0;
	char *p_buf = (char *)buf;

	while (n_left > 0) {
		n_recv = recv(fd, p_buf, n_left, 0);
		if (n_recv < 0) {
			// 被信号打断
			if (errno == EINTR) {
				continue;
			} else {
				return -1;
			}
		} else if (n_recv == 0) {
			return (count - n_left);	// 若不为 count，则说明接收不正确
		} else {
			n_left -= n_recv;
			p_buf += n_recv;
		}
	}

	return count;
}

ssize_t sendn(const int fd, const char *buf, size_t count) {
	size_t n_left = count;
	ssize_t n_send = 0;
	char *p_buf = (char *)buf;

	while (n_left > 0) {
		n_send = send(fd, p_buf, n_left, 0);
		if (n_send < 0) {
			// 被信号打断
			if (errno == EINTR) {
				continue;
			} else {
				return -1;
			}
		} else if (n_send == 0) {
			continue;	// 没有写入，可以继续
		} else {
			n_left -= n_send;
			p_buf += n_send;
		}
	}

	return count;
}

// recv 线程
void *recv_thread(void *arg) {

	DEBUG_PTINTF("new client connected.\n");

	int client_sock_fd = *(int *)&arg;
	ssize_t rcv_len = 0;
	// 设置定时器
	CTimer my_timer([=]() {handler_request_timeout();} );
	CTimer::Clock::duration normal_timeout = std::chrono::milliseconds(100);

	while (1) {
		rcv_len = recv(client_sock_fd, rcv_buf, sizeof(rcv_buf), 0);
		if (rcv_len == 0 ) {		// 通信中断或客户端退出
			DEBUG_PTINTF("client disconnected.\n");
			close(client_sock_fd);
			break;
		} else if (rcv_len < 0) {	// 出错
			perror("recv");
			close(client_sock_fd);
			break;
		}
		
		if (rcv_len >= HEADER_LENGTH) {

#ifdef __DEBUG
			for (int i = 0; i < rcv_len; i++) {
				printf("%02x ", rcv_buf[i]);
			}
			printf("\n");
#endif
			Package_Header pack_header;
			memset(&pack_header, 0, sizeof(pack_header));
			memcpy(&pack_header, rcv_buf, HEADER_LENGTH);
			// 获取版本号
			uint8_t pro_ver = pack_header.u8_pro_version;
			uint8_t inv_ver = pack_header.u8_inv_version;
			DEBUG_PTINTF("pro_ver : %02X, inv_ver: %02X.\n", pro_ver, inv_ver);
			if ((pro_ver != PRO_VERSION) || ((pro_ver ^ inv_ver) != 0xFF) ) {
				XLOG_ERROR("Protocol version error.");
				DEBUG_PTINTF("Protocol version error.\n");
				continue;
			}
			// 获取命令
			CMD_ID cmd_id = (CMD_ID)ntohs(pack_header.u16_cmd_id);
			DEBUG_PTINTF("cmd_id : %04X.\n", cmd_id);

			if ((cmd_id <= CMD_ID::MIN_CMD_ID) || (cmd_id >= CMD_ID::MAX_CMD_ID) ) {
				XLOG_ERROR("Unknown commad: {:X}(h).", cmd_id);
				DEBUG_PTINTF("unknown commad.\n");
				char response[10] = {0};
				create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_unknown_cmd, response);
				send(client_sock_fd, response, sizeof(response), 0);
				continue;
			}
			
			// 取出负载长度
			uint32_t payload_len = ntohl(pack_header.u32_data_len);
			DEBUG_PTINTF("payload_len = %u\n", payload_len);
			int32_t n_rcv_payload = rcv_len - HEADER_LENGTH;
			int32_t n_left_payload = payload_len - n_rcv_payload;
			DEBUG_PTINTF("n_left_payload = %d\n", n_left_payload);

			CTimer::Clock::duration recv_timeout_ms;
			// 接收完整包
			if (cmd_id != CMD_ID::Download) {
				recv_timeout_ms = normal_timeout;
				my_timer.StartOnce(recv_timeout_ms);
				while ((payload_len > n_rcv_payload) && my_timer.IsRunning()) {
					ssize_t temp_len = recv(client_sock_fd, rcv_buf + rcv_len, payload_len - n_rcv_payload, MSG_DONTWAIT);
					if (temp_len < 0) {
						if ( (errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINTR) ) {
							continue;
						}
						break;
					}
					if (temp_len == 0) {
						break;
					}
					rcv_len += temp_len;
					n_rcv_payload += temp_len;
				}
				if (payload_len > n_rcv_payload) {
					XLOG_ERROR("recv timeout, cmd: {0:X}(h) {1:.0f} ms.", cmd_id, recv_timeout_ms.count() / 1e+6);
					DEBUG_PTINTF("timeout!!!!\n");
					char response[10] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(client_sock_fd, response, sizeof(response), 0);
					continue;
				} else {
					my_timer.Stop();
				}
			} else {
				// 每 1M 超时为 500 ms
				uint64_t ms_count = (payload_len / (1024 * 1024 * 1) + 1) * 500;
				recv_timeout_ms = std::chrono::milliseconds(ms_count);
			}

			/* 获取系统信息 */
			if (cmd_id == CMD_ID::Get_Sys_Info) {
				DEBUG_PTINTF("Get system information.\n");
				char response[38] = {0};
				// 获取序列号
				int ret = gethostname(serial, sizeof(serial));
				if (-1 == ret) {
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					XLOG_ERROR("Sys info -- get serial ERROR, {}.", strerror(errno));
					continue;
				}
				DEBUG_PTINTF("serrial number: %s\n", serial);
				// TODO:版本号
				// 获取系统版本
				VERSION_INFO sys_version = {9, 6, 0, 0};
				// ret = get_debian_sysversion(&sys_version);
				// if (-1 == ret) {
				// 	create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
				// 	send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
				// 	XLOG_ERROR("Sys info -- get serial ERROR, {}.", strerror(errno));
				// 	continue;
				// }
				// 采集程序版本号
				VERSION_INFO wcp_version = {2, 0, 0, 0};
				// 管理程序版本号
				VERSION_INFO tcp_version = {2, 0, 0, 0};

				// 获取成功
				int idx = HEADER_LENGTH + sizeof(RESPONSE_ID);
				create_rsp_general(PRO_VERSION, cmd_id, 28, RSP_ID::rsp_success, response);
				memcpy(response + idx, serial, SERIAL_BYTES);
				idx += sizeof(serial);
				memcpy(response + idx, &sys_version, sizeof(VERSION_INFO));
				idx += sizeof(VERSION_INFO);
				memcpy(response + idx, &wcp_version, sizeof(VERSION_INFO));
				idx += sizeof(VERSION_INFO);
				memcpy(response + idx, &tcp_version, sizeof(VERSION_INFO));
				idx += sizeof(VERSION_INFO);
				send(client_sock_fd, response, sizeof(response), 0);
				XLOG_INFO("Sys info -- get ok.");

			/* 下载 */
			} else if (cmd_id == CMD_ID::Download) {
				char response[10] = {0};
				int dfd = -1;
				DOWNLOAD_TYPE d_type = (DOWNLOAD_TYPE)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);

				if (d_type == DOWNLOAD_TYPE::dl_xml_hw) {
					dfd = open(TEMP_HW_XML.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
				} else if (d_type == DOWNLOAD_TYPE::dl_json_app) {
					dfd = open(TEMP_APP_JSON.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
				} else if (d_type == DOWNLOAD_TYPE::dl_json_info) {
					dfd = open(TEMP_INFO_JSON.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
				} else if (d_type == DOWNLOAD_TYPE::dl_fireware) {
					dfd = open(REAL_FIREWARE.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
				} else if (d_type == DOWNLOAD_TYPE::dl_license) {
					// TODO
					dfd = open(REAL_LICENSE.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
				} else {
					// 回复下载类型不支持
					XLOG_ERROR("Download -- unsupported type: {:X}(h).", d_type);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_unsupported_dl_type, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				
				if (dfd < 0) {
					// 回复错误
					XLOG_ERROR("Download -- open file ERROR. {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				} 
				
				// 先写入部分，减去 下载类型（2 byte）
				write(dfd, rcv_buf + HEADER_LENGTH + 2, n_rcv_payload - 2);
				my_timer.StartOnce(recv_timeout_ms);
				while (n_left_payload > 0 && my_timer.IsRunning()) {
					char buffer[1024] = {0};
					int num_recv = recv(t_client.cfd, buffer, sizeof(buffer), MSG_DONTWAIT);
					if (num_recv < 0) {
						if ( (errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == EINTR) ) {
							continue;
						}
						break;
					}
					if (num_recv == 0) {
						break;
					}
					
					// 写文件
					ssize_t bytes_to_file = write(dfd, buffer, num_recv);
					if (bytes_to_file < 0) {
						XLOG_INFO("Download -- write file ERROR, {}.", strerror(errno));
						close(dfd);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						break;
					}
					n_left_payload -= num_recv;
				}

				// 根据下载类型确定要操作的文件
				int is_configuration_file = true;
				char temp_file_path[256] = {0};
				if (d_type == DOWNLOAD_TYPE::dl_xml_hw) {
					strcpy(temp_file_path, TEMP_HW_XML.c_str());
				} else if (d_type == DOWNLOAD_TYPE::dl_json_app) {
					strcpy(temp_file_path, TEMP_APP_JSON.c_str());
				} else if (d_type == DOWNLOAD_TYPE::dl_json_info) {
					strcpy(temp_file_path, TEMP_INFO_JSON.c_str());
				} else if (d_type == DOWNLOAD_TYPE::dl_license) {
					strcpy(temp_file_path, REAL_LICENSE.c_str());
					is_configuration_file = false;
				} else if (d_type == DOWNLOAD_TYPE::dl_fireware) {
					strcpy(temp_file_path, REAL_FIREWARE.c_str());
					is_configuration_file = false;
				}

				// 超时，撤销已经执行的操作
				if (n_left_payload > 0) {
					XLOG_ERROR("Download -- timeout, {:.0f} ms.", recv_timeout_ms.count() / 1e+6);
					DEBUG_PTINTF("download timeout.\n");
					close(dfd);
					// 删除临时文件
					if (is_configuration_file = true) {
						if (0 == access(TEMP_HW_XML.c_str(), F_OK)) remove(TEMP_HW_XML.c_str());
						if (0 == access(TEMP_APP_JSON.c_str(), F_OK)) remove(TEMP_APP_JSON.c_str());
						if (0 == access(TEMP_INFO_JSON.c_str(), F_OK)) remove(TEMP_INFO_JSON.c_str());
					}
					
					// 回复超时
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				} else {
					my_timer.Stop();
					close(dfd);
					// 比较文件长度
					struct stat stat_buf;
					DEBUG_PTINTF("temp_file_path: %s\n", temp_file_path);
					int ret = stat(temp_file_path, &stat_buf);
					if (-1 == ret) {
						DEBUG_PTINTF("stat error, %s.\n", strerror(errno));
						XLOG_ERROR("Download -- get temp file {} size ERROR, {}.", temp_file_path, strerror(errno));
						// 获取长度失败，直接删除临时文件（temp_hw.xml, temp_app.json, temp_info.json）
						if (is_configuration_file = true) {
							if (0 == access(TEMP_HW_XML.c_str(), F_OK)) remove(TEMP_HW_XML.c_str());
							if (0 == access(TEMP_APP_JSON.c_str(), F_OK)) remove(TEMP_APP_JSON.c_str());
							if (0 == access(TEMP_INFO_JSON.c_str(), F_OK)) remove(TEMP_INFO_JSON.c_str());
						}

						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					// 比较文件大小
					if (payload_len - 2 == stat_buf.st_size) {
						XLOG_INFO("Download -- file {:d} ok.", d_type);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
					} else {
						// 回复文件长度不对
						// remove(temp_file_path);
						XLOG_ERROR("Download -- file length ERROR.");
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_payload_len_error, response);
					}
					// 给出响应
					send(t_client.cfd, response, sizeof(response), 0);
				}
			/* 上传数据文件 */
			} else if (cmd_id == CMD_ID::Upload_Data_File) {
				int ufd = -1;
				char response[10] = {0};
				uint16_t file_no = ((uint16_t)rcv_buf[8] << 8) | rcv_buf[9];
				DEBUG_PTINTF("file_no = %u\n", file_no);
				if (file_no >= vec_file_info.size()) {
					// 回复文件不存在
					XLOG_ERROR("Upload -- the date file {:d} does not exist.", file_no);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				DEBUG_PTINTF("file name : %s\n", vec_file_info[file_no].file_name.c_str());
				ufd = open(vec_file_info[file_no].file_name.c_str(), O_RDONLY);
				if (ufd < 0) {
					// 回复失败
					XLOG_ERROR("Upload -- open data file ERROR, {}", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				} 
				// 数据文件存在
				create_rsp_general(PRO_VERSION, cmd_id, vec_file_info[file_no].file_size, RSP_ID::rsp_success, response);
				send(t_client.cfd, response, sizeof(response), 0);
				// 发送文件内容
				ssize_t total_send_bytes = 0;
				while (1) {
					char snd_buf[1024] = {0};
					ssize_t num_read = read(ufd, snd_buf, sizeof(snd_buf));
					if (num_read < 0) {
						XLOG_ERROR("Upload -- read data file ERROR, {}.", strerror(errno));
						DEBUG_PTINTF("upload read file.\n");
						break;
					}
					if (send(t_client.cfd, snd_buf, num_read, 0) < 0) {
						XLOG_ERROR("Upload -- send data file ERROR, {}.", strerror(errno));
						DEBUG_PTINTF("upload send.\n");
						// exit(1);
						break;
					}
					if (num_read == 0) {
						break;
					}
					total_send_bytes += num_read;
				}
				close(ufd);
				if (total_send_bytes != vec_file_info[file_no].file_size) {
					XLOG_ERROR("Upload -- data file size ERROR.");
				} else {
					XLOG_INFO("Upload -- data file ok.");
				}
			/* 上传其他文件 */
			} else if (cmd_id == CMD_ID::Upload_Regualr_File) {
				char response[10] = {0};
				int ufd = -1;
				char file_name[256] = {0};
				UPLOAD_TYPE u_type = (UPLOAD_TYPE)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				if (u_type == UPLOAD_TYPE::ul_xml_hw) {
					strcpy(file_name, "Hardware.xml");
					ufd = open(REAL_HW_XML.c_str(), O_RDONLY);
				} else if (u_type == UPLOAD_TYPE::ul_user_log) {
					// TODO
					strcpy(file_name, "user.log");
					if (0 == get_all_file_info(USER_LOG_PATH.c_str(), "log")) {
						if (vec_file_info.size() == 0) {
							// 不存在 user log 文件
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
							send(t_client.cfd, response, sizeof(response), 0);
							XLOG_ERROR("Upload -- user log does not exist.");
							continue;
						}
					} else {
						// 获取 user log 文件信息失败
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						XLOG_ERROR("Upload -- get user log ERROR, {}.", strerror(errno));
						continue;
					}
					// 上传最新的 log 文件
					ufd = open(vec_file_info[vec_file_info.size() - 1].file_name.c_str(), O_RDONLY);
				} else if (u_type == UPLOAD_TYPE::ul_sys_log) {
					// TODO
					strcpy(file_name, "sys.log");
					ufd = open((SYS_LOG_PATH + SYS_LOG_NAME).c_str(), O_RDONLY);
				} else if (u_type == UPLOAD_TYPE::ul_license) {
					strcpy(file_name, "license");
					ufd = open(REAL_LICENSE.c_str(), O_RDONLY);
				} else {
					// 回复上传类型不支持
					XLOG_ERROR("Upload -- unsupported file type.");
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_unsupported_ul_type, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				
				if (ufd < 0) {
					// 回复错误，打开文件失败
					XLOG_ERROR("Upload -- open file {} ERROR, {}.", file_name, strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				//
				struct stat stat_buf;
				int ret = fstat(ufd, &stat_buf);
				if (ret == -1) {
					DEBUG_PTINTF("fstat error.\n");
					close(ufd);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}

				create_rsp_general(PRO_VERSION, cmd_id, stat_buf.st_size, RSP_ID::rsp_success, response);
				send(t_client.cfd, response, sizeof(response), 0);
				// 发送文件内容
				ssize_t total_send_bytes = 0;
				while (1) {
					char snd_buf[1024] = {0};
					int num_read = read(ufd, snd_buf, sizeof(snd_buf));
					if (num_read < 0) {
						XLOG_ERROR("Upload -- read file {} ERROR, {}.", file_name, strerror(errno));
						DEBUG_PTINTF("upload read file.\n");
						break;
					}
					if (num_read == 0) {
						break;
					}
					if (send(t_client.cfd, snd_buf, num_read, 0) < 0) {
						XLOG_ERROR("Upload -- send file {} ERROR, {}.", file_name, strerror(errno));
						DEBUG_PTINTF("upload send.\n");
						break;
					}
					total_send_bytes += num_read;
				}
				close(ufd);
				if (total_send_bytes != stat_buf.st_size) {
					XLOG_ERROR("Upload -- file size ERROR.");
				} else {
					XLOG_INFO("Upload -- file {} ok.", file_name);
				}
			/* 获取数据文件信息 */
			} else if (cmd_id == CMD_ID::Get_Data_File_Info) {
				if (0 == get_all_file_info(DATA_PATH.c_str(), "owa")) {
					// 正响应，并给出具体信息
					DEBUG_PTINTF("get data files info ok.\n");
					size_t file_cnt = vec_file_info.size();
					size_t file_info_bytes = 14 * file_cnt;
					size_t payload_bytes = file_info_bytes + 2;
					size_t num_bytes = HEADER_LENGTH + payload_bytes;
					char *response = (char *)malloc(num_bytes);

					create_rsp_general(PRO_VERSION, cmd_id, file_info_bytes, RSP_ID::rsp_success, response);
					// 文件信息
					size_t idx = HEADER_LENGTH + sizeof(RESPONSE_ID);
					for (uint16_t i = 0; i < file_cnt; i++) {
						uint16_t file_no = htons(i);
						memcpy(response + idx, &file_no, sizeof(file_no));
						idx += sizeof(file_no);
						
						uint32_t file_size = htonl(vec_file_info[i].file_size);
						memcpy(response + idx, &file_size, sizeof(file_size));
						idx += sizeof(file_size);

						uint64_t file_timestamp = my_hton64(vec_file_info[i].mod_time);
						memcpy(response + idx, &file_timestamp, sizeof(file_timestamp));
						idx += sizeof(file_timestamp);
					}

					XLOG_INFO("Get info -- get data files info ok.");
					send(t_client.cfd, response, num_bytes, 0);
					free(response);
				} else {
					// 负响应，附加错误码
					DEBUG_PTINTF("get data file info error. %s.\n", strerror(errno));
					XLOG_ERROR("Get info -- get data file info ERROR. {}.", strerror(errno));
					char response[10] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
				}
			/* 删除数据文件 */
			} else if (cmd_id == CMD_ID::Delete_Data_File) {
				char response[10] = {0};
				// my_timer.StartOnce(normal_timeout);
				DEBUG_PTINTF("delete file.\n");
				uint16_t file_no = ((uint16_t)rcv_buf[8] << 8) | rcv_buf[9];
				if (0 == delete_file((unsigned long)file_no)) {
					// 回复完成
					XLOG_INFO("Delete -- data file ok.");
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
					DEBUG_PTINTF("delete ok.\n");
				} else {
					// 回复失败
					XLOG_ERROR("Detele -- data file {:d} ERROR, {}.", file_no, strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					DEBUG_PTINTF("delete error.\n");
				}
				send(t_client.cfd, response, sizeof(response), 0);
			/* 记录仪动作 */
			} else if (cmd_id == CMD_ID::Logger_Action) {
				char response[10] = {0};
				LOGGER_ACTION action = (LOGGER_ACTION)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				if (action == LOGGER_ACTION::act_start) {
					if (0 != access(START_SCRIPT.c_str(), X_OK)) {
						XLOG_ERROR("Action -- start ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					int status = my_system(START_SCRIPT.c_str());
					// 判断返回值
					if (-1 == status) {
						XLOG_ERROR("Action -- start ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					} else {
						if (true == WIFEXITED(status)) {    // 正常退出
							if (0 == WEXITSTATUS(status)) {
								XLOG_INFO("Action -- start ok.");
								DEBUG_PTINTF("Action -- start ok.\n");
							} else {
								XLOG_ERROR("Action -- start ERROR, status: {:d}.", WEXITSTATUS(status));
								DEBUG_PTINTF("run shell script fail, exit code: {%d}.\n", WEXITSTATUS(status));
								create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
								send(t_client.cfd, response, sizeof(response), 0);
								continue;
							}
						} else {
							XLOG_ERROR("Action -- start ERROR, signal: {:d}.", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
							DEBUG_PTINTF("run shell script fail, exit code: {%d}.\n", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
							send(t_client.cfd, response, sizeof(response), 0);
							continue;
						}
					}
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
				} else if (action == LOGGER_ACTION::act_stop) {
					// 进程间通信
					Req_Msg w_buf = {0};
					w_buf.m_act = action;
					ssize_t write_bytes = write(w_fifo_fd, &w_buf, sizeof(w_buf));
					if (write_bytes < 0) {
						XLOG_ERROR("Action -- stop ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					} else {
						// TODO:等待响应
						XLOG_INFO("Action -- stop ok.");
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
					}
				} else {
					// 回复失败
					XLOG_ERROR("Action -- unsupported action: {:X}(h).", action);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					DEBUG_PTINTF("unknown action.\n");
				}
				send(t_client.cfd, response, sizeof(response), 0);
			/* 获取/设置系统时间 */
			} else if (cmd_id == CMD_ID::Method_System_Time) {
				METHOD method = (METHOD)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				if (method == METHOD::get) {
					char response[17] = {0};
#if 1
					struct tm get_tm;
					RESPONSE_ID rsp_id = RSP_ID::rsp_success;
					int ret = get_systime(&get_tm);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_fail;
						XLOG_ERROR("Time -- get ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
						send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					} else {
						create_rsp_general(PRO_VERSION, cmd_id, 7, RSP_ID::rsp_success, response);
						int idx = 10;
						uint16_t year = htons((uint16_t)(get_tm.tm_year) + 1900);
						memcpy(response + idx, &year, sizeof(year));
						idx += sizeof(year);
						uint8_t month = (uint8_t)(get_tm.tm_mon + 1);
						memcpy(response + idx, &month, sizeof(month));
						idx += sizeof(month);
						uint8_t day = (uint8_t)(get_tm.tm_mday);
						memcpy(response + idx, &day, sizeof(day));
						idx += sizeof(day);
						uint8_t hours = (uint8_t)(get_tm.tm_hour);
						memcpy(response + idx, &hours, sizeof(hours));
						idx += sizeof(hours);
						uint8_t minutes = (uint8_t)(get_tm.tm_min);
						memcpy(response + idx, &minutes, sizeof(minutes));
						idx += sizeof(minutes);
						uint8_t seconds = (uint8_t)(get_tm.tm_sec);
						memcpy(response + idx, &seconds, sizeof(seconds));
						idx += sizeof(seconds);
						XLOG_INFO("Time -- get ok, {:d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", ntohs(year), month, day, hours, minutes, seconds);
						send(t_client.cfd, response, sizeof(response), 0);
					}
#else
					time_t t, ret_t;
					struct tm get_tm, *ret_tm;
					ret_t = time(&t);
					if ((time_t)-1 == ret_t) {
						XLOG_ERROR("Time -- get ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
						continue;
					}
					tzset();	// 强制刷新时区
					ret_tm = localtime_r(&t, &get_tm);
					if (NULL == ret_tm) {
						XLOG_ERROR("Time -- get ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
						continue;
					}

					create_rsp_general(PRO_VERSION, cmd_id, 7, RSP_ID::rsp_success, response);
					int idx = 10;
					uint16_t year = htons((uint16_t)(get_tm.tm_year) + 1900);
					memcpy(response + idx, &year, sizeof(year));
					idx += sizeof(year);
					uint8_t month = (uint8_t)(get_tm.tm_mon + 1);
					memcpy(response + idx, &month, sizeof(month));
					idx += sizeof(month);
					uint8_t day = (uint8_t)(get_tm.tm_mday);
					memcpy(response + idx, &day, sizeof(day));
					idx += sizeof(day);
					uint8_t hours = (uint8_t)(get_tm.tm_hour);
					memcpy(response + idx, &hours, sizeof(hours));
					idx += sizeof(hours);
					uint8_t minutes = (uint8_t)(get_tm.tm_min);
					memcpy(response + idx, &minutes, sizeof(minutes));
					idx += sizeof(minutes);
					uint8_t seconds = (uint8_t)(get_tm.tm_sec);
					memcpy(response + idx, &seconds, sizeof(seconds));
					idx += sizeof(seconds);
					XLOG_INFO("Time -- get ok, {:d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", ntohs(year), month, day, hours, minutes, seconds);
					DEBUG_PTINTF("%u-%02u-%02u %02u:%02u:%02u\n", ntohs(year), month, day, hours, minutes, seconds);
					send(t_client.cfd, response, sizeof(response), 0);
#endif
				} else if (method == METHOD::set) {
					char response[10] = {0};
					RESPONSE_ID rsp_id = RSP_ID::rsp_success;
					// 判断该包的负载长度是否正确
					if (payload_len != 0x09) {
						rsp_id = RSP_ID::rsp_payload_len_error;
						create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					struct tm set_tm;
					memset(&set_tm, 0, sizeof(set_tm));
					set_tm.tm_year = (((uint16_t)rcv_buf[10] << 8) | rcv_buf[11]) - 1900;
					set_tm.tm_mon = rcv_buf[12] - 1;
					set_tm.tm_mday = rcv_buf[13];
					set_tm.tm_hour = rcv_buf[14];
					set_tm.tm_min = rcv_buf[15];
					set_tm.tm_sec = rcv_buf[16];
#if 1
					int ret = set_systime(set_tm);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_fail;
						XLOG_ERROR("Time -- set ERROR, {}.", strerror(errno));
					} else {
						XLOG_INFO("Time -- set ok, {:d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", 
								set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday, set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);
					}

					create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
					send(t_client.cfd, response, sizeof(response), 0);
	#ifndef PC_TEST
					system("sysclktohw");
	#endif
#else
					time_t set_time = mktime(&set_tm);
					if ((time_t)-1 == set_time) {
						XLOG_ERROR("Time -- set ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}

					struct timeval set_tv;
					set_tv.tv_sec = set_time;
					set_tv.tv_usec = 0;
					int ret = settimeofday(&set_tv, NULL);
					
					if (ret == -1) {
						if (errno == EINVAL) {
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_invalid_time, response);
						} else {
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						}
						
						XLOG_ERROR("Time -- set ERROR, {}.", strerror(errno));
					} else {
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
						XLOG_INFO("Time -- set ok, {:d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", 
								set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday, set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);
					}
					send(t_client.cfd, response, sizeof(response), 0);
	#ifndef PC_TEST
					system("sysclktohw");
	#endif
#endif

				} else {
					// 回复失败
					XLOG_ERROR("Time -- unknown method: {:X}(h).", method);
					char response[10] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					DEBUG_PTINTF("unknown method.\n");
				}
				
			/* 获取/设置系统时区 */
			} else if (cmd_id == CMD_ID::Method_TZ) {
				const char *TZ_FILE = "/etc/timezone";
				METHOD method = (METHOD)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				if (method == METHOD::get) {
					char response[10 + MAXLEN_TIMEZONE] = {0};
					char tz_buf[MAXLEN_TIMEZONE] = {0};
#if 1
					RESPONSE_ID rsp_id = RSP_ID::rsp_success;
					int ret = get_timezone(tz_buf);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_fail;
						XLOG_ERROR("Timezone -- get ERROR, {}.", strerror(errno));
					} else {
						XLOG_INFO("Timezone -- get ok, {}.", tz_buf); 
					}
					create_rsp_general(PRO_VERSION, cmd_id, strlen(tz_buf), rsp_id, response);
					memcpy(response + 10, tz_buf, strlen(tz_buf));
					send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID) + strlen(tz_buf), 0);
#else
					int fd = open(TZ_FILE, O_RDONLY);
					if (fd < 0) {
						XLOG_ERROR("Timezone -- get ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
						continue;
					}
					
					ssize_t read_bytes = read(fd, tz_buf, sizeof(tz_buf));
					if (read_bytes < 0) {
						XLOG_ERROR("Timezone -- get ERROR, {}.", strerror(errno));
						close(fd);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, HEADER_LENGTH + 2, 0);
						continue;
					}
					// 去掉 Linux 文件行末尾的换行符（'\n'）
					if (tz_buf[strlen(tz_buf) - 1] == '\n') tz_buf[strlen(tz_buf) - 1] = '\0';
					DEBUG_PTINTF("get timezone: {%s}\n", tz_buf);
					close(fd);
					XLOG_INFO("Timezone -- get ok, {}.", tz_buf); 
					create_rsp_general(PRO_VERSION, cmd_id, strlen(tz_buf), RSP_ID::rsp_success, response);
					memcpy(response + 10, tz_buf, strlen(tz_buf));
					send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID) + strlen(tz_buf), 0);
#endif

				} else if (method == METHOD::set) {
					char tz_buf[MAXLEN_TIMEZONE] = {0};
					char tz_string_to_file[MAXLEN_TIMEZONE] = {0};
					char response[10] = {0};
#if 1
					RESPONSE_ID rsp_id = RSP_ID::rsp_success;
					memcpy(tz_buf, rcv_buf + HEADER_LENGTH + sizeof(RESPONSE_ID), n_rcv_payload - sizeof(RESPONSE_ID));
					int ret = set_timezone(tz_buf, strlen(tz_buf));
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_fail;
						XLOG_INFO("Timezone -- set ERROR, {}.", strerror(errno));
					} else {
						XLOG_INFO("Timezone -- set {} ok.", tz_buf);
					}
					create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
					send(t_client.cfd, response, sizeof(response), 0);
#else
					memcpy(tz_buf, rcv_buf + HEADER_LENGTH + sizeof(RESPONSE_ID), n_rcv_payload - sizeof(RESPONSE_ID));
					DEBUG_PTINTF("set timezone: {%s}\n", tz_buf);
					// 实际时区文件目录
					char share_tz_file[100] = "/usr/share/zoneinfo/";
					strcat(share_tz_file, tz_buf);
					DEBUG_PTINTF("share_tz_file: {%s}\n", share_tz_file);

					if (0 != access(share_tz_file, F_OK)) {
						XLOG_INFO("Timezone -- set ERROR, {} is not a valid timezone.", tz_buf);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_invalid_timezone, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}

					int fd = open(TZ_FILE, O_CREAT | O_RDWR | O_TRUNC, 0644);
					if (fd < 0) {
						XLOG_INFO("Timezone -- set ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					// 写入文件前添加换行符
					memcpy(tz_string_to_file, tz_buf, strlen(tz_buf));
					tz_string_to_file[strlen(tz_buf)] = '\n';
					ssize_t write_bytes = write(fd, tz_string_to_file, strlen(tz_string_to_file));
					if (write_bytes < 0) {
						XLOG_INFO("Timezone -- set ERROR, {}.", strerror(errno));
						close(fd);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					close(fd);
					
					int ret = access("/etc/localtime", F_OK);
					if (0 == ret) {
						ret = remove("/etc/localtime");
						if (-1 == ret) {
							XLOG_INFO("Timezone -- set ERROR, {}.", strerror(errno));
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
							send(t_client.cfd, response, sizeof(response), 0);
							continue;
						}
					}
					ret = symlink(share_tz_file, "/etc/localtime");
					if (-1 == ret) {
						XLOG_INFO("Timezone -- set ERROR, {}.", strerror(errno));
						// 恢复修改前时区
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					// char sys_cmd[200] = {0};
					// sprintf(sys_cmd, "echo %s > %s", tz_buf, TZ_FILE);
					// my_system(sys_cmd);
					// sprintf(sys_cmd, "ln -sf %s /etc/localtime", share_tz_file);
					// my_system(sys_cmd);
					tzset();	// 强制刷新时区
					XLOG_INFO("Timezone -- set {} ok.", tz_buf);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
					send(t_client.cfd, response, sizeof(response), 0);
#endif

				} else {
					// 回复失败
					XLOG_ERROR("Timezone -- unknown method: {:X}(h).", method);
					char response[10] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					DEBUG_PTINTF("unknown method.\n");
				}
				
			} else if (cmd_id == Update_Fireware) {
				DEBUG_PTINTF("update fireware.\n");
				char response[10] = {0};

				int status = my_system(UPDATE_SCRIPT.c_str());
				if (-1 == status) {
					XLOG_ERROR("Update -- ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				} else {
					if (true == WIFEXITED(status)) {    // 正常退出
						uint8_t exit_code = WEXITSTATUS(status);
						if (0 == exit_code) {
							XLOG_INFO("Update -- ok.");
						} else {
							// 注意 shell 脚本返回值要与最终错误码相对应
							uint16_t update_error_base = RSP_ID::rsp_update_error_base;
							RESPONSE_ID rsp_id = (RESPONSE_ID)(update_error_base + exit_code);
							XLOG_ERROR("Update -- ERROR, status: {:d}.", exit_code);
							DEBUG_PTINTF("run shell script fail, exit code: {%d}.\n", exit_code);
							create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
							send(t_client.cfd, response, sizeof(response), 0);
							continue;
						}
					} else {
						XLOG_ERROR("Update -- start ERROR, signal: {:d}.", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
						DEBUG_PTINTF("run shell script fail, exit code: {%d}.\n", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
				}
				create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_success, response);
				send(t_client.cfd, response, sizeof(response), 0);
			}
			
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		
	}
	DEBUG_PTINTF("recv thread exit.\n");
	return NULL;
}

// accept 线程
void *accept_thread(void *arg) {

	int server_sock_fd = *(int *)&arg;
	int client_sock_fd;
	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(client_addr);

	while (1) {
		DEBUG_PTINTF("Waitting client...\n");
		client_sock_fd = accept(server_sock_fd, (sockaddr *)&client_addr, &client_addr_size);
		if (-1 == client_sock_fd) {
			XLOG_ERROR("accept");
			handle_error("accept");
		}

		t_client.cfd = client_sock_fd;
		
		DEBUG_PTINTF("t_client.cfd = %d, ip = %s, port = %u\n", t_client.cfd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

		pthread_t recv_tid;
		pthread_create(&recv_tid, NULL, &recv_thread, (void *)client_sock_fd);
		pthread_setname_np(recv_tid, "recv thread");
		pthread_detach(recv_tid);
	}
	return NULL;
}

int main(int argc, char *argv[]) {

	std::string sys_log = SYS_LOG_PATH + SYS_LOG_NAME;
	if (0 == access(sys_log.c_str(), F_OK)) {
		remove(sys_log.c_str());
	}
	XLogger::getInstance()->InitXLogger(SYS_LOG_PATH, SYS_LOG_NAME);

	int server_sock_fd;
	struct sockaddr_in server_addr;
	server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == server_sock_fd) {
		XLOG_ERROR("socket error");
		handle_error("socket");
	}
	// 设置端口复用
	int opt = 1;
	setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	if (-1 == bind(server_sock_fd, (sockaddr *)&server_addr, sizeof(server_addr))) {
		XLOG_ERROR("bind error");
		handle_error("bind");
	}

	if (-1 == listen(server_sock_fd, LISTEN_BACKLOG)) {
		XLOG_ERROR("listen error");
		handle_error("listen");
	}

	// 创建 accept 线程
	pthread_t accept_tid;
	pthread_create(&accept_tid, NULL, accept_thread, (void *)server_sock_fd);
	pthread_setname_np(accept_tid, "accept thread");
	pthread_detach(accept_tid);

	// 干状态检测的线程？

	// IPC 初始化
/* 	int nSrvToLoggerFd = 0, nLoggerToSrvFd = 0;
	int nRet = InitIPCFIFO(&m_Logger_Ctrl);
	if (0 == nRet) {
		nSrvToLoggerFd = m_Logger_Ctrl.n_srv_to_logger_fd;
		nLoggerToSrvFd = m_Logger_Ctrl.n_logger_to_srv_fd;
	} */
	int ret = 0;
	ret = mkfifo(FIFO_REQ_PATH, 0666);
	if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
		// 日志
        return -1;
    }
	w_fifo_fd = open(FIFO_REQ_PATH, O_WRONLY);
	if (-1 == w_fifo_fd) {
        perror("open");
		// 日志
        return -1;
    }
	DEBUG_PTINTF("w_fifo_fd = %d\n", w_fifo_fd);
	// ret = mkfifo(FIFO_RSP_PATH, 0666);
	// if (ret < 0 && errno != EEXIST) {
    //     perror("mkfifo");
	// 	// 日志
    //     return -1;
    // }
	// r_fifo_fd = open(FIFO_REQ_PATH, O_RDONLY);
	// if (-1 == r_fifo_fd) {
    //     perror("open");
	// 	// 日志
    //     return -1;
    // }

	while(1) {
		sleep(1);
	}

	// close(server_sock_fd);

	return 0;
}

