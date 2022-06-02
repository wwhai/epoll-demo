#ifndef __EEPOLL_H__

#include <sys/epoll.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "log.h"
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
//
// 最大等文件描述符
//
#define MAX_WAIT_FD_NUM 64
//
// 最大可接受连接
//
#define MAX_ACCEPT_FD_NUM 2000000
//
// 全局数据缓冲区(字节)
//
#define RECV_BUFFER_LEN 1024
//
// 默认超时时间
//
#define TIMEOUT 5000
//
// 全局连接层ID
//
unsigned int global_connection_id;
//
// 最大监控事件数
//
struct epoll_event e_events[MAX_WAIT_FD_NUM];

//
// 数据缓冲区
//
unsigned char recv_buffer[RECV_BUFFER_LEN];

//
// 连接层抽象
//
typedef struct
{
    unsigned int connection_id;
    int listen_socket;
} connection;
typedef struct
{

} channel;
typedef struct
{

} client;

/**
 *假设最大只能连接1024个客户端
 * */
connection *connections[2 << 10];
/**
 *
 * */
connection *new_connection(int listen_socket);
/**
 *
 * */
void add_new_connection(int new_socket);
/**
 *
 * */
void set_no_block(int fd);
/**
 *
 * */
int init_tcp_socket(char *ip, int port);
/**
 *
 * */
int init_epoll(int listen_socket);
/**
 *
 * */
void start_epoll(int epoll_fd, int listen_socket);
/**
 *
 * */
int init_tcp_socket(char *ip, int port);
int init_udp_socket(char *ip, int port);
void start_tcp_server(char *ip, int port);
void start_udp_server(char *ip, int port);
/**
 *
 * */
int epoll_add_fd(int epoll_fd, int fd, struct epoll_event e_event);
/**
 *
 * */
int epoll_mod_fd(int epoll_fd, int fd, struct epoll_event e_event);
/**
 *
 * */
int epoll_del_fd(int epoll_fd, int fd);

#define __EEPOLL_H__
#endif
