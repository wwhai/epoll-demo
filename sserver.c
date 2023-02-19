#include "sserver.h"
#include <pthread.h>
/**
 *
 * */
void set_no_block(int fd)
{
    int fl = fcntl(fd, F_GETFL);
    if (fl < 0)
    {
        log_debug("fcntl");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, fl | O_NONBLOCK))
    {
        log_error("fcntl");
        exit(1);
    }
}
/**
 * init_socket
 * */
char *format_type(int type)
{
    if (SOCK_STREAM == type)
    {
        return "TCP";
    }
    if (SOCK_DGRAM == type)
    {
        return "UCP";
    }
    log_error("format_type, unknown protocol type: %d", type);
    exit(1);
}
int init_socket(char *ip, int port, int type)
{
    int listen_socket = socket(AF_INET, type, 0);
    if (listen_socket < 0)
    {
        log_error("init_socket: %d", errno);
        exit(1);
    }
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(ip);
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(listen_socket, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        log_error("bind: %d", errno);
        exit(1);
    }
    if (SOCK_STREAM == type)
    {
        if (listen(listen_socket, 5) < 0)
        {
            log_error("listen: %d", errno);
            exit(3);
        }
        log_debug("Server started %s@[%s:%d]", format_type(type), ip, port);
    }

    return listen_socket;
}
/**
 * init_tcp_socket
 * */
int init_tcp_socket(char *ip, int port)
{
    return init_socket(ip, port, SOCK_STREAM);
}

/**
 *
 * */

int init_epoll(int listen_socket)
{
    int epoll_fd = epoll_create(1);
    if (epoll_fd < 0)
    {
        log_error("epoll_create: %d", errno);
        exit(errno);
    }
    set_no_block(listen_socket);
    struct epoll_event e_event;
    e_event.events = EPOLLIN | EPOLLET;
    e_event.data.fd = listen_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket, &e_event) < 0)
    {
        log_error("epoll_ctl: %d", errno);
        exit(errno);
    }
    return epoll_fd;
}

void start_epoll(int epoll_fd, int listen_socket)
{
    if (epoll_fd < 0)
    {
        log_error("init_epoll: %d", errno);
        exit(errno);
    }
    // 保存客户端信息
    struct sockaddr_in client_sockaddr_in;
    socklen_t len = sizeof(client_sockaddr_in);

    while (1)
    {
        int ready_fd_count = epoll_wait(epoll_fd, g_events, MAX_WAIT_FD_NUM, TIMEOUT);
        switch (ready_fd_count)
        {
        case -1:
            break;
        case 0:
            break;
        default:
        {
            for (int i = 0; i < ready_fd_count; ++i)
            {
                th_args *args = (th_args *)malloc(sizeof(th_args));
                if (g_events[i].data.fd == listen_socket && g_events[i].events & EPOLLIN)
                {
                    //
                    // 新连接加入
                    //
                    int new_socket = accept(listen_socket, (struct sockaddr *)&client_sockaddr_in, &len);
                    if (new_socket < 0)
                    {
                        continue;
                    }
                    set_no_block(new_socket);
                    //
                    struct epoll_event e_event;
                    e_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    e_event.data.fd = new_socket;
                    if (epoll_add_fd(epoll_fd, new_socket, e_event) < 0)
                    {
                        continue;
                    }
                    // TODO 起一个定时器检查这个客户端的CONN包,超时时间5秒
                    add_new_connection(new_socket);
                    // log_debug("Client[%s:%d] connected", inet_ntoa(client_sockaddr_in.sin_addr), ntohs(client_sockaddr_in.sin_port));
                }
                else
                {
                    //
                    // 老连接发消息
                    //
                    int old_socket = g_events[i].data.fd;
                    if (old_socket < 0)
                    {
                        continue;
                    }
                    if (g_events[i].events & (EPOLLRDHUP | EPOLLHUP))
                    {
                        epoll_del_fd(epoll_fd, old_socket);
                        free(args);
                        log_debug("Client Disconnect: [%s:%d]", inet_ntoa(client_sockaddr_in.sin_addr), ntohs(client_sockaddr_in.sin_port));
                        continue;
                    }
                    // EPOLLIN: 表示进来的消息
                    if (g_events[i].events & EPOLLIN)
                    {
                        args->socket_fd = old_socket,
                        bzero(args->recv_buffer, sizeof(args->recv_buffer));
                        args->recv_len = recv(old_socket, args->recv_buffer, RECV_BUFFER_LEN, 0);
                        // recv_len <0 出错 ；=0 连接关闭 ；>0 接收到数据大小
                        if (args->recv_len == 0)
                        {
                            // log_debug("TCP Closed:%d", errno);
                            break;
                        }
                        if (args->recv_len < 0)
                        {
                            log_error("TCP error:%d", errno);
                            break;
                        }
                        // 缓冲区溢出 直接丢包
                        if (args->recv_len > RECV_BUFFER_LEN)
                        {
                            bzero(&args->recv_buffer, sizeof(&args->recv_buffer));
                            log_error("recv_buffer overflow: %d, :%d", args->recv_len, RECV_BUFFER_LEN);
                            continue;
                        }
                        // 这个问题知道怎么回事了 主要是持续不断的流量输入问题
                        // 解决办法：把进来的流量保存到某个全局地方
                        // log_debug(">> BEGIN PKT------------------------------");
                        // for (size_t i = 0; i < args->recv_len; i++)
                        // {
                        //     log_debug("PKt[%d]  -> [%04X]", i, args->recv_buffer[i]);
                        // }
                        // log_debug(">> END PKT------------------------------");
                        // 线程池
                        thpool_add_work(thpool, parse_packet, (void *)args);
                        struct epoll_event new_event = {
                            .data.fd = old_socket,
                            .events = EPOLLIN | EPOLLET | EPOLLOUT,
                        };
                        epoll_mod_fd(epoll_fd, old_socket, new_event);
                    }
                }
            }
        }
        break;
        }
    }
}
//
// 解析包
//
void parse_packet(void *pt)
{
    th_args *args = (th_args *)pt;
    // 如果剩余的数据超过4个字节就继续开始循环
    if (args->recv_len < 4)
    {
        log_error("parse_packet error, receive len is:%d", args->recv_len);
        free(args);
        close(args->socket_fd);
        return;
    }

    else
    {
        // SSP 包头
        union
        {
            unsigned char b[3]; // 小端模式 PSS
            unsigned int v;     // "PSS" -> 5264211
        } header;
        memcpy(&header, args->recv_buffer, 3);
        if (header.v == 5264211)
        {
            unsigned char packet_type = args->recv_buffer[3];
            union data_len
            {
                unsigned short value;
                unsigned char buffer[2];
            } data_len;
            data_len.buffer[1] = args->recv_buffer[4];
            data_len.buffer[0] = args->recv_buffer[5];
            // log_debug("receive data_len[uuid,data] is: %d", data_len.value);
            int next_parse_offset;
            switch (packet_type)
            {
            case PING:
            {
                next_parse_offset = 4;
                // reply
                unsigned char dist_buffer[] = {'S', 'S', 'P', PING_OK};
                log_debug("Client PING");
                send(args->socket_fd, dist_buffer, 4, 0);
                break;
            }
            case CONN:
                log_debug("Client COUNT:%ld", global_connection_id);
                //
                // 连接成功后加入全局MAP Key:socket_fd，Value: Client, 同时加入保活监控器
                //
                {
                    next_parse_offset = 6 + data_len.value;
                    unsigned char *uuid = (unsigned char *)malloc(32);
                    bzero(uuid, 32);
                    memcpy(uuid, &args->recv_buffer[6], 32);
                    unsigned char dist_buffer[] = {'S', 'S', 'P', CONN_ACK, 0};
                    log_debug("Client CONN: %s", uuid);
                    send(args->socket_fd, dist_buffer, 5, 0);
                    free(uuid);
                    break;
                }
            case DIS_CONN:
            {
                log_debug("Client DIS_CONN");
                close(args->socket_fd);
                break;
            }
            case SEND: // SSP|T|LL|DATA(N)
            {
                next_parse_offset = 6 + data_len.value;
                unsigned char *data = (unsigned char *)malloc(data_len.value);
                bzero(data, data_len.value);
                memcpy(data, &args->recv_buffer[6], data_len.value);
                unsigned char dist_buffer[] = {'S', 'S', 'P', SEND_ACK, 0};
                log_debug("Client SEND data: [%s]", data);
                send(args->socket_fd, dist_buffer, 5, 0);
                free(data);
                break;
            }
            case PUBLISH: // HEADER(4)| Len(2) | UUID(32byte) | DATA(N)
            {
                next_parse_offset = 6 + data_len.value;
                unsigned char *uuid = (unsigned char *)malloc(32);
                bzero(uuid, 32);
                memcpy(uuid, &args->recv_buffer[6], 32);
                unsigned char *data = (unsigned char *)malloc(data_len.value - 32 /*UUID*/);
                bzero(data, data_len.value - 32);
                memcpy(data, &args->recv_buffer[6 + 32], data_len.value - 31);
                //
                log_debug("Client PUBLISH data: [%s] to [%s]", data, uuid);
                unsigned char dist_buffer[] = {'S', 'S', 'P', PUBLISH_ACK, 0};
                send(args->socket_fd, dist_buffer, 5, 0);
                free(uuid);
                free(data);
                break;
            }
            default:
                log_error("Unknown packet type: %d, recv_buffer is: %s", packet_type, args->recv_buffer);
                return;
            }

            int remain_len = args->recv_len - next_parse_offset;
            if (remain_len == 0) // 无字节可读
            {
                free(args);
                return;
            }
            args->recv_len = remain_len;
            bzero(args->recv_buffer + next_parse_offset, RECV_BUFFER_LEN - next_parse_offset);
            memcpy(args->recv_buffer, &args->recv_buffer[next_parse_offset], args->recv_len);
            parse_packet((void *)args);
        }
        else
        {
            log_error("Unknown packet drop it");
            return;
        }
    }
}
/**
 *
 * */

void start_tcp_server(char *ip, int port)
{
    int listen_socket = init_tcp_socket(ip, port);
    int epoll_fd = init_epoll(listen_socket);
    start_epoll(epoll_fd, listen_socket);
}

/**
 *
 * */
pthread_mutex_t mutex;

connection *new_connection(int listen_socket)
{
    global_connection_id++;
    connection *c = (connection *)malloc(sizeof(connection));
    pthread_mutex_lock(&mutex);
    if (global_connection_id > (2 << 10))
    {
        log_error("max connection is 1024 but current is:%d", global_connection_id);
        return NULL;
    }
    c->connection_id = global_connection_id;
    c->listen_socket = listen_socket;
    pthread_mutex_unlock(&mutex);
    return c;
}

/**
 *
 * */
void add_new_connection(int new_socket)
{
    connections[global_connection_id] = new_connection(new_socket);
}

/**
 *
 * */
int epoll_add_fd(int epoll_fd, int fd, struct epoll_event e_event)
{
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e_event);
}
/**
 *
 * */
int epoll_mod_fd(int epoll_fd, int fd, struct epoll_event e_event)
{
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &e_event);
}
/**
 *
 * */
int epoll_del_fd(int epoll_fd, int fd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    connections[global_connection_id] = NULL;
    global_connection_id--;
    if (global_connection_id < 0)
    {
        global_connection_id = 0;
    }
    return 0;
}
//-------------------------------------------------------
// 编解码
//-------------------------------------------------------
void *encode_packet(Packet packet, unsigned char dist[])
{
    memcpy(dist, &packet, 3 + strlen(packet.data));
}
void *decode_packet(unsigned char source[], Packet packet)
{
}