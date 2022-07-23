/*************************************************************************
	> File Name: SftpClient.h
	> Author: 
	> Mail: 
	> Created Time: Mon 28 Dec 2020 02:47:08 PM CST
 ************************************************************************/

#ifndef _COMPRESS_H
#define _COMPRESS_H
/*
	├── dir
	│   ├── 0001
	│   └── 0002
	│   └── 0003
	│   └── ...
	│   └── 000n
	├── outfilename

	dir -- 临时数据文件的保存目录
	outfilename -- 压缩文件名称
*/
int compress_gz(const char *dir, char *outfilename);

#endif
