#include <unistd.h>
#include "server/webserver.h"

int main()
{
    WebServer server(
        1316, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "root", "root", "webserver", /* Mysql配置 */
        8, 4);                             /* 连接池数量 线程池数量*/
    server.Start();
}
