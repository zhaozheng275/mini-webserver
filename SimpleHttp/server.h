#ifndef SERVER_H
#define SERVER_H

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>

int initListenFd(uint16_t port); // 初始化监听的文件描述符
// 启动epoll
int runEpoll(int lfd);
// 增加fd到epoll
//int accpetFd(int lfd, int epollFd);
void* accpetFd(void* arg);
// 接收http请求
//int recvHttpRequest(int cfd, int epfd);
void* recvHttpRequest(void* arg);
// 解析请求首行
int parseRequestLine(const char *line, int cfd);
// 发送文件
int sendFile(const char *fileName, int cfd);
// 发送响应头g
int sendFileHeader(int cfd, int status, const char *descr, const char *type, int length);
//发送目录
int sendDir(const char * dirName,int cfd);
//解析文件格式
const char* getFileType(const char* name);
#endif