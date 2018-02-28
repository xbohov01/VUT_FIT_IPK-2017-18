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
#include <errno.h>

//Creates a new socket
int get_socket()
{
    int client_socket;
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        perror("Unable to create socket.");
        exit(22);
    }
    return client_socket;
}

//Converts given host's name to IP address
void hostname_to_addr(char* hostname, char* address)
{
    struct hostent *host_info;
    struct in_addr **address_list;

    host_info = gethostbyname(hostname);
    if (host_info == NULL)
    {
        perror("Unable to resolve host name.\n");
        exit(23);
    }
    else
    {
        address_list = (struct in_addr **) host_info->h_addr_list;
        strcpy(address, inet_ntoa(*address_list[0]));
        return;
    }
}

void connect_socket(int socket, char* hostname, int port)
{
    struct sockaddr_in server_address;
    //Might cause trouble if IPv6 is used
    char host_addr[15];

    //Resolve host name
    hostname_to_addr(hostname, host_addr);

    fprintf(stderr, "Resolved host name %s to IP %s connecting now at port %d.\n", hostname, host_addr, port);

    //This section of code is inspired by:
    //https://stackoverflow.com/questions/27014955/socket-connect-vs-bind
    //Credit to StackOverflow user Khan Nov 19th 2014
    //Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(host_addr);
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    if (connect(socket, (struct sockaddr*) &server_address, sizeof(server_address)) != 0)
    {
        perror("Unable to connect socket.");
        //fprintf(stderr, "Unable to connect socket %d.\n", errno);
        exit(22);
    }
    return;
}

int send_msg(int dest_socket)
{
    int sent_chars = 0;
    char *outbound_buffer = "Test message";

    //TODO dynamic allocation based on message type
    //Send message to server
    sent_chars = send(dest_socket, outbound_buffer, strlen(outbound_buffer), 0);
    if (sent_chars < 0)
    {
        perror("Unable to send message.\n");
        exit(25);
    }
}

int main (int argc, char* argv[])
{
    char *host;
    unsigned int port;
    char *login;
    //Parse arguments
    int opt;
    bool option_used = false;
    bool full_name = false;
    bool home_folder = false;
    bool user_list = false;
    if (argc < 5)
    {
        perror("Not enough arguments.\n");
        return 21;
    }
    while ((opt = getopt(argc, argv, "h:p:n:f:l")) != -1)
    {
        switch (opt)
        {
        case 'h' :
            host = optarg;
            break;
        case 'p' :
            port = (int) strtol(optarg, (char **)NULL, 10);
            break;
        case 'n' :
            if (option_used == false)
            {
                login = optarg;
                full_name = true;
                option_used = true;
            }
            else
            {
                perror("Something wrong with arguments.\n");
                return 21;
            }
            break;
        case 'f' :
            if (option_used == false)
            {
                login = optarg;
                home_folder = true;
                option_used = true;
            }
            else
            {
                perror("Something wrong with arguments.\n");
                return 21;
            }
            break;
        case 'l' :
            if (option_used == false)
            {
                login = argv[optind];
                home_folder = true;
                option_used = true;
            }
            else
            {
                perror("Something wrong with arguments.\n");
                return 21;
            }
            break;
        case '?' :
            perror("Something wrong with arguments.\n");
            return 21;
        default :
            perror("Something wrong with arguments.\n");
            return 21;
        }
    }

    //Create socket
    int client_socket;
    client_socket = get_socket();
    //Connect socket
    connect_socket(client_socket, host, port);

    close(client_socket);

    printf("%s %d %s.\n",host, port, login);

    return 0;
}

