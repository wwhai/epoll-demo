#include "sserver.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    int arg;
    opterr = 0;
    char ip[15];
    int port;
    while ((arg = getopt(argc, argv, "h:p:")) != -1)
    {
        switch (arg)
        {
        case 'h':
            strcpy(ip, optarg);
            break;
        case 'p':
            *(&port) = atoi(optarg);
            break;
        default:
            log_error("Unknown option: '%c', Usage is: sserver -h ${Host} -p ${Port}. \n", (char)optopt);
            exit(1);
        }
    }
    if (strlen(ip) < 7 || port < 1)
    {
        log_error("Start server error, Usage is: sserver -h ${Host} -p ${Port}. \n");
        exit(1);
    }

    start_tcp_server(ip, port);
    return 0;
}