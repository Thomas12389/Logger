/*************************************************************************
	> File Name: SftpClient.cpp
	> Author: 
	> Mail: 
	> Created Time: Fri 25 Dec 2020 05:48:34 PM CST
 ************************************************************************/
#include "SftpClient.h"

#include "Logger/Logger.h"

SftpClient::SftpClient(std::string strIP, uint16_t nPort, std::string strUserName, std::string strPasswd)
    : host(strIP.c_str())
    , port(nPort)
    , username(strUserName.c_str())
    , password(strPasswd.c_str())
    , session(nullptr)
    , sftp_session(nullptr)
{
}

SftpClient::~SftpClient() {
    // printf("xxxxxxxxxxxxxxxx\n");
    SftpSessionDisconnect();
}

static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&fd);

    FD_SET(socket_fd, &fd);

    /* now make sure we wait in the correct direction */
    dir = libssh2_session_block_directions(session);

    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        readfd = &fd;

    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        writefd = &fd;

    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

    return rc;
}

int SftpClient::SftpSessionDisconnect() {
    if(session) {
        libssh2_session_disconnect(session, "Normal Shutdown");
        libssh2_session_free(session);
    }

    close(sock);

    fprintf(stderr, "all done\n");

    libssh2_exit();
    return 0;
}

int SftpClient::SftpConnection() {
    unsigned long hostaddr = inet_addr(host);

	int iReturnCode = libssh2_init(0);
	if (LIBSSH2_ERROR_NONE != iReturnCode) {
		XLOG_DEBUG("libssh2 initialization failed {:d}", iReturnCode);
        return -1;
	}

	/*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
	struct sockaddr_in sin;
    sock = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = hostaddr;
    if(connect(sock, (struct sockaddr*)(&sin),
               sizeof(struct sockaddr_in)) != 0) {
        XLOG_DEBUG("Failed to connect the FTP server.");
        return -1;
    }
    return 0;
}

int SftpClient::SftpAuthentication() {
    /* Create a session instance */
    session = libssh2_session_init();
    if(!session) {
        XLOG_DEBUG("libssh2_session_init ERROR.");
        return -1;
    }

    /* Since we have set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking(session, 1);

    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    int iReturnCode = libssh2_session_handshake(session, sock);
    if(iReturnCode) {
        XLOG_DEBUG("Failure establishing SSH session: {:d}", iReturnCode);
        return -1;
    }

    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    const char *fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    char fingerNum[25] = {0};
    for(int i = 0; i < 20; i++) {
        sprintf(fingerNum, "%02X", (char)fingerprint[i]);
    }
    // XLOG_DEBUG("Fingerprint: {}", fingerNum);

    /* check what authentication methods are available */
    char *userauthlist = libssh2_userauth_list(session, username, strlen(username));
    XLOG_DEBUG("Authentication methods: {}", userauthlist);
    if(strstr(userauthlist, "password") != NULL) {
        auth_pw |= 1;
    }
    if(strstr(userauthlist, "keyboard-interactive") != NULL) {
        auth_pw |= 2;
    }
    if(strstr(userauthlist, "publickey") != NULL) {
        auth_pw |= 4;
    }
    return 0;
}

int SftpClient::SftpLogin() {
    if(auth_pw & 1) {
        /* We could authenticate via password */
        if(libssh2_userauth_password(session, username, password)) {
            XLOG_DEBUG("Authentication by password failed.");
            SftpSessionDisconnect();
            return -1;
        }
    }

    fprintf(stderr, "libssh2_sftp_init()!\n");
    sftp_session = libssh2_sftp_init(session);
    if(!sftp_session) {
        fprintf(stderr, "Unable to init SFTP session\n");
        SftpSessionDisconnect();
        return -1;
    }
    return 0;
}

int SftpClient::SftpUpload(const char* LocalFile, const char* uploadDir, const char* RemoteFile) {
    FILE *pFlocal = fopen(LocalFile, "rb");
    if(!pFlocal) {
        XLOG_DEBUG("Can't open local file {}", LocalFile);
        return -1;
    }

    fseek(pFlocal, 0, SEEK_END);
    long totalBytes = ftell(pFlocal);

    /* mkdir */
    int ret = 0;
    if (nullptr == libssh2_sftp_opendir(sftp_session, uploadDir)) {
        ret = libssh2_sftp_mkdir(sftp_session, uploadDir, 0755);
        if (ret < 0) {
            fprintf(stderr, "libssh2_sftp_mkdir failed, error_no: %d\n", ret);
            SftpSessionDisconnect();
            return -1;
        }
    }

    /* Request a file via SFTP */
    XLOG_DEBUG("libssh2_sftp_open().");
    sftp_handle =
        libssh2_sftp_open(sftp_session, RemoteFile,
                      LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE | LIBSSH2_FXF_READ,
                      LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR|
                      LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
    
    ret = libssh2_sftp_fstat(sftp_handle, &attrs);
    if( ret < 0) {
        XLOG_DEBUG("libssh2_sftp_fstat_ex failed, error_no: {:d}", ret);
        SftpSessionDisconnect();
        return -1;
    } else {
        libssh2_sftp_seek64(sftp_handle, attrs.filesize);
        XLOG_DEBUG("Did a seek to position {:d}", (long) attrs.filesize);
    }

    XLOG_DEBUG("libssh2_sftp_open() a handle for APPEND.");

    if(!sftp_handle) {
        XLOG_DEBUG("Unable to open file with SFTP.");
        SftpSessionDisconnect();
        return -1;
    }
    XLOG_DEBUG("libssh2_sftp_open() is done, now send data.");
    size_t nread = 0;
    ssize_t nwrite = 0;
    char *ptr = NULL;

    long sendBytes = 0;

    // 移动到上次传输的位置
    fseeko64(pFlocal, attrs.filesize, SEEK_SET);
    do {
        nread = fread(mem, 1, sizeof(mem), pFlocal);
        if(nread <= 0) {
            /* end of file */
            break;
        }
        ptr = mem;
        sendBytes = nread;

        do {
            /* write data in a loop until we block */
            nwrite = libssh2_sftp_write(sftp_handle, ptr, nread);
            if(nwrite < 0) {
                if (nwrite == LIBSSH2_ERROR_EAGAIN) {
                    waitsocket(sock, session);
                } else {
                    break;
                }
            }
            ptr += nwrite;
            nread -= nwrite;
        } while(nread);

        // 仅调试使用
        totalBytes -= sendBytes;
        XLOG_DEBUG("send {} bytes, left {} bytes.", sendBytes, totalBytes);
    } while(nwrite > 0);

    fclose(pFlocal);

    libssh2_sftp_close(sftp_handle);
    libssh2_sftp_shutdown(sftp_session);
    return 0;
}