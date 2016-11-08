#include <swoole/Server.hpp>
#include <iostream>

using namespace std;
using namespace swoole;

class MyServer : public Server
{
public:
    MyServer(string _host, int _port, int _mode = SW_MODE_PROCESS, int _type = SW_SOCK_TCP) :
            Server(_host, _port, _mode, _type)
    {
        SwooleG.task_worker_num = 2;
    }

    virtual void onStart();

    virtual void onShutdown(){

    }

    virtual void onWorkerStart(int worker_id){

    }

    virtual void onWorkerStop(int worker_id){

    }

    virtual void onReceive(int fd, string &data);

    virtual void onConnect(int fd);

    virtual void onClose(int fd);

    virtual void onPacket(string &data, ClientInfo &clientInfo){

    }

    virtual void onTask(int task_id, int src_worker_id, string &data);
    virtual void onFinish(int task_id, string &data);
};

void MyServer::onReceive(int fd, string &data)
{
    swConnection *conn = swWorker_get_connection(&this->serv, fd);
    printf("onReceive: fd=%d, ip=%s|port=%d Data=%s|Len=%ld\n", fd, swConnection_get_ip(conn),
           swConnection_get_port(conn), (char *) data.c_str(), data.length());

    int ret;
    char resp_data[SW_BUFFER_SIZE];
    int n = snprintf(resp_data, SW_BUFFER_SIZE, (char *) "Server: %*s\n", (int) data.length(), data.c_str());
    ret = this->send(fd, resp_data, (uint32_t) n);
    if (ret < 0)
    {
        printf("send to client fail. errno=%d\n", errno);
    }
    else
    {
        printf("send %d bytes to client success. data=%s\n", n, resp_data);
    }
    string task("hello world\n");
    this->task(task);
//    this->close(fd);
}

void MyServer::onConnect(int fd)
{
    printf("PID=%d\tConnect fd=%d\n", getpid(), fd);
}

void MyServer::onClose(int fd)
{
    printf("PID=%d\tClose fd=%d\n", getpid(), fd);
}

void MyServer::onTask(int task_id, int src_worker_id, string &data)
{
    printf("PID=%d\tTaskID=%d\n", getpid(), task_id);
}

void MyServer::onFinish(int task_id, string &data)
{
    printf("PID=%d\tClose fd=%d\n", getpid(), task_id);
}

void MyServer::onStart()
{
    printf("server start\n");
}

int main(int argc, char **argv)
{
    MyServer server("127.0.0.1", 9501);
    server.listen("127.0.0.1", 9502, SW_SOCK_UDP);
    server.listen("::1", 9503, SW_SOCK_TCP6);
    server.listen("::1", 9504, SW_SOCK_UDP6);
    server.setEvents(EVENT_onStart | EVENT_onReceive | EVENT_onClose | EVENT_onTask | EVENT_onFinish);
    server.start();
    return 0;
}
