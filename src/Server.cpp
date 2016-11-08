#include "Server.hpp"

#include <iostream>

using namespace std;

namespace swoole
{
    Server::Server(string _host, int _port, int _mode, int _type)
    {
        host = _host;
        port = _port;
        mode = _mode;

        swServer_init(&serv);

        if (_mode == SW_MODE_SINGLE)
        {
            serv.reactor_num = 1;
            serv.worker_num = 1;
        }

//        serv.reactor_num = 4;
//        serv.worker_num = 2;
        serv.factory_mode = (uint8_t) mode;
        //serv.factory_mode = SW_MODE_SINGLE; //SW_MODE_PROCESS/SW_MODE_THREAD/SW_MODE_BASE/SW_MODE_SINGLE
//        serv.max_connection = 10000;
        //serv.open_cpu_affinity = 1;
        //serv.open_tcp_nodelay = 1;
        //serv.daemonize = 1;
//	memcpy(serv.log_file, SW_STRL("/tmp/swoole.log")); //日志

        serv.dispatch_mode = 2;
//	serv.open_tcp_keepalive = 1;

#ifdef HAVE_OPENSSL
        //serv.ssl_cert_file = "tests/ssl/ssl.crt";
        //serv.ssl_key_file = "tests/ssl/ssl.key";
        //serv.open_ssl = 1;
#endif
        //create Server
        int ret = swServer_create(&serv);
        if (ret < 0)
        {
            swTrace("create server fail[error=%d].\n", ret);
            exit(0);
        }
        this->listen(host, port, _type);
    }

    void Server::setEvents(int _events)
    {
        events = _events;
    }

    bool Server::listen(string host, int port, int type)
    {
        auto ls = swServer_add_port(&serv, type, (char *) host.c_str(), port);
        if (ls == NULL)
        {
            return false;
        } else
        {
            ports.push_back(ls);
            return true;
        }
    }

    bool Server::send(int fd, string &data)
    {
        if (SwooleGS->start == 0)
        {
            return false;
        }
        if (data.length() <= 0)
        {
            return false;
        }
        return serv.send(&serv, fd, (char *) data.c_str(), data.length()) == SW_OK ? true : false;
    }

    bool Server::send(int fd, const char *data, int length)
    {
        if (SwooleGS->start == 0)
        {
            return false;
        }
        if (length <= 0)
        {
            return false;
        }
        return serv.send(&serv, fd, (char *) data, length) == SW_OK ? true : false;
    }

    bool Server::close(int fd, bool reset)
    {
        if (SwooleGS->start == 0)
        {
            return false;
        }
        if (swIsMaster())
        {
            return false;
        }

        swConnection *conn = swServer_connection_verify_no_ssl(&serv, fd);
        if (!conn)
        {
            return false;
        }

        //Reset send buffer, Immediately close the connection.
        if (reset)
        {
            conn->close_reset = 1;
        }

        int ret;
        if (!swIsWorker())
        {
            swWorker *worker = swServer_get_worker(&serv, conn->fd % serv.worker_num);
            swDataHead ev;
            ev.type = SW_EVENT_CLOSE;
            ev.fd = fd;
            ev.from_id = conn->from_id;
            ret = swWorker_send2worker(worker, &ev, sizeof(ev), SW_PIPE_MASTER);
        }
        else
        {
            ret = serv.factory.end(&serv.factory, fd);
        }
        return ret == SW_OK;
    }

    static int task_id = 0;

    static int task_pack(swEventData *task, DataBuffer &data)
    {
        task->info.type = SW_EVENT_TASK;
        //field fd save task_id
        task->info.fd = task_id++;
        //field from_id save the worker_id
        task->info.from_id = SwooleWG.id;
        swTask_type(task) = 0;

        if (data.length >= SW_IPC_MAX_SIZE - sizeof(task->info))
        {
            if (swTaskWorker_large_pack(task, (char *) data.buffer, (int) data.length) < 0)
            {
                swWarn("large task pack failed()");
                return SW_ERR;
            }
        }
        else
        {
            memcpy(task->data, (char *) data.buffer, data.length);
            task->info.len = (uint16_t) data.length;
        }
        return task->info.fd;
    }

    static DataBuffer task_unpack(swEventData *task_result)
    {
        char *result_data_str;
        int result_data_len = 0;
        int data_len;
        char *data_str = NULL;
        DataBuffer retval;

        if (swTask_type(task_result) & SW_TASK_TMPFILE)
        {
            swTaskWorker_large_unpack(task_result, malloc, data_str, data_len);
            /**
             * unpack failed
             */
            if (data_len == -1)
            {
                if (data_str != NULL)
                {
                    free(data_str);
                }
                return retval;
            }
            result_data_str = data_str;
            result_data_len = data_len;
        }
        else
        {
            result_data_str = task_result->data;
            result_data_len = task_result->info.len;
        }

        retval.copy(result_data_str, (size_t) result_data_len);
        return retval;
    }


    static DataBuffer get_recv_data(swEventData *req, char *header, uint32_t header_length)
    {
        char *data_ptr = NULL;
        int data_len;
        DataBuffer retval;

#ifdef SW_USE_RINGBUFFER
        swPackage package;
        if (req->info.type == SW_EVENT_PACKAGE)
        {
            memcpy(&package, req->data, sizeof (package));
            data_ptr = package.data;
            data_len = package.length;
        }
#else
        if (req->info.type == SW_EVENT_PACKAGE_END)
        {
            swString *worker_buffer = swWorker_get_buffer(SwooleG.serv, req->info.from_id);
            data_ptr = worker_buffer->str;
            data_len = worker_buffer->length;
        }
#endif
        else
        {
            data_ptr = req->data;
            data_len = req->info.len;
        }

        if (header_length >= data_len)
        {
            return retval;
        }
        else
        {
            retval.copy(data_ptr + header_length, data_len - header_length);
        }

        if (header_length > 0)
        {
            memcpy(header, data_ptr, header_length);
        }

#ifdef SW_USE_RINGBUFFER
        if (req->info.type == SW_EVENT_PACKAGE)
        {
            swReactorThread *thread = swServer_get_thread(SwooleG.serv, req->info.from_id);
            thread->buffer_input->free(thread->buffer_input, data_ptr);
        }
#endif
        return retval;
    }

    static int check_task_param(int dst_worker_id)
    {
        if (SwooleG.task_worker_num < 1)
        {
            swWarn("Task method cannot use, Please set task_worker_num.");
            return SW_ERR;
        }
        if (dst_worker_id >= SwooleG.task_worker_num)
        {
            swWarn("worker_id must be less than serv->task_worker_num.");
            return SW_ERR;
        }
        if (!swIsWorker())
        {
            swWarn("The method can only be used in the worker process.");
            return SW_ERR;
        }
        return SW_OK;
    }

    int Server::task(DataBuffer &data, int dst_worker_id)
    {
        if (SwooleGS->start == 0)
        {
            swWarn("Server is not running.");
            return false;
        }

        swEventData buf;
        if (check_task_param(dst_worker_id) < 0)
        {
            return false;
        }

        if (task_pack(&buf, data) < 0)
        {
            return false;
        }

        swTask_type(&buf) |= SW_TASK_NONBLOCK;
        if (swProcessPool_dispatch(&SwooleGS->task_workers, &buf, &dst_worker_id) >= 0)
        {
            sw_atomic_fetch_add(&SwooleStats->tasking_num, 1);
            return buf.info.fd;
        }
        else
        {
            return -1;
        }
    }

    bool Server::sendto(string &ip, int port, string &data, int server_socket)
    {
        if (SwooleGS->start == 0)
        {
            return false;
        }
        if (data.length() <= 0)
        {
            return false;
        }
        bool ipv6 = false;
        if (strchr(ip.c_str(), ':'))
        {
            ipv6 = true;
        }

        if (ipv6 && serv.udp_socket_ipv6 <= 0)
        {
            return false;
        }
        else if (serv.udp_socket_ipv4 <= 0)
        {
            swWarn("You must add an UDP listener to server before using sendto.");
            return false;
        }

        if (server_socket < 0)
        {
            server_socket = ipv6 ? serv.udp_socket_ipv6 : serv.udp_socket_ipv4;
        }

        int ret;
        if (ipv6)
        {
            ret = swSocket_udp_sendto6(server_socket, (char *) ip.c_str(), port, (char *) data.c_str(), data.length());
        }
        else
        {
            ret = swSocket_udp_sendto(server_socket, (char *) ip.c_str(), port, (char *) data.c_str(), data.length());
        }
        return ret == SW_OK;
    }

    bool Server::start(void)
    {
        serv.ptr2 = this;
        if (this->events & EVENT_onStart)
        {
            serv.onStart = Server::_onStart;
        }
        if (this->events & EVENT_onShutdown)
        {
            serv.onShutdown = Server::_onShutdown;
        }
        if (this->events & EVENT_onConnect)
        {
            serv.onConnect = Server::_onConnect;
        }
        if (this->events & EVENT_onReceive)
        {
            serv.onReceive = Server::_onReceive;
        }
        if (this->events & EVENT_onPacket)
        {
            serv.onPacket = Server::_onPacket;
        }
        if (this->events & EVENT_onClose)
        {
            serv.onClose = Server::_onClose;
        }
        if (this->events & EVENT_onWorkerStart)
        {
            serv.onWorkerStart = Server::_onWorkerStart;
        }
        if (this->events & EVENT_onWorkerStop)
        {
            serv.onWorkerStop = Server::_onWorkerStop;
        }
        if (this->events & EVENT_onTask)
        {
            serv.onTask = Server::_onTask;
        }
        if (this->events & EVENT_onFinish)
        {
            serv.onFinish = Server::_onFinish;
        }
        int ret = swServer_start(&serv);
        if (ret < 0)
        {
            swTrace("start server fail[error=%d].\n", ret);
            return false;
        }
        return true;
    }

    int Server::_onReceive(swServer *serv, swEventData *req)
    {
        DataBuffer data = get_recv_data(req, NULL, 0);
        Server *_this = (Server *) serv->ptr2;
        _this->onReceive(req->info.fd, data);
        free(data.buffer);
        return SW_OK;
    }

    void Server::_onWorkerStart(swServer *serv, int worker_id)
    {
        Server *_this = (Server *) serv->ptr2;
        _this->onWorkerStart(worker_id);
    }

    void Server::_onWorkerStop(swServer *serv, int worker_id)
    {
        Server *_this = (Server *) serv->ptr2;
        _this->onWorkerStop(worker_id);
    }

    int Server::_onPacket(swServer *serv, swEventData *req)
    {
        swDgramPacket *packet;

        swString *buffer = swWorker_get_buffer(serv, req->info.from_id);
        packet = (swDgramPacket *) buffer->str;

        char *data = NULL;
        int length = 0;
        ClientInfo clientInfo;
        clientInfo.server_socket  = req->info.from_fd;

        //udp ipv4
        if (req->info.type == SW_EVENT_UDP)
        {
            struct in_addr sin_addr;
            sin_addr.s_addr = packet->addr.v4.s_addr;
            char *tmp = inet_ntoa(sin_addr);
            memcpy(clientInfo.address, tmp, strlen(tmp));
            data = packet->data;
            length = packet->length;
            clientInfo.port = packet->port;
        }
            //udp ipv6
        else if (req->info.type == SW_EVENT_UDP6)
        {
            inet_ntop(AF_INET6, &packet->addr.v6, clientInfo.address, sizeof( clientInfo.address));
            data = packet->data;
            length = packet->length;
            clientInfo.port = packet->port;
        }
            //unix dgram
        else if (req->info.type == SW_EVENT_UNIX_DGRAM)
        {
            memcpy(clientInfo.address, packet->data, packet->addr.un.path_length);
            data = packet->data + packet->addr.un.path_length;
            length = packet->length - packet->addr.un.path_length;
        }

        DataBuffer _data;
        _data.copy(data, length);

        Server *_this = (Server *) serv->ptr2;
        _this->onPacket(_data, clientInfo);

        return SW_OK;
    }

    void Server::_onStart(swServer *serv)
    {
        Server *_this = (Server *) serv->ptr2;
        _this->onStart();
    }

    void Server::_onShutdown(swServer *serv)
    {
        Server *_this = (Server *) serv->ptr2;
        _this->onShutdown();
    }

    void Server::_onConnect(swServer *serv, swDataHead *info)
    {
        Server *_this = (Server *) serv->ptr2;
        _this->onConnect(info->fd);
    }

    void Server::_onClose(swServer *serv, swDataHead *info)
    {
        Server *_this = (Server *) serv->ptr2;
        _this->onClose(info->fd);
    }

    int Server::_onTask(swServer *serv, swEventData *task)
    {
        Server *_this = (Server *) serv->ptr2;
        DataBuffer data = task_unpack(task);
        _this->onTask(task->info.fd, task->info.from_fd, data);
        return SW_OK;
    }

    int Server::_onFinish(swServer *serv, swEventData *task)
    {
        Server *_this = (Server *) serv->ptr2;
        DataBuffer data = task_unpack(task);
        _this->onFinish(task->info.fd, data);
        return SW_OK;
    }
}