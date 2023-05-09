#include "server.h"

struct fdInfo
{
    int fd;
    int epfd;
    pthread_t tid;
};

int initListenFd(uint16_t port)
{
    // 1、创建监听的套接字
    // AF_INET IPV4 / AF_INET6 IPV6 / SOCK_STREAM 流式协议  / SOCK_DGRAM 报式协议 /0分别对应了TCP和UDP
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        return -1;
    }
    // 2、设置端口复用
    int opt = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        perror("setsockopt");
        return -1;
    }
    // 3、绑定ip和端口
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1)
    {
        perror("bind");
        return -1;
    }
    // 4、监听
    ret = listen(fd, 128);
    if (ret == -1)
    {
        perror("listen");
        return -1;
    }
    // printf("监听文件描述符初始化成功\n");
    return fd;
}

int runEpoll(int lfd)
{
    int epollfd = epoll_create(1);
    if (epollfd == -1)
    {
        perror("epoll_create");
        return -1;
    }
    struct epoll_event epollEvent;
    epollEvent.data.fd = lfd;
    epollEvent.events = EPOLLIN;
    int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, lfd, &epollEvent);
    if (ret == -1)
    {
        perror("epoll_ctl");
        return -1;
    }
    struct epoll_event evts[1024];
    int size = sizeof(evts) / sizeof(struct epoll_event);
    // printf("epoll启动成功\n");
    while (1)
    {
        // break;
        int num = epoll_wait(epollfd, evts, size, -1);
        for (int i = 0; i < num; i++)
        {
            struct fdInfo *info = (struct fdInfo*)malloc(sizeof(struct fdInfo));
            int fd = evts[i].data.fd;
            info->epfd = epollfd;
            info->fd = fd;
            if (fd == lfd)
            {
                // 建立新连接
                // printf("建立新连接\n");
                //accpetFd(lfd, epollfd);
                pthread_create(&info->tid,0,&accpetFd,info);
            }
            else
            {
                // printf("接收请求\n");
                //recvHttpRequest(fd, epollfd);
                pthread_create(&info->tid,0,&recvHttpRequest,info);
            }
        }
    }
    return 0;
}

void* accpetFd(void* arg)
{
    struct fdInfo* info = (struct fdInfo*) arg;
    int lfd = info->fd;
    int epollfd = info->epfd;
    // printf("添加文件描述符\n");
    int cfd = accept(lfd, 0, 0);
    if (cfd == -1)
    {
        perror("accpet");
        return 0;
    }
    // int flag = fcntl(cfd,F_GETFL);  //得到当前设置
    // flag |= O_NONBLOCK;     //设置非阻塞
    // fctnl(cfd,F_SETFL);     //设置进去
    fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | O_NONBLOCK);

    struct epoll_event epollEvent;
    epollEvent.data.fd = cfd;
    epollEvent.events = EPOLLIN | EPOLLET; // 边沿触发模式
    int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, cfd, &epollEvent);
    if (ret == -1)
    {
        perror("epoll_ctl");
        return 0;
    }
    free(info);
    return 0;
}

void* recvHttpRequest(void* arg)
{
    // printf("开始接收数据\n");
    struct fdInfo* info = (struct fdInfo*)arg;
    int cfd = info->fd;
    int epfd = info->fd;
    int len = 0;
    char buff[4096] = {0};
    char tmp[1024] = {0};
    int cur = 0;
    while ((len = recv(cfd, tmp, sizeof(tmp), 0)) > 0)
    {
        if (cur + len < sizeof(buff))
        {
            memcpy(buff + cur, tmp, len);
        }
        cur += len;
    }
    if (len == -1 && errno == EAGAIN)
    {
        // 解析
        char *pt = strstr(buff, "\r\n");
        int reqLen = buff - pt;
        buff[reqLen] = '\0';
        // printf("解析请求\n");
        parseRequestLine(buff, cfd);
    }
    else if (len == 0)
    {
        // 客户端已经断开了连接
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, 0);
        close(cfd);
    }
    else
    {
        perror("recv");
        return 0;
    }
    free(info);
    return 0;
}

int parseRequestLine(const char *line, int cfd)
{
    char method[12];
    char path[1024];
    char version[128];
    sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, version);
    // 如果不是get请求   暂时不处理
    if (strcasecmp(method, "get") != 0)
    {
        return -1;
    }
    char *file;
    // 处理客户端请求的文件
    if (strcasecmp(path, "/") == 0)
    {
        file = "./";
    }
    else
    {
        // 把第一个字符去掉
        file = path + 1;
    }
    // 获取文件的属性
    struct stat sta;
    int ret = stat(file, &sta);
    // printf("回复了文件\n");
    if (ret == -1)
    {
        // 文件不存在回复404
        char *file404 = "./404.html";
        struct stat sta1;
        stat(file404, &sta1);
        sendFileHeader(cfd, 404, "not found", "text/html", sta1.st_size);
        sendFile("./404.html", cfd);
    }
    else if (S_ISDIR(sta.st_mode))
    {
        // 是目录 把首页发送给客户端
        char *fileindex = "./index.html";
        struct stat sta2;
        stat(fileindex, &sta2);
        sendFileHeader(cfd, 200, "OK", "text/html", sta2.st_size);
        sendFile("index.html", cfd);
    }
    else
    {
        // 是文件 把文件内容发送给客户端
        sendFileHeader(cfd, 200, "OK", getFileType(file), sta.st_size);
        sendFile(file, cfd);
    }
    // printf("文件回复成功\n");
    return 0;
}

int sendFile(const char *fileName, int cfd)
{
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
#if 0
    while (1)
    {
        char buff[1024];
        int len = read(fd, buff, sizeof(buff));
        if (len > 0)
        {
            send(cfd, buff, len, 0);
            usleep(10);
        }
        else if (len == 0)
        {
            break;
        }
        else
        {
            perror("read");
        }
    }
#else
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    while (offset < size)
    {
        sendfile(cfd, fd, &offset, size - offset);
    }
#endif
    close(fd);
    return 0;
}

int sendFileHeader(int cfd, int status, const char *descr, const char *type, int length)
{
    char buff[4096] = {0};
    // 状态行
    sprintf(buff, "http/1.1 %d %s\r\n", status, descr);
    // 响应头
    sprintf(buff + strlen(buff), "Content-type: %s\r\n", type);
    sprintf(buff + strlen(buff), "Content-length: %d\r\n\r\n", length);
    send(cfd, buff, strlen(buff), 0);
    int size = strlen(buff);
    // printf("响应头的大小位:%d\n", size);
    return 0;
}

int sendDir(const char *dirName, int cfd)
{
    return 0;
}

const char *getFileType(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
