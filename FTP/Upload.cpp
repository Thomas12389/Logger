/*************************************************************************
	> File Name: sftp_test.cpp
	> Author: 
	> Mail: 
	> Created Time: Mon 28 Dec 2020 02:52:48 PM CST
 ************************************************************************/

#include <iostream>
using namespace std;

#include "SftpClient.h"
#include "DevConfig/PhyChannel.hpp"

#include "Compress/Compress.h"

#include <dirent.h>
#include <sys/stat.h>

#include "Logger/Logger.h"

extern Net_Info g_Net_Info;
extern File_Save g_File_Info;

// 文件目录
extern std::string STORGE_DIR_NAME;

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
			char sub_dir_name[256] = {0};
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


int sftp_upload() {

	// compress
	std::string ABSOULT_PATH_NAME = STORGE_DIR_NAME + g_File_Info.strLocalDirName;

	char compressName[256] = {0};
	strcpy(compressName, (char *)(g_File_Info.strRemoteFileName + '_' + g_File_Info.strLocalDirName + ".gz").c_str());
	
	int ret = 0;
    if ( 0 == compress_gz(ABSOULT_PATH_NAME.c_str(), compressName)) {
		ret = remove_recursively(ABSOULT_PATH_NAME.c_str());
		if (ret < 0) {
			XLOG_DEBUG("remove_recursively error.");
		}
		XLOG_DEBUG("compress .gz file successfully.");
	}

	SftpClient *client = new SftpClient(g_Net_Info.ftp_server.strIP, g_Net_Info.ftp_server.nPort,
									g_Net_Info.ftp_server.strUserName, g_Net_Info.ftp_server.strPasswd);
	
	ret = client->SftpConnection();
	if (0 != ret) {
		XLOG_ERROR("SFTP connection failed.");
		delete client;
		return -1;
	}
	XLOG_INFO("Connect the SFTP server successfully.");
	
	ret = client->SftpAuthentication();
	if (0 != ret) {
		XLOG_ERROR("SFTP authentication failed.");
		delete client;
		return -1;
	}

	ret = client->SftpLogin();
	if (0 != ret) {
		XLOG_ERROR("SFTP login failed.");
		delete client;
		return -1;
	}
	XLOG_INFO("SFTP login successfully.");

	// 构建上传文件名
	char uploadDir[256] = {0};
	char uploadName[256] = {0};
	strncat(uploadDir, (g_File_Info.strRemoteDirName).c_str(), g_File_Info.strRemoteDirName.length());
	strncat(uploadName, uploadDir, strlen(uploadDir));
	strncat(uploadName, compressName, strlen(compressName));
	XLOG_INFO("Remote file path: {}", uploadName);

	// 本地压缩文件名（绝对路径）
	char localCompressFileName[256] = {0};
	strncat(localCompressFileName, STORGE_DIR_NAME.c_str(), STORGE_DIR_NAME.length());
	strncat(localCompressFileName, compressName, strlen(compressName));

	ret = client->SftpUpload(localCompressFileName, uploadDir, uploadName);
	if (0 != ret) {
		XLOG_DEBUG("Sftp upload failed.");
		delete client;
		return -1;
	}
	ret = remove_recursively(localCompressFileName);
	if (ret  < 0) {
		XLOG_DEBUG("remove gz file {} error.", localCompressFileName);
	} else {
		XLOG_DEBUG("remove gz file {} successfully.", localCompressFileName);
	}
	XLOG_DEBUG("Sftp upload succcess.");
	// if (0 != client.SftpLogout()) {
	// 	printf("SftpLogout failed!\n");
	// 	return -1;
	// }

	delete client;
	return 0;
}
