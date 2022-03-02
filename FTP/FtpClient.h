
/*************************************************************************
	> File Name: FtpClient.h
	> Author: 
	> Mail: 
	> Created Time: Tue 14 Dec 2021 03:52:14 PM CST
 ************************************************************************/

#ifndef _FTPCLIENT_H
#define _FTPCLIENT_H

#include "ftplib.h"

class FtpClient
{
public:
    netbuf *m_ftpconn;   // FTP 连接句柄
    unsigned int m_size; // 文件大小，Byte
    char m_mtime[21];    // 文件修改时间，yyyymmddhh24miss
    
    // 存放登录失败的原因
    bool m_connectfailed;    // 连接失败
    bool m_loginfailed;      // 登录失败，用户名、秘密错误，或无权限
    bool m_optionfailed;     // 设置传输模式失败
    
    FtpClient();
    ~FtpClient();
    
    void initdata();   // 初始化 m_size 和 m_mtime 
    /**
     * @description: 
     * @param {char} *host: FTP ip地址及端口号（ip:port）
     * @param {char} *username：用户名
     * @param {char} *password：密码
     * @param {int} imode：传输模式，FTPLIB_PASSIVE -- 被动模式，FTPLIB_PORT 主动模式
     * @return {*}：true/false -- 成功/失败
     */    
    bool login(const char *host,const char *username,const char *password,const int imode = FTPLIB_PASSIVE);
    
    // 注销
    bool logout();
    /**
     * @description: 获取 FTP 服务器上文件的时间
     * @param {char} *remotefilename
     * @return {*}：true/false -- 成功/失败，时间存放在 m_mtime 中
     */    
    bool mtime(const char *remotefilename);
    /**
     * @description: 获取 FTP 服务器上文件的大小
     * @param {char} *remotefilename
     * @return {*}：true/false -- 成功/失败，大小存放在 m_size 中
     */    
    bool size(const char *remotefilename);
    // 改变 FTP 服务器当前工作目录
    bool chdir(const char *remotedir);
    // 在 FTP 服务器上创建目录
    bool mkdir(const char *remotedir);
    // 删除 FTP 目录
    bool rmdir(const char *remotedir);
    // 列出 FTP 目录和文件
    bool nlist(const char *remotedir,const char *listfilename);
    /**
     * @description: 从 FTP 获取文件
     * @param {char} *remotefilename
     * @param {char} *localfilename
     * @param {bool} bCheckMTime：传输完成后，是否比较时间
     * @return {*}
     */    
    bool get(const char *remotefilename,const char *localfilename,const bool bCheckMTime=true);
    /**
     * @description: 向 FTP 发送文件
     * @param {char} *localfilename：本地待发送文件
     * @param {char} *remotefilename：发送到 FTP 文件名
     * @param {bool} bCheckSize：传输完成后，是否比较大小
     * @return {*}
     */    
    bool put(const char *localfilename,const char *remotefilename,const bool bCheckSize=true);
    // 删除 FTP 上的文件
    bool ftpdelete(const char *remotefilename);
    // 重命名 FTP 上的文件
    bool ftprename(const char *srcremotefilename,const char *dstremotefilename);
    // 
    bool dir(const char *remotedir,const char *listfilename);
    // 向 FTP 服务器发送 site 命令  
    bool site(const char *command);
    // 获取服务器返回信息的最后一条
    char *response();
};

#endif
