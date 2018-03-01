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

    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    return server_socket;
}

struct sockaddr_in bind_socket(int socket, int port)
{
    struct sockaddr_in src_sock_addr;
    int return_val;

    bzero((char *) &src_sock_addr, sizeof(src_sock_addr));
    src_sock_addr.sin_family = AF_INET;
    src_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    src_sock_addr.sin_port = htons(port);

    return_val = bind(socket, (struct sockaddr*) &src_sock_addr, sizeof(src_sock_addr));

    if (return_val < 0)
    {
        perror("Unable to bind socket.\n");
        exit(22);
    }
    return src_sock_addr;
}

void listen_on_socket(int listen_socket)
{
    if (listen(listen_socket, 1) != 0)
    {
        perror("Something went wrong during listen.\n");
        exit(23);
    }
    fprintf(stderr, "Now listening on socket.\n");
    return;
}

void receive_message(int rec_socket)
{
    char buffer[1024];
    int msg_len = 0;
    memset(buffer, '\0', sizeof(buffer));
    struct sockaddr_in client_address;
    int client_len = sizeof(client_address);

    for (;;)
    {
        msg_len = recv(rec_socket, buffer, 1024, 0);
        if (msg_len < 0)
        {
            perror("recv ");
            exit(25);
        }
        else if (msg_len == 0)
        {
            break;
        }
    }
    fprintf(stderr, "Message (%lu) received: %s.\n", strlen(buffer), buffer);

    return;
}

int accept_connection(int target_socket, struct sockaddr_in addr)
{
    int i = 0;
    int comm_socket;
    socklen_t addr_len = sizeof(addr);

    fprintf(stderr, "Waiting for message.\n");

    while (42)
    {
        comm_socket = accept(target_socket, (struct sockaddr*)&addr, &addr_len);
        //Connection found
        if (comm_socket > 0)
        {
            receive_message(comm_socket);
            fprintf(stderr, "Message done.\n");
        }
    }
}

int main(int argc, char* argv[])
{
    unsigned int port;
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
    //Bind socket
    struct sockaddr_in sock_address;
    sock_address = bind_socket(server_socket, port);

    listen_on_socket(server_socket);

    accept_connection(server_socket, sock_address);
}
