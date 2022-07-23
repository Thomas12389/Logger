/*************************************************************************
	> File Name: sftp_test.cpp
	> Author: 
	> Mail: 
	> Created Time: Mon 28 Dec 2020 02:52:48 PM CST
 ************************************************************************/

#include <iostream>
#include <cstring>

#include "../tfm_common.h"

#include "SftpClient.h"
#include "FtpClient.h"
#include "DevConfig/PhyChannel.hpp"

#include "Compress/Compress.h"

#include <dirent.h>
#include <sys/stat.h>

#include "Logger/Logger.h"
#include "tbox/LED_Control.h"
#include "tbox/Common.h"
extern Connection g_Connection;
extern Net_Info g_Net_Info;
extern File_Save g_File_Info;

int remove_recursively(const char *dir) {
	// 检查路径是否存在
	if (0 != access(dir, F_OK)) {
		return 0;
	}

	// 获取相关信息
	struct stat dir_stat;
	if (stat(dir, &dir_stat) < 0) {
		XLOG_DEBUG("get directory stat error.");
		return -1;
	}

	DIR *dirp = NULL;
	struct dirent *dp = NULL;
	if (S_ISREG(dir_stat.st_mode)) {			// 普通文件直接删除
		remove(dir);
	} else if (S_ISDIR(dir_stat.st_mode)) {		// 子目录递归删除
		dirp = opendir(dir);
		while ((dp = readdir(dirp)) != NULL) {
        	if (strncmp(dp->d_name, ".", strlen(dp->d_name)) == 0 ||
            	strncmp(dp->d_name, "..", strlen(dp->d_name)) == 0) {
                	continue;
        	}

			// 递归子目录
			char sub_dir_name[512] = {0};
			sprintf(sub_dir_name, "%s/%s", dir, dp->d_name);
			remove_recursively(sub_dir_name);
		}
		closedir(dirp);
		// 递归完成后删除空目录
		rmdir(dir);
	} else {
		XLOG_DEBUG("Unknow file type.");
	}

	return 0;
}


int ftp_upload() {

	DEVICE_STATUS_COMPRESS();
	// compress
	std::string data_absoult_path = DATA_DIR + g_File_Info.strLocalDirName;
	std::string log_file_name = "log_" + g_File_Info.strLocalDirName + ".log";	// log 文件

	char compressName[256] = {0};
	// 数据文件名称
	strcpy(compressName, (char *)(/* g_File_Info.strConfigFileName + '_' + */ g_File_Info.strLocalDirName + ".owa").c_str());
	
	int ret = 0;
	if (g_File_Info.bIsActive) {
		// 如果使能了数据保存，则压缩数据文件
		ret = compress_gz(data_absoult_path.c_str(), compressName);
		if ( 0 == ret) {
			XLOG_INFO("Compress data file successfully.");
		} else {
			XLOG_WARN("Failed to compress the data files. Restart may resolve the problem.");
		}
	} else {
		XLOG_INFO("No data file compression is required.");
	}

#if 0
	// 最终打包文件名
	char tar_name[512] = {0};
	strncat(tar_name, data_absoult_path.c_str(), data_absoult_path.length());
	strncat(tar_name, ".tgz", strlen(".tgz"));
	// 打包压缩命令
	char tar_cmd[1024] = {0};
	strncat(tar_cmd, "tar -zcvPf ", strlen("tar -zcvPf "));
	strncat(tar_cmd, tar_name, strlen(tar_name));
	// 数据文件
	if (0 == access( (DATA_DIR + compressName).c_str(), F_OK )) {
		strncat(tar_cmd, " -C ", strlen(" -C "));
		strncat(tar_cmd, DATA_DIR.c_str(), DATA_DIR.length());
		strncat(tar_cmd, " ", strlen(" "));
		strncat(tar_cmd, compressName, strlen(compressName));

		// 数据描述文件
		char cp_cmd[512] = {0};
		char desp_xml[256] = "description.xml";
		strncat(cp_cmd, "cp /home/debian/WINDHILLRT/hw.xml ", strlen("cp /home/debian/WINDHILLRT/hw.xml "));
		strncat(cp_cmd, DATA_DIR.c_str(), DATA_DIR.length());
		strncat(cp_cmd, desp_xml, strlen(desp_xml));
		system(cp_cmd);
	}

	strncat(tar_cmd, " ", strlen(" "));
	strncat(tar_cmd, desp_xml, strlen(desp_xml));
	// 日志文件
	strncat(tar_cmd, " -C ", strlen(" -C "));
	strncat(tar_cmd, USER_LOG_DIR.c_str(), USER_LOG_DIR.length());
	strncat(tar_cmd, " ", strlen(" "));
	strncat(tar_cmd, log_file_name.c_str(), log_file_name.length());
	// traffic 文件
	std::string can_blf_file_name = g_File_Info.strLocalDirName + ".blf";	// blf 文件
	if (0 == access((CAN_TRAFFIC_DIR + can_blf_file_name).c_str(), F_OK) ) {
		struct dirent *dir = NULL;
		DIR *dirpCan = opendir(CAN_TRAFFIC_DIR.c_str());
		if (NULL != dirpCan) {

			strncat(tar_cmd, " -C ", strlen(" -C "));
			strncat(tar_cmd, CAN_TRAFFIC_DIR.c_str(), CAN_TRAFFIC_DIR.length());
			strncat(tar_cmd, " ", strlen(" "));

			while ((dir = readdir(dirpCan)) != NULL) {
				if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
					continue;
				}

				if (dir->d_type != DT_REG) continue;
				// 后缀符合，解析文件信息
				if (0 == strncmp(dir->d_name, g_File_Info.strLocalDirName.c_str(), g_File_Info.strLocalDirName.length()) ) {
					strncat(tar_cmd, dir->d_name, strlen(dir->d_name));
					strncat(tar_cmd, " ", strlen(" "));
				}
			}
		}

		closedir(dirpCan);
	}
	// 打包压缩后删除源文件
	// XLOG_INFO("tar cmd: {}", tar_cmd);
	strncat(tar_cmd, " --remove-files", strlen(" --remove-files"));

	// 不应再打印 log		
	printf("tar cmd :  %s\n", tar_cmd);
	int status = my_system(tar_cmd);
#else
	std::string desp_xml_name = "description.xml";
	std::string cp_xml_cmd = "cp " + REAL_HW_XML_PATH + " " + DATA_DIR + desp_xml_name;
	// 最终打包文件名称
	std::string tar_name = data_absoult_path + ".tgz"; // TODO：加上结束时间？
	std::string tar_cmd = "tar -zcPf " + tar_name;
	
	// 如果数据文件存在
	if (0 == access( (DATA_DIR + compressName).c_str(), F_OK )) {
		tar_cmd += (" -C " + DATA_DIR + " " + compressName);

		my_system(cp_xml_cmd.c_str());
		// 配置文件
		if (0 == access((DATA_DIR + desp_xml_name).c_str(), F_OK) ) {
			tar_cmd += (" " + desp_xml_name);
		} else {
			XLOG_WARN("The data file exist but the description file not exist.\nUpload the configuration file(xml) before conversion， if haven't update it yet.");
			// return -1;
		}
	} 

	// 日志文件
	if (0 == access((USER_LOG_DIR + log_file_name).c_str(), F_OK) ) {
		tar_cmd += (" -C " + USER_LOG_DIR + " " + log_file_name);
	} else {
		return -1;
	}
	// can traffic 文件
	std::string can_blf_file_name = g_File_Info.strLocalDirName + ".blf";
	if (0 == access((CAN_TRAFFIC_DIR + can_blf_file_name).c_str(), F_OK) ) {
		struct dirent *dir = NULL;
		DIR *dirpCan = opendir(CAN_TRAFFIC_DIR.c_str());
		if (NULL != dirpCan) {

			tar_cmd += (" -C " + CAN_TRAFFIC_DIR + " ");

			while ((dir = readdir(dirpCan)) != NULL) {
				if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
					continue;
				}

				if (dir->d_type != DT_REG) continue;

				if (0 == strncmp(dir->d_name, g_File_Info.strLocalDirName.c_str(), g_File_Info.strLocalDirName.length()) ) {
					tar_cmd += dir->d_name;
					tar_cmd += " ";
				}
			}
		}

		closedir(dirpCan);
	}

	// 打包压缩后删除源文件
	tar_cmd += " --remove-files";
	XLOG_DEBUG("tar cmd: {}", tar_cmd);

	// 不应再打印 log
	sleep(1);		// 用于日志刷新
	
	// printf("tar cmd :  %s\n", tar_cmd.c_str());
	int status = my_system(tar_cmd.c_str());
#endif
	if (-1 == status) {
		// printf("Create tgz file ERROR.");
		return -1;
	} else {
		if (true == WIFEXITED(status)) {    // 正常退出
			uint8_t exit_code = WEXITSTATUS(status);
			if (0 == exit_code) {
				// printf("Create tgz file successfully.\n");
				// 不删除用于测试数据文件何时出错
				ret = remove_recursively(data_absoult_path.c_str());
				if (ret < 0) {
					// printf("remove_recursively error.\n");
				}
			} else {
				// printf("Create tgz file ERROR, status: %d.\n", exit_code);
				return -1;
			}
		} else {
			// printf("Create tgz file ERROR, signal: %d.\n", WIFSIGNALED(status) ? WTERMSIG(status) : 0xFF);
			return -1;
		}
	}

	// 判断 FTP 是否激活
	if (false == g_Net_Info.ftp_server.bIsActive) {
		// printf("FTP is not active.\n");
		return 0;
	}

	// 判断是否配置了网络
/* 	if ( (false == g_Connection.Modem_Conn.bActive) && (g_Connection.nWLAN <= 0) ){
		return -1;
	} */

	DEVICE_STATUS_UPLOAD();
	// 构建上传文件名
	char uploadDir[256] = {0};
	char uploadName[256] = {0};
	// 远程文件夹名称 -- 设备序列号
	strncat(uploadDir, (g_File_Info.strRemoteDirName).c_str(), g_File_Info.strRemoteDirName.length());
	// 远程文件名称（在当前工作目录下的绝对路径）
	strncat(uploadName, uploadDir, strlen(uploadDir));
	char name[256] = {0};
	// strcpy(name, strrchr(tar_name, '/'));
	strcpy(name, strrchr(tar_name.c_str(), '/'));
	strncat(uploadName, name, strlen(name));
	// printf("uploadName: %s\n", uploadName);
	XLOG_DEBUG("Remote file path: {}", uploadName);

	// 本地待上传文件名（绝对路径）
	char localCompressFileName[256] = {0};
	strncat(localCompressFileName, (data_absoult_path + ".owa").c_str(), (data_absoult_path + ".owa").length());

	// 未启用 SFTP
	if (false == g_Net_Info.ftp_server.bIsSFTP) {
		bool retVal = false;
		// 使用 FTP 上传
		FtpClient *pFTP = new FtpClient();

		FTP_Server ftp_server = g_Net_Info.ftp_server;
		char host[50] = {0};
		sprintf(host, "%s", ftp_server.strIP.c_str());
		sprintf(host + ftp_server.strIP.length(), "%s", ":");
		sprintf(host + ftp_server.strIP.length() + 1, "%u", ftp_server.nPort);

		// printf("FTP server	%s\n", host);
		
		retVal = pFTP->login(host, g_Net_Info.ftp_server.strUserName.c_str(), g_Net_Info.ftp_server.strPasswd.c_str());
		if (false == retVal) {
			// printf("FTP connection failed.");
			delete pFTP;
			return -1;
		}
		// printf("Connect the FTP server successfully.");

		if (false == pFTP->chdir(uploadDir) ) {
			// printf("%s not exist.\n", uploadDir);

			if (false == pFTP->mkdir(uploadDir)) {
				// printf("FTP mkdir %s failed.\n", uploadDir);
				delete pFTP;
				return -1;
			}

			if (false == pFTP->chdir(uploadDir) ) {
				// printf("FTP chdir %s failed.\n", uploadDir);
				delete pFTP;
				return -1; 
			}
		}

		retVal = pFTP->put(localCompressFileName, uploadName);
		if (false == retVal) {
			// printf("FTP upload to {%s/%s} failed.", uploadDir, uploadName);
			delete pFTP;
			return -1;
		}

		delete pFTP;
	} else {

		// 使用 SFTP 上传
		SftpClient *pSFTP = new SftpClient(g_Net_Info.ftp_server.strIP, g_Net_Info.ftp_server.nPort,
										g_Net_Info.ftp_server.strUserName, g_Net_Info.ftp_server.strPasswd);
		
		ret = pSFTP->SftpConnection();
		if (0 != ret) {
			// printf("SFTP connection failed.");
			delete pSFTP;
			return -1;
		}
		// printf("Connect the SFTP server successfully.");
		
		ret = pSFTP->SftpAuthentication();
		if (0 != ret) {
			// printf("SFTP authentication failed.");
			delete pSFTP;
			return -1;
		}

		ret = pSFTP->SftpLogin();
		if (0 != ret) {
			// printf("SFTP login failed.");
			delete pSFTP;
			return -1;
		}
		// printf("SFTP login successfully.");

		ret = pSFTP->SftpUpload(localCompressFileName, uploadDir, uploadName);
		if (0 != ret) {
			// printf("SFTP upload to {%s/%s} failed.", uploadDir, uploadName);
			delete pSFTP;
			return -1;
		}

		delete pSFTP;
	}
	
	// 上传成功，根据配置是否保留 SD 卡数据文件
	if (true == g_Net_Info.ftp_server.bDeleteAfterUpload) {
		ret = remove_recursively(localCompressFileName);
		if (ret < 0) {
			// printf("remove owa file %s error.", localCompressFileName);
		} else {
			// printf("remove owa file %s successfully.", localCompressFileName);
		}
		// printf("FTP upload to {%s/%s} succcess.", uploadDir, uploadName);
	}

	return 0;
}
