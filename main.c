#include "eepoll.h"
#define IP "0.0.0.0"
#define PORT1 2889
#define PORT2 2890
int main(int argc, char *argv[])
{
    start_tcp_server(IP, PORT1);
    return 0;
}