#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

//Creates a new socket
int get_socket()
{
    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        perror("Unable to create socket.");
        exit(22);
    }
    return server_socket;
}

int bind_socket(int socket, int port)
{
    struct sockaddr_in sock_addr;

    sock_addr.sin_family = AF_INET;
}

int main(int argc, char* argv[])
{
    int port;
    int opt;

    //Parse arguments
    if (argc != 3)
    {
        perror("Not enough arguments.\n");
        exit (21);
    }
    while ((opt = getopt(argc, argv, "p:")) != -1)
    {
        switch (opt)
        {
        case 'p' :
            port = (int) strtol(optarg, (char **)NULL, 10);
            break;
        case '?' :
            perror("Something wrong with arguments.\n");
            return 21;
        default :
            perror("Something wrong with arguments.\n");
            return 21;
        }
    }

    //Create new socket
    int server_socket;
    server_socket = get_socket();
}
