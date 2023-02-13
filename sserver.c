#include "sserver.h"

/**
 *
 * */
void set_no_block(int fd)
{
    int fl = fcntl(fd, F_GETFL);
    if (fl < 0)
    {
        log_info("fcntl");
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
        log_info("Server started %s@[%s:%d]", format_type(type), ip, port);
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
                    log_info("Client_sockaddr_in[%s:%d] connected", inet_ntoa(client_sockaddr_in.sin_addr), ntohs(client_sockaddr_in.sin_port));
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
                        log_info("EPOLLRDHUP | EPOLLHUP [%s:%d]", inet_ntoa(client_sockaddr_in.sin_addr), ntohs(client_sockaddr_in.sin_port));
                        continue;
                    }
                    // EPOLLIN: 表示进来的消息
                    if (g_events[i].events & EPOLLIN)
                    {
                        th_args *args = (th_args *)malloc(sizeof(th_args));
                        args->socketfd = old_socket,
                        bzero(args->recv_buffer, sizeof(args->recv_buffer));
                        args->recv_len = recv(old_socket, args->recv_buffer, RECV_BUFFER_LEN, 0);
                        if (args->recv_len == -1)
                        {
                            break;
                        }
                        // 缓冲区溢出 直接丢包
                        if (args->recv_len > RECV_BUFFER_LEN)
                        {
                            bzero(&args->recv_buffer, sizeof(&args->recv_buffer));
                            log_error("&args->recv_buffer overflow, current packet size is: %d, but max buffer size is:%d", args->recv_len, RECV_BUFFER_LEN);
                            continue;
                        }
                        log_debug(">> BEGIN PKT------------------------------");
                        for (size_t i = 0; i < args->recv_len; i++)
                        {
                            log_debug("[%04X]", args->recv_buffer[i]);
                        }
                        log_debug(">> END PKT------------------------------");
                        // 线程池
                        thpool_add_work(thpool, parse_packet, (void *)args);
                        struct epoll_event e_event = {
                            .data.fd = old_socket,
                            .events = EPOLLIN | EPOLLET | EPOLLOUT,
                        };
                        epoll_mod_fd(epoll_fd, old_socket, e_event);
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
        free(args);
        log_error("parse_packet error:%d", args->recv_len);
        return;
    }

    else
    {
        // SSP 包头
        if ((args->recv_buffer[0] == 'S') &&
            (args->recv_buffer[1] == 'S') &&
            (args->recv_buffer[2] == 'P'))
        {
            unsigned char packet_type = args->recv_buffer[3];
            union data_len
            {
                unsigned short value;
                unsigned char buffer[2];
            } data_len;
            data_len.buffer[1] = args->recv_buffer[4];
            data_len.buffer[0] = args->recv_buffer[5];
            int offset;
            switch (packet_type)
            {
            case PING:
            {
                offset = 4;
                log_info("Client_sockaddr_in PING");
                // reply
                unsigned char dist_buffer[] = {'S', 'S', 'P', PING_OK};
                send(args->socketfd, dist_buffer, 4, 0);
                break;
            }
            case CONN:
                //
                // 连接成功后加入全局MAP Key:SocketFD，Value: Client, 同时加入保活监控器
                //
                {
                    offset = 6 + data_len.value;
                    unsigned char uuid[32];
                    bzero(uuid, 32);
                    memcpy(uuid, &args->recv_buffer[6], 31);
                    log_info("Client_sockaddr_in [%s] request CONN", uuid);
                    // reply
                    unsigned char dist_buffer[] = {'S', 'S', 'P', CONN_ACK, 0};
                    send(args->socketfd, dist_buffer, 5, 0);
                    break;
                }
            case DIS_CONN:
            {
                log_info("Client_sockaddr_in request DIS_CONN");
                close(args->socketfd);
                break;
            }
            case SEND: // SSP|T|LL|DATA(N)
            {
                offset = 6 + data_len.value;
                unsigned char *data = (unsigned char *)malloc(data_len.value);
                bzero(data, data_len.value);
                memcpy(data, &args->recv_buffer[6], data_len.value);
                log_info("Client request SEND: %s", data);
                free(data);
                // reply
                unsigned char dist_buffer[] = {'S', 'S', 'P', SEND_ACK, 0};
                send(args->socketfd, dist_buffer, 5, 0);
                break;
            }
            case PUBLISH: // HEADER(4)Len(2)|UUID(32byte)|DATA(N)
            {
                offset = 6 + data_len.value;
                unsigned char uuid[32];
                bzero(uuid, 32);
                memcpy(uuid, &args->recv_buffer[6], 31);
                unsigned char *data = (unsigned char *)malloc(data_len.value - 32);
                bzero(data, data_len.value - 32);
                memcpy(data, &args->recv_buffer[6 + 32], data_len.value - 32);
                log_info("Client request PUBLISH data: [%s] to: %s", data, uuid);
                free(data);
                // reply
                unsigned char dist_buffer[] = {'S', 'S', 'P', PUBLISH_ACK, 0};
                send(args->socketfd, dist_buffer, 5, 0);
                break;
            }
            default:
                log_error("Unknown packet type: %d, recv_buffer is: %s", packet_type, args->recv_buffer);
                return;
            }

            int remain_len = args->recv_len - offset;
            if (remain_len == 0) // 无字节可读
            {
                free(args);
                return;
            }
            args->recv_len = remain_len;
            memcpy(args->recv_buffer, &args->recv_buffer[offset], args->recv_len);
            parse_packet((void *)args);
        }
        else
        {
            log_error("Unknown packet data: %s, drop it", args->recv_buffer);
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
connection *new_connection(int listen_socket)
{
    connection *c = (connection *)malloc(sizeof(connection));
    global_connection_id++;
    if (global_connection_id > (2 << 10))
    {
        log_error("max connection is 1024 but current is:%d", global_connection_id);
        return NULL;
    }
    c->connection_id = global_connection_id;
    c->listen_socket = listen_socket;
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