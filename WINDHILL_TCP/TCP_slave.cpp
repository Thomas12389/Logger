/*************************************************************************
	> File Name: TCP_slave.cpp
	> Author: 
	> Mail: 
	> Created Time: Mon 08 Nov 2021 05:59:09 PM CST
 ************************************************************************/

#include "../tfm_common.h"
#include "Package.h"
#include "file_opt.h"
#include "time_opt.h"
#include "get_status.h"
#include "get_version.h"
#include "CTimer.h"
// #include "IPC.h"
#include "../Logger/Logger.h"

#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/wait.h>

typedef struct REQUEST_MSG {
	uint16_t m_act;
} Req_Msg;

typedef struct RESPONSE_MSG {
	uint16_t m_rsp;
} Rsp_Msg;

int r_fifo_fd = -1, w_fifo_fd = -1;

#define PORT			14500
#define LISTEN_BACKLOG 	1

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

extern std::vector<FileInfo> vec_file_info;

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

void handler_request_timeout() {
	DEBUG_PTINTF("timeout handler.\n");
}

int DL_Error_NO = 0;
int set_error_no(int err_no) {
	DL_Error_NO = err_no;
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

typedef enum {
	real_config,
	temp_config,
	all_config
} CONFIG_TYPE_t;

int delete_config(CONFIG_TYPE_t c_type) {
	if ( (c_type == CONFIG_TYPE_t::real_config) || (c_type == CONFIG_TYPE_t::all_config) ) {
		int ret = delete_file_by_name(REAL_HW_XML_PATH.c_str());
		if (0 != ret) {
			return 1;
		}

		ret = delete_file_by_name(REAL_APP_JSON_PATH.c_str()); 
		if (0 != ret) {
			return 2;
		}
		
		ret = delete_file_by_name(REAL_INFO_JSON_PATH.c_str());
		if (0 != ret) {
			return 3;
		}
	}

	if ( (c_type == CONFIG_TYPE_t::temp_config) || (c_type == CONFIG_TYPE_t::all_config) ) {
		int ret = delete_file_by_name(TEMP_HW_XML_PATH.c_str());
		if (0 != ret) {
			return 1;
		}

		ret = delete_file_by_name(TEMP_APP_JSON_PATH.c_str()); 
		if (0 != ret) {
			return 2;
		}
		
		ret = delete_file_by_name(TEMP_INFO_JSON_PATH.c_str());
		if (0 != ret) {
			return 3;
		}
	}

    return 0;
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
	CTimer my_timer([=]() {/* handler_request_timeout(); */ nullptr;} );
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
			// 获取协议版本号
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
				create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_unknown_cmd, response);
				send(client_sock_fd, response, sizeof(response), 0);
				continue;
			}
			
			// 取出负载长度
			uint32_t payload_len = ntohl(pack_header.u32_data_len);
			DEBUG_PTINTF("payload_len = %u\n", payload_len);
			uint32_t n_rcv_payload = rcv_len - HEADER_LENGTH;
			uint32_t n_left_payload = 0;

			if (payload_len > n_rcv_payload) {
				n_left_payload = payload_len - n_rcv_payload;
			}

			DEBUG_PTINTF("n_left_payload = %d\n", n_left_payload);

			CTimer::Clock::duration recv_timeout_ms;
			// 判断是否有命令参数
			if (is_need_cmdpara(cmd_id)) {
				// 负载长度不够
				if (payload_len < CMD_PARA_LENGTH) {
					XLOG_ERROR("recv paload length ERROR, cmd: {0:X}(h).", cmd_id);
					DEBUG_PTINTF("recv paload length!!!!\n");
					char response[10] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_payload_len_error, response);
					send(client_sock_fd, response, sizeof(response), 0);
					continue;
				}
			}
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
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_timeout, response);
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

			/* 获取版本信息 */
			if (cmd_id == CMD_ID::Get_Version_Info) {
				DEBUG_PTINTF("Get version information.\n");
				char response[HEADER_LENGTH + sizeof(RESPONSE_ID) + VERSION_LEN] = {0};
				// 获取序列号
				int ret = gethostname(serial, sizeof(serial));
				if (-1 == ret) {
					XLOG_ERROR("Version info -- get serial ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_version_serial_error, response);
					send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					continue;
				}
				DEBUG_PTINTF("serrial number: %s\n", serial);
				// 获取系统版本(Owasys 系统固件版本)
				VERSION_INFO sys_version = {0, 0, 0, 0};
				ret = get_sys_fireware_version(&sys_version);
				if (-1 == ret) {
					XLOG_ERROR("Version info -- get sys version ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_version_sysfw_error, response);
					send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					continue;
				}

				// 采集程序版本号
				VERSION_INFO wcp_version = {0, 0, 0, 0};
				ret = get_wcp_version(&wcp_version);
				if (-1 == ret) {
					XLOG_ERROR("Version info -- get wcp version ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_version_wcp_error, response);
					send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					continue;
				}
				// 管理程序版本号
				VERSION_INFO tcp_version = {0, 0, 0, 0};
				ret = get_tcp_version(&tcp_version);
				if (-1 == ret) {
					XLOG_ERROR("Version info -- get tcp version ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_version_tcp_error, response);
					send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					continue;
				}

				// 获取成功
				XLOG_INFO("Version info -- get ok. SysFW: {:d}.{:d}.{:d}; WCP: {:d}.{:d}.{:d}; TCP: {:d}.{:d}.{:d}.",
							sys_version.maj_version, sys_version.min_version, sys_version.revision_number, 
							wcp_version.maj_version, wcp_version.min_version, wcp_version.revision_number, 
							tcp_version.maj_version, tcp_version.min_version, tcp_version.revision_number);
				int idx = HEADER_LENGTH + sizeof(RESPONSE_ID);
				create_rsp_general(PRO_VERSION, cmd_id, 28, RSP_ID::rsp_common_success, response);
				memcpy(response + idx, serial, SERIAL_BYTES);
				idx += sizeof(serial);
				memcpy(response + idx, &sys_version, sizeof(VERSION_INFO));
				idx += sizeof(VERSION_INFO);
				memcpy(response + idx, &wcp_version, sizeof(VERSION_INFO));
				idx += sizeof(VERSION_INFO);
				memcpy(response + idx, &tcp_version, sizeof(VERSION_INFO));
				idx += sizeof(VERSION_INFO);
				send(client_sock_fd, response, sizeof(response), 0);
				
			/* 获取状态信息 */	
			} else if (cmd_id == CMD_ID::Get_Status) {
				GET_STATUS staus_type = (GET_STATUS)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				int rsp_error_len = HEADER_LENGTH + sizeof(RESPONSE_ID);

				if (staus_type == GET_STATUS::get_sys_status) {
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID) + SYS_STATUS_LEN] = {0};
					typedef struct SYS_STATUS {
						float cpu_load;
						MEM_OCCUPY_t ram_usage;
						DISK_OCCUPY_t flash_usage;
						DISK_OCCUPY_t tf_usage;
					} SYS_STATUS;
					SYS_STATUS sys_status;
					if (-1 == get_sysCpuUsage(&sys_status.cpu_load)) {
						XLOG_ERROR("Get status -- get system cpu load ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_status_cpuload_error, response);
						send(t_client.cfd, response, rsp_error_len, 0);
						continue;
					}
					if (-1 == get_memocupy(&sys_status.ram_usage)) {
						XLOG_ERROR("Get status -- get system RAM status ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_status_ram_error, response);
						send(t_client.cfd, response, rsp_error_len, 0);
						continue;
					}
					if (-1 == get_diskUsage(FLASH_MOUNT_DIR.c_str(), &sys_status.flash_usage)) {
						XLOG_ERROR("Get status -- get system Flash status ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_status_flash_error, response);
						send(t_client.cfd, response, rsp_error_len, 0);
						continue;
					}
					if (-1 == get_diskUsage(TF_MOUNT_DIR.c_str(), &sys_status.tf_usage)) {
						XLOG_ERROR("Get status -- get system external TF status ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_status_tf_error, response);
						send(t_client.cfd, response, rsp_error_len, 0);
						continue;
					}
					// 获取系统状态成功
					XLOG_INFO("Get status -- get ok. CPU: {:.2f}%; RAM: {:.2f}(KB)/{:.2f}(KB); Flash: {:.2f}(KB)/{:.2f}(KB); TF: {:.2f}(KB)/{:.2f}(KB)",
								sys_status.cpu_load, sys_status.ram_usage.availableKB, sys_status.ram_usage.totalKB, sys_status.flash_usage.availableKB, 
								sys_status.flash_usage.totalKB, sys_status.tf_usage.availableKB, sys_status.tf_usage.totalKB);
					DEBUG_PTINTF("CPU: %.2f%%; RAM: %.2f(KB)/%.2f(KB); Flash: %.2f(KB)/%.2f(KB); TF: %.2f(KB)/%.2f(KB)",
								sys_status.cpu_load, sys_status.ram_usage.availableKB, sys_status.ram_usage.totalKB, sys_status.flash_usage.availableKB, 
								sys_status.flash_usage.totalKB, sys_status.tf_usage.availableKB, sys_status.tf_usage.totalKB);
					int idx = rsp_error_len;
					create_rsp_general(PRO_VERSION, cmd_id, SYS_STATUS_LEN, RSP_ID::rsp_common_success, response);
					float temp = my_htonf(sys_status.cpu_load);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					temp = my_htonf(sys_status.ram_usage.totalKB);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					temp = my_htonf(sys_status.ram_usage.availableKB);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					temp = my_htonf(sys_status.flash_usage.totalKB);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					temp = my_htonf(sys_status.flash_usage.availableKB);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					temp = my_htonf(sys_status.tf_usage.totalKB);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					temp = my_htonf(sys_status.tf_usage.availableKB);
					memcpy(response + idx, &temp, sizeof(temp));
					idx += sizeof(temp);
					send(client_sock_fd, response, sizeof(response), 0);
				} else if (staus_type == GET_STATUS::get_wcp_status) {
					// 运行状态 0: 停止  1： 运行
					uint8_t wcp_status = 0;
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID) + sizeof(wcp_status)] = {0};

					int ret = get_process_status(WCP_NAME.c_str());
					if (-1 == ret) {
						XLOG_ERROR("Get status -- get Logger status ERROR, {}", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_status_wcp_error, response);
						send(client_sock_fd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
						continue;
					} 

					wcp_status = (0 == ret) ? 0 : 1;
					XLOG_INFO("Get status -- get Logger status ok, is {}", (0 == ret) ? "stopped" : "running");
					create_rsp_general(PRO_VERSION, cmd_id, sizeof(wcp_status), RSP_ID::rsp_common_success, response);
					int idx = rsp_error_len;
					memcpy(response + idx, &wcp_status, sizeof(wcp_status));
					send(client_sock_fd, response, sizeof(response), 0);
				} else {
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID)] = {0};
					XLOG_ERROR("Get status -- unknown status type({:d}).", staus_type);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_get_status_unsupported, response);
					send(client_sock_fd, response, sizeof(response), 0);
				}

			/* 下载 */
			} else if (cmd_id == CMD_ID::Download) {
				char response[HEADER_LENGTH + sizeof(RSP_ID)] = {0};
				// 开启记时器
				my_timer.StartOnce(recv_timeout_ms);
				// 先接收完下载类型
				while ((n_rcv_payload < CMD_PARA_LENGTH) && my_timer.IsRunning() ) {
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
				if (n_rcv_payload < CMD_PARA_LENGTH) {
					XLOG_ERROR("Download -- recv payload timeout, {:.0f} ms.", recv_timeout_ms.count() / 1e+6);
					DEBUG_PTINTF("timeout!!!!\n");
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_timeout, response);
					send(client_sock_fd, response, sizeof(response), 0);
					continue;
				}

				int dfd = -1;
				DOWNLOAD_TYPE d_type = (DOWNLOAD_TYPE)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				DEBUG_PTINTF("d_type = %u\n", d_type);
				
				// 根据下载类型确定要操作的文件
				int is_configuration_file = true;
				char temp_file_path[256] = {0};
				char temp_file_name[128] = {0};
				// 确保配置文件目录存在
				if (-1 == access(CONFIG_DIR.c_str(), F_OK)) {
					if (-1 == mkdir(CONFIG_DIR.c_str(), 0775)) {
						XLOG_ERROR("make configuare dir: {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_download_config_dir_not_exist, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
        			}
				}
				if (d_type == DOWNLOAD_TYPE::dl_xml_hw) {
					dfd = open(TEMP_HW_XML_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
					strcpy(temp_file_path, TEMP_HW_XML_PATH.c_str());
					strcpy(temp_file_name, TEMP_HW_XML_NAME.c_str());
				} else if (d_type == DOWNLOAD_TYPE::dl_json_app) {
					dfd = open(TEMP_APP_JSON_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
					strcpy(temp_file_path, TEMP_APP_JSON_PATH.c_str());
					strcpy(temp_file_name, TEMP_APP_JSON_NAME.c_str());
				} else if (d_type == DOWNLOAD_TYPE::dl_json_info) {
					dfd = open(TEMP_INFO_JSON_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
					strcpy(temp_file_path, TEMP_INFO_JSON_PATH.c_str());
					strcpy(temp_file_name, TEMP_INFO_JSON_NAME.c_str());
				} else if (d_type == DOWNLOAD_TYPE::dl_license) {
					dfd = open(TEMP_LICENSE_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
					strcpy(temp_file_path, TEMP_LICENSE_PATH.c_str());
					strcpy(temp_file_name, TEMP_LICENSE_NAME.c_str());
					is_configuration_file = false;
				} else if (d_type == DOWNLOAD_TYPE::dl_fireware) {
					dfd = open(REAL_FIREWARE_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
					strcpy(temp_file_path, REAL_FIREWARE_PATH.c_str());
					strcpy(temp_file_name, REAL_FIREWARE_NAME.c_str());
					is_configuration_file = false;
				} else {
					// 回复下载类型不支持
					XLOG_ERROR("Download -- unsupported type: {:X}(h).", d_type);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_download_unsupported_type, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				
				if (dfd < 0) {
					// 回复错误
					XLOG_ERROR("Download -- open file {}({:d}) ERROR. {}.", temp_file_name, d_type, strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_open_failed, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				} 
				// 若接收到部分内容则先写入，注意要减去 下载类型所占字节数（2 byte）
				if (n_rcv_payload > CMD_PARA_LENGTH) {
					if (0 > write(dfd, rcv_buf + HEADER_LENGTH + CMD_PARA_LENGTH, n_rcv_payload - CMD_PARA_LENGTH) ) {
						XLOG_INFO("Download -- write file {}({:d}) ERROR, {}.", temp_file_name, d_type, strerror(errno));
						close(dfd);
						// 删除临时文件
						if (true == is_configuration_file) {
							delete_config(CONFIG_TYPE_t::temp_config);
						} else {
							delete_file_by_name(temp_file_path);
						}
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_download_file_save_error, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
				}
				
				while (n_left_payload > 0 && my_timer.IsRunning()) {
					char buffer[1024] = {0};
					ssize_t num_recv = recv(t_client.cfd, buffer, sizeof(buffer), MSG_DONTWAIT);
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
					if (n_left_payload < num_recv) {
						num_recv = n_left_payload;
					}
					ssize_t bytes_to_file = write(dfd, buffer, num_recv);
					if (bytes_to_file < 0) {
						XLOG_INFO("Download -- write file {}({:d}) ERROR, {}.", temp_file_name, d_type, strerror(errno));
						close(dfd);
						// 删除临时文件
						if (true == is_configuration_file) {
							delete_config(CONFIG_TYPE_t::temp_config);
						} else {
							delete_file_by_name(temp_file_path);
						}
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_download_file_save_error, response);
						send(t_client.cfd, response, sizeof(response), 0);
						break;
					}
					// n_left_payload >= num_recv
					n_left_payload -= num_recv;
				}

				// 超时，撤销已经执行的操作
				if (n_left_payload > 0) {
					XLOG_ERROR("Download -- {}({:d}) timeout, {:.0f} ms.", temp_file_name, d_type, recv_timeout_ms.count() / 1e+6);
					DEBUG_PTINTF("download timeout.\n");
					close(dfd);
					// 删除临时文件
					if (true == is_configuration_file) {
						delete_config(CONFIG_TYPE_t::temp_config);
					} else {
						delete_file_by_name(temp_file_path);
					}
					
					// 回复超时
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_timeout, response);
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
						XLOG_ERROR("Download -- get temp file {}({:d}) size ERROR, {}.", temp_file_name, d_type, strerror(errno));
						// 获取长度失败，直接删除临时文件（temp_hw.xml, temp_app.json, temp_info.json）
						if (true == is_configuration_file) {
							delete_config(CONFIG_TYPE_t::temp_config);
						} else {
							delete_file_by_name(temp_file_path);
						}
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_get_file_attr_error, response);
					} else if (stat_buf.st_size == 0) {
						XLOG_ERROR("Download -- file {}({:d}) length is 0.", temp_file_name, d_type);
						if (true == is_configuration_file) {
							delete_config(CONFIG_TYPE_t::temp_config);
						} else {
							delete_file_by_name(temp_file_path);
						}
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_download_file_length_zero, response);
					} else if (payload_len - CMD_PARA_LENGTH == stat_buf.st_size) {
						// TODO：校验文件格式？？？
						// 更新 license 文件
						if (0 == strcmp(temp_file_path, TEMP_LICENSE_PATH.c_str())) {
							rename(temp_file_path, REAL_LICENSE_PATH.c_str());
						}
						XLOG_INFO("Download -- file {}({:d}) ok.", temp_file_name, d_type);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_success, response);
					} else {
						// 回复文件长度不对
						if (true == is_configuration_file) {
							delete_config(CONFIG_TYPE_t::temp_config);
						} else {
							delete_file_by_name(temp_file_path);
						}
						XLOG_ERROR("Download -- file {}({:d}) length ERROR.", temp_file_name, d_type);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_download_file_len_error, response);
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
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_upload_file_not_exist, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				DEBUG_PTINTF("file name : %s\n", vec_file_info[file_no].file_name.c_str());
				ufd = open(vec_file_info[file_no].file_name.c_str(), O_RDONLY);
				if (ufd < 0) {
					// 回复失败
					XLOG_ERROR("Upload -- open data file ERROR, {}", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_open_failed, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				} 
				// 数据文件存在
				create_rsp_general(PRO_VERSION, cmd_id, vec_file_info[file_no].file_size, RSP_ID::rsp_common_success, response);
				send(t_client.cfd, response, sizeof(response), 0);
				// 发送文件内容
				ssize_t total_send_bytes = 0;
				while (1) {
					char snd_buf[1024] = {0};
					ssize_t num_read = read(ufd, snd_buf, sizeof(snd_buf));
					if (num_read < 0) {
						XLOG_ERROR("Upload -- read data file ERROR, {}.", strerror(errno));
						DEBUG_PTINTF("upload, read file ERROR.\n");
						break;
					}
					if (send(t_client.cfd, snd_buf, num_read, 0) < 0) {
						XLOG_ERROR("Upload -- send data file ERROR, {}.", strerror(errno));
						DEBUG_PTINTF("upload, send ERROR.\n");
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
					strcpy(file_name, REAL_HW_XML_NAME.c_str());
					ufd = open(REAL_HW_XML_PATH.c_str(), O_RDONLY);
				} else if (u_type == UPLOAD_TYPE::ul_user_log) {
					strcpy(file_name, USER_LOG_NAME.c_str());
					if (0 == get_all_file_info(USER_LOG_DIR.c_str(), "log")) {
						if (vec_file_info.size() == 0) {
							// 不存在 user log 文件
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_upload_file_not_exist, response);
							send(t_client.cfd, response, sizeof(response), 0);
							XLOG_ERROR("Upload -- user log does not exist.");
							continue;
						}
					} else {
						RSP_ID rsp_id = (errno == ENOENT) ? RSP_ID::rsp_upload_file_not_exist : RSP_ID::rsp_common_get_file_attr_error;
						// 获取 user log 文件信息失败
						create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
						send(t_client.cfd, response, sizeof(response), 0);
						XLOG_ERROR("Upload -- get user log ERROR, {}.", strerror(errno));
						continue;
					}
					// 打开最新的 log 文件
					ufd = open(vec_file_info[vec_file_info.size() - 1].file_name.c_str(), O_RDONLY);
				} else if (u_type == UPLOAD_TYPE::ul_sys_log) {
					strcpy(file_name, SYS_LOG_NAME.c_str());
					ufd = open(SYS_LOG_PATH.c_str(), O_RDONLY);
				} else if (u_type == UPLOAD_TYPE::ul_license) {
					strcpy(file_name, REAL_LICENSE_NAME.c_str());
					ufd = open(REAL_LICENSE_PATH.c_str(), O_RDONLY);
				} else {
					// 回复上传类型不支持
					XLOG_ERROR("Upload -- unsupported file type {:X}(h).", u_type);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_upload_unsupported_type, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				
				if (ufd < 0) {
					// 回复错误，打开文件失败
					XLOG_ERROR("Upload -- open file {} ERROR, {}.", file_name, strerror(errno));
					RSP_ID rsp_id = (errno == ENOENT) ? RSP_ID::rsp_upload_file_not_exist : RSP_ID::rsp_common_open_failed;
					create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				//
				struct stat stat_buf;
				int ret = fstat(ufd, &stat_buf);
				if (ret == -1) {
					DEBUG_PTINTF("fstat error.\n");
					close(ufd);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_get_file_attr_error, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}

				create_rsp_general(PRO_VERSION, cmd_id, stat_buf.st_size, RSP_ID::rsp_common_success, response);
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
					XLOG_INFO("Upload -- file {}({:d}) ok.", file_name, u_type);
				}
			/* 获取数据文件信息 */
			} else if (cmd_id == CMD_ID::Get_Data_File_Info) {
				if (0 == get_all_file_info(DATA_DIR.c_str(), "tgz")) {
					// 正响应，并给出具体信息
					DEBUG_PTINTF("get data files info ok.\n");
					size_t file_cnt = vec_file_info.size();
					size_t file_info_bytes = 14 * file_cnt;
					size_t payload_bytes = file_info_bytes + 2;
					size_t num_bytes = HEADER_LENGTH + payload_bytes;
					char *response = (char *)malloc(num_bytes);

					create_rsp_general(PRO_VERSION, cmd_id, file_info_bytes, RSP_ID::rsp_common_success, response);
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
					RSP_ID rsp_id = (errno == ENOENT) ? RSP_ID::rsp_upload_file_not_exist : RSP_ID::rsp_common_get_file_attr_error;
					create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
					send(t_client.cfd, response, sizeof(response), 0);
				}
			/* 删除数据文件 */
			} else if (cmd_id == CMD_ID::Delete_Data_File) {
				char response[10] = {0};
				uint16_t file_no = ((uint16_t)rcv_buf[8] << 8) | rcv_buf[9];
				if (0 == delete_file((unsigned long)file_no)) {
					// 回复完成
					XLOG_INFO("Delete -- data file ok.");
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_success, response);
					DEBUG_PTINTF("delete ok.\n");
				} else {
					// 回复失败
					XLOG_ERROR("Detele -- data file {:d} ERROR, {}.", file_no, strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
					DEBUG_PTINTF("delete error.\n");
				}
				send(t_client.cfd, response, sizeof(response), 0);
			/* 记录仪动作 */
			} else if (cmd_id == CMD_ID::Logger_Action) {
				char response[HEADER_LENGTH + sizeof(RESPONSE_ID)] = {0};
				LOGGER_ACTION action = (LOGGER_ACTION)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				// 获取进程状态
				int wcp_status = get_process_status(WCP_NAME.c_str());

				if (action == LOGGER_ACTION::act_start) {
					if (-1 == wcp_status) {
						XLOG_ERROR("Action -- start ERROR, {}", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					} else if (1 == wcp_status) {
						XLOG_ERROR("Action -- start, {} is running.", WCP_NAME);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_action_is_running, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					}

					if (0 != access(WCP_START_SCRIPT_PATH.c_str(), X_OK)) {
						XLOG_ERROR("Action -- start ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_script_not_exist, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
					// 使用脚本启动 WCP
					char start_wcp[128] = {0};
					sprintf(start_wcp, "%s %s", WCP_START_SCRIPT_PATH.c_str(), WCP_NAME.c_str());
					int status = my_system(start_wcp);
					// 判断返回值
					if (-1 == status) {
						XLOG_ERROR("Action -- start ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					} else {
						if (true == WIFEXITED(status)) {    // 启动脚本正常退出
							uint8_t exit_code = WEXITSTATUS(status);
							if (0 == exit_code) {
								XLOG_INFO("Action -- start ok.");
								DEBUG_PTINTF("Action -- start ok.\n");
							} else {
								uint16_t action_error_base = RSP_ID::rsp_action_error_base;
								RESPONSE_ID rsp_id = (RESPONSE_ID)(action_error_base + exit_code);
								XLOG_ERROR("Action -- start ERROR, status: {:d}.", exit_code);
								DEBUG_PTINTF("run shell script fail, exit code: {%d}.\n", exit_code);
								create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
								send(t_client.cfd, response, sizeof(response), 0);
								continue;
							}
						} else {
							XLOG_ERROR("Action -- start ERROR, signal: {:d}.", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
							DEBUG_PTINTF("run shell script fail, exit code: {%d}.\n", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
							create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
							send(t_client.cfd, response, sizeof(response), 0);
							continue;
						}
					}
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_success, response);
				} else if (action == LOGGER_ACTION::act_stop) {
					if (-1 == wcp_status) {
						XLOG_ERROR("Action -- stop ERROR, {}", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					} else if (0 == wcp_status) {
						XLOG_ERROR("Action -- stop, {} is not running.", WCP_NAME);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_action_is_stopped, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					}
					// 进程间通信
					Req_Msg w_buf = {0};
					w_buf.m_act = action;
					ssize_t write_bytes = write(w_fifo_fd, &w_buf, sizeof(w_buf));
					if (write_bytes < 0) {
						XLOG_ERROR("Action -- stop ERROR, {}.", strerror(errno));
						RSP_ID rsp_id = (errno == EPIPE) ? RSP_ID::rsp_action_has_execute_stop : RSP_ID::rsp_common_fail;
						create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
					} else {
						// TODO:等待响应
						XLOG_INFO("Action -- stop ok.");
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_success, response);
					} 
					
				} else if (action == LOGGER_ACTION::act_reset) {
					if (-1 == wcp_status) {
						XLOG_ERROR("Action -- reset ERROR, {}", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					} else if (1 == wcp_status) {
						XLOG_ERROR("Action -- reset ERROR, {} is running.", WCP_NAME);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_action_is_running, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					}
					// 清除配置，添加默认配置
					RSP_ID rsp_id = RSP_ID::rsp_common_success;
					int ret = delete_config(CONFIG_TYPE_t::all_config);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_action_reset_failed;
						XLOG_ERROR("Action -- reset ERROR, {}.", strerror(errno));
					} else {
						XLOG_INFO("Action -- reset ok.");
					}
					create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);

				} else {
					// 回复失败
					XLOG_ERROR("Action -- unsupported action: {:X}(h).", action);
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_action_unsupported, response);
					DEBUG_PTINTF("unknown action.\n");
				}
				send(t_client.cfd, response, sizeof(response), 0);
			/* 获取/设置系统时间 */
			} else if (cmd_id == CMD_ID::Method_System_Time) {
				METHOD method = (METHOD)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				if (method == METHOD::get) {
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID) + DATE_TIME_BYTE] = {0};

					struct tm get_tm;
					RESPONSE_ID rsp_id = RSP_ID::rsp_common_success;
					int ret = get_systime(&get_tm);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_common_fail;
						XLOG_ERROR("Time -- get ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
						send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID), 0);
					} else {
						create_rsp_general(PRO_VERSION, cmd_id, DATE_TIME_BYTE, RSP_ID::rsp_common_success, response);
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

				} else if (method == METHOD::set) {
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID)] = {0};
					RESPONSE_ID rsp_id = RSP_ID::rsp_common_success;
					// 若采集程序正在运行，则不允许修改时间
					int ret = get_process_status(WCP_NAME.c_str());
					if (-1 == ret) {
						XLOG_ERROR("Time -- set ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					} else if (1 == ret) {
						XLOG_ERROR("Time -- set ERROR, {} is running.", WCP_NAME);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_time_wcp_is_running, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					}
					// 判断该包的负载长度是否正确
					if (payload_len != (DATE_TIME_BYTE + CMD_PARA_LENGTH)) {
						rsp_id = RSP_ID::rsp_common_payload_len_error;
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
					// TODO:增加合法性判断？
					XLOG_INFO("Time -- need to set, {:d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", 
								set_tm.tm_year + 1900, set_tm.tm_mon + 1, set_tm.tm_mday, set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);

					ret = set_systime(set_tm);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_time_invalid_time;
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

				} else {
					// 回复失败
					XLOG_ERROR("Time -- unknown method: {:X}(h).", method);
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID)] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_time_unknown_method, response);
					send(t_client.cfd, response, sizeof(response), 0);
					DEBUG_PTINTF("unknown method.\n");
				}
				
			/* 获取/设置系统时区 */
			} else if (cmd_id == CMD_ID::Method_TZ) {
				METHOD method = (METHOD)(((uint16_t)rcv_buf[8] << 8) | rcv_buf[9]);
				if (method == METHOD::get) {
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID) + MAXLEN_TIMEZONE] = {0};
					char tz_buf[MAXLEN_TIMEZONE] = {0};

					RESPONSE_ID rsp_id = RSP_ID::rsp_common_success;
					int ret = get_timezone(tz_buf);
					if (0 != ret) {
						rsp_id = RSP_ID::rsp_common_fail;
						XLOG_ERROR("Timezone -- get ERROR, {}.", strerror(errno));
					} else {
						XLOG_INFO("Timezone -- get ok, {}.", tz_buf); 
					}
					create_rsp_general(PRO_VERSION, cmd_id, strlen(tz_buf), rsp_id, response);
					memcpy(response + 10, tz_buf, strlen(tz_buf));
					send(t_client.cfd, response, HEADER_LENGTH + sizeof(RESPONSE_ID) + strlen(tz_buf), 0);

				} else if (method == METHOD::set) {
					char tz_buf[MAXLEN_TIMEZONE] = {0};
					char response[HEADER_LENGTH + sizeof(RESPONSE_ID)] = {0};

					RESPONSE_ID rsp_id = RSP_ID::rsp_common_success;
					// 若采集程序正在运行，则不允许修改时区
					int ret = get_process_status(WCP_NAME.c_str());
					if (-1 == ret) {
						XLOG_ERROR("Time -- set ERROR, {}.", strerror(errno));
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					} else if (1 == ret) {
						XLOG_ERROR("Time -- set ERROR, {} is running.", WCP_NAME);
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_time_wcp_is_running, response);
						send(client_sock_fd, response, sizeof(response), 0);
						continue;
					}
					memcpy(tz_buf, rcv_buf + HEADER_LENGTH + sizeof(RESPONSE_ID), n_rcv_payload - sizeof(RESPONSE_ID));
					int ret = set_timezone(tz_buf, strlen(tz_buf));
					if (2 == ret) {
						rsp_id = RSP_ID::rsp_time_invalid_timezone;
						XLOG_ERROR("Timezone -- set ERROR, {%s} is not a valid timezone.\n", tz_buf);
					} else if (0 != ret) {
						rsp_id = RSP_ID::rsp_common_fail;
						XLOG_ERROR("Timezone -- set ERROR, {}.", strerror(errno));
					} else {
						XLOG_INFO("Timezone -- set {} ok.", tz_buf);
					}
					create_rsp_general(PRO_VERSION, cmd_id, 0, rsp_id, response);
					send(t_client.cfd, response, sizeof(response), 0);

				} else {
					// 回复失败
					XLOG_ERROR("Timezone -- unknown method: {:X}(h).", method);
					char response[10] = {0};
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_time_unknown_method, response);
					send(t_client.cfd, response, sizeof(response), 0);
					DEBUG_PTINTF("unknown method.\n");
				}
			/* 固件更新 */
			} else if (cmd_id == Update_Fireware) {
				DEBUG_PTINTF("update fireware.\n");
				char response[HEADER_LENGTH + sizeof(RESPONSE_ID)] = {0};

				if (0 != access(UPDATE_FIREWARE_SCRIPT_PATH.c_str(), X_OK)) {
					XLOG_ERROR("Update -- ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_script_not_exist, response);
					send(t_client.cfd, response, sizeof(response), 0);
					continue;
				}
				// 启动更新
				int status = my_system(UPDATE_FIREWARE_SCRIPT_PATH.c_str());
				if (-1 == status) {
					XLOG_ERROR("Update -- ERROR, {}.", strerror(errno));
					create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
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
						create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_fail, response);
						send(t_client.cfd, response, sizeof(response), 0);
						continue;
					}
				}
				create_rsp_general(PRO_VERSION, cmd_id, 0, RSP_ID::rsp_common_success, response);
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

	if (0 == access(SYS_LOG_PATH.c_str(), F_OK)) {
		remove(SYS_LOG_PATH.c_str());
	}
	XLogger::getInstance()->InitXLogger(SYS_LOG_DIR, SYS_LOG_NAME);

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
	ret = mkfifo(FIFO_REQ_PATH.c_str(), 0666);
	if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
		// 日志
        return -1;
    }
	w_fifo_fd = open(FIFO_REQ_PATH.c_str(), O_WRONLY);
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

