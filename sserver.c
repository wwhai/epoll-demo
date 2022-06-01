#include "sserver.h"
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')
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
        log_info("%s server started at [%s:%d]", format_type(type), ip, port);
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
 * init_udp_socket
 * */
int init_udp_socket(char *ip, int port)
{
    return init_socket(ip, port, SOCK_DGRAM);
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
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    while (1)
    {
        int ready_fd_count = epoll_wait(epoll_fd, e_events, MAX_WAIT_FD_NUM, TIMEOUT);
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
                if (e_events[i].data.fd == listen_socket && e_events[i].events & EPOLLIN)
                {
                    //
                    // 新连接加入
                    //
                    int new_socket = accept(listen_socket, (struct sockaddr *)&client, &len);
                    if (new_socket < 0)
                    {
                        continue;
                    }
                    set_no_block(new_socket);

                    struct epoll_event e_event;
                    e_event.events = EPOLLIN | EPOLLET;
                    e_event.data.fd = new_socket;
                    if (epoll_add_fd(epoll_fd, new_socket, e_event) < 0)
                    {
                        continue;
                    }
                    add_new_connection(new_socket);
                    log_info("client[%s:%d] connect", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                }
                else
                {
                    //
                    // 老连接发消息
                    //
                    int old_socket = e_events[i].data.fd;
                    if (old_socket < 0)
                    {
                        continue;
                    }

                    if (e_events[i].events & EPOLLIN)
                    {
                        bzero(recv_buffer, sizeof(recv_buffer));
                        ssize_t len = recv(old_socket, recv_buffer, RECV_BUFFER_LEN, 0);
                        if (len < 1)
                        {
                            epoll_del_fd(epoll_fd, old_socket);
                            log_info("client[%s:%d] closed", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                            continue;
                        }
                        printf("%s\n", recv_buffer);
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, old_socket, &e_event);
                    }
                    if (e_events[i].events & EPOLLOUT)
                    {

                        // sendto(sock, buffer, Len, 0, &client, nSize);
                        log_info("EPOLLOUT <<");
                    }
                }
            }
        }
        break;
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
void start_udp_server(char *ip, int port)
{
    log_debug("wait monment");
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
