#include<stdio.h>
#include "server.h"

int main()
{
    int lfd = initListenFd(9999);   //parm端口
    runEpoll(lfd);
    return 0;
}