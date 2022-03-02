/*************************************************************************
	> File Name: FtpClient.cpp
	> Author: 
	> Mail: 
	> Created Time: Tue 14 Dec 2021 03:52:30 PM CST
 ************************************************************************/

#include "string.h"

#include "FtpClient.h"
#include "Logger/Logger.h"

FtpClient::FtpClient() {
	m_ftpconn = 0;
	initdata();
	FtpInit();
	
	m_connectfailed = false;
	m_loginfailed = false;
	m_optionfailed = false;
}

FtpClient::~FtpClient() {
	logout();
}

void FtpClient::initdata() {
	m_size = 0;
	memset(m_mtime, 0, sizeof(m_mtime));
}

bool FtpClient::login(const char *host, const char *username, const char *password, const int imode) {
	if (m_ftpconn != 0) {
		FtpQuit(m_ftpconn);
		m_ftpconn=0;
	}
	m_connectfailed = m_loginfailed = m_optionfailed = false;
	
	if (FtpConnect(host, &m_ftpconn) == false) {
		m_connectfailed = true;
		return false;
	}
	
	if (FtpLogin(username, password, m_ftpconn) == false) {
		m_loginfailed = true;
		return false;
	}
	
	if (FtpOptions(FTPLIB_CONNMODE, (long)imode,m_ftpconn) == false) {
		m_optionfailed = true;
		return false;
	}
	
	return true;
}

bool FtpClient::logout() {
	if (m_ftpconn == 0) return false;

	FtpQuit(m_ftpconn);

	m_ftpconn = 0;

	return true;
}

bool FtpClient::get(const char *remotefilename, const char *localfilename, const bool bCheckMTime) {
	if (m_ftpconn == 0) return false;

	// MKDIR(localfilename);

	// char strlocalfilenametmp[301];
	// memset(strlocalfilenametmp,0,sizeof(strlocalfilenametmp));
	// snprintf(strlocalfilenametmp,300,"%s.tmp",localfilename);

	// // »ñÈ¡Ô¶³Ì·þÎñÆ÷µÄÎÄŒþµÄÊ±Œä
	// if (mtime(remotefilename) == false) return false;

	// // È¡ÎÄŒþ
	// if (FtpGet(strlocalfilenametmp,remotefilename,FTPLIB_IMAGE,m_ftpconn) == false) return false;

	// // ÅÐ¶ÏÎÄŒþ»ñÈ¡Ç°ºÍ»ñÈ¡ºóµÄÊ±Œä£¬Èç¹ûÊ±Œä²»Í¬£¬±íÊŸÎÄŒþÒÑžÄ±ä£¬·µ»ØÊ§°Ü
	// if (bCheckMTime==true)
	// {
	// char strmtime[21];
	// strcpy(strmtime,m_mtime);

	// if (mtime(remotefilename) == false) return false;

	// if (strcmp(m_mtime,strmtime) != 0) return false;
	// }

	// // ÖØÖÃÎÄŒþÊ±Œä
	// UTime(strlocalfilenametmp,m_mtime);

	// // žÄÎªÕýÊœµÄÎÄŒþ
	// if (rename(strlocalfilenametmp,localfilename) != 0) return false; 

	// // »ñÈ¡ÎÄŒþµÄŽóÐ¡
	// m_size=FileSize(localfilename);

	return true;
}

bool FtpClient::mtime(const char *remotefilename) {
//   if (m_ftpconn == 0) return false;
  
//   memset(m_mtime,0,sizeof(m_mtime));
  
//   char strmtime[21];
//   memset(strmtime,0,sizeof(strmtime));

//   if (FtpModDate(remotefilename,strmtime,14,m_ftpconn) == false) return false;

//   AddTime(strmtime,m_mtime,0+8*60*60,"yyyymmddhh24miss");

	return true;
}

bool FtpClient::size(const char *remotefilename) {
	if (m_ftpconn == 0) return false;

	m_size = 0;

	if (FtpSize(remotefilename,&m_size,FTPLIB_IMAGE,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::site(const char *command) {
	if (m_ftpconn == 0) return false;

	if (FtpSite(command,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::chdir(const char *remotedir) {
	if (m_ftpconn == 0) return false;

	if (FtpChdir(remotedir,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::mkdir(const char *remotedir) {
	if (m_ftpconn == 0) return false;

	if (FtpMkdir(remotedir,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::rmdir(const char *remotedir) {
	if (m_ftpconn == 0) return false;

	if (FtpRmdir(remotedir,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::dir(const char *remotedir,const char *listfilename) {
	if (m_ftpconn == 0) return false;

	if (FtpDir(listfilename,remotedir,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::nlist(const char *remotedir,const char *listfilename) {
	// if (m_ftpconn == 0) return false;

	// // ŽŽœš±ŸµØÎÄŒþÄ¿ÂŒ
	// MKDIR(listfilename);

	// if (FtpNlst(listfilename,remotedir,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::put(const char *localfilename,const char *remotefilename,const bool bCheckSize) {
	if (m_ftpconn == 0) return false;

	char strremotefilenametmp[301];
	memset(strremotefilenametmp,0,sizeof(strremotefilenametmp));
	snprintf(strremotefilenametmp,300,"%s.tmp",remotefilename);

	if (FtpPut(localfilename,strremotefilenametmp,FTPLIB_IMAGE,m_ftpconn) == false) return false;

	if (FtpRename(strremotefilenametmp,remotefilename,m_ftpconn) == false) return false;

	// 判断已上传的文件大小和本地的是否相同
	if (bCheckSize==true) {
		if (size(remotefilename) == false) return false;

		// if (m_size != FileSize(localfilename)) return false; 
		// 获取本地文件大小
		struct stat st_filestat;
		if (stat(localfilename, &st_filestat) < 0) return false;
		return (m_size == st_filestat.st_size);
	}

	return true;
}

bool FtpClient::ftpdelete(const char *remotefilename) {
	if (m_ftpconn == 0) return false;

	if (FtpDelete(remotefilename,m_ftpconn) == false) return false;

	return true;
}

bool FtpClient::ftprename(const char *srcremotefilename,const char *dstremotefilename) {
	if (m_ftpconn == 0) return false;

	if (FtpRename(srcremotefilename,dstremotefilename,m_ftpconn) == false) return false;

	return true;
}

char *FtpClient::response() {
	if (m_ftpconn == 0) return 0;

	return FtpLastResponse(m_ftpconn);
}

