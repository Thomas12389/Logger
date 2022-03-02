/*
 * @Author: your name
 * @Date: 2021-11-08 10:09:06
 * @LastEditTime: 2021-12-14 15:20:44
 * @LastEditors: Please set LastEditors
 * @Description: 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 * @FilePath: /Remote_Recoder/FTP/Upload.h
 */
/*************************************************************************
	> File Name: SftpClient.h
	> Author: 
	> Mail: 
	> Created Time: Mon 28 Dec 2020 02:47:08 PM CST
 ************************************************************************/

#ifndef _UPLOAD_H
#define _UPLOAD_H

/**
 * @description: 使用 FTP 上传数据文件
 * @param {*}
 * @return {*}
 * 		-1	上传失败
 * 		0	FTP 未启用
 * 		1	SFTP 上传成功
 * 		2	FTP 上传成功
 */
int ftp_upload();

#endif
