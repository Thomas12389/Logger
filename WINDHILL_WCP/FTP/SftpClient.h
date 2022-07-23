/*************************************************************************
	> File Name: SftpClient.h
	> Author: 
	> Mail: 
	> Created Time: Mon 28 Dec 2020 02:47:08 PM CST
 ************************************************************************/

#ifndef _SFTPCLIENT_H
#define _SFTPCLIENT_H

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <iostream>
#include <mutex>
#include <arpa/inet.h>
using namespace std;

#define sftppath    "/tmp/test.gz"


class SftpClient
{
private:
    const char *host;
    const uint16_t port;
    const char *username;
    const char *password;
    const char *keyfile1 = "~/.ssh/id_rsa.pub";
    const char *keyfile2 = "~/.ssh/id_rsa";

	unsigned long hostaddr;
    int sock, i, auth_pw = 0;
    struct sockaddr_in sin;
    const char *fingerprint;
    char *userauthlist;
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SFTP_HANDLE *sftp_handle;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    // FILE *pFlocal;
    char mem[1024*100];
    // size_t nread;
    // char *ptr;

public:
    SftpClient(std::string strIP, uint16_t nPort, std::string strUserName, std::string strPasswd);
	~SftpClient();

    int SftpSessionDisconnect();
    int SftpConnection();

    int SftpAuthentication();
    int SftpLogin();
    int SftpLogout();

    int SftpUpload(const char* LocalFile, const char* uploadDir, const char* RemoteFile = sftppath);
    
};

#endif
