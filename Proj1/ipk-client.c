/*
ipk-client.c
Samuel Bohovic
xbohov01

Copyright notice:
Various parts of code are inspired by DEMO source codes provided.
Linux man pages were used for getting information of functions used.
*/

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
#include <time.h>
#include <fcntl.h>

typedef enum
{
    name,
    folder,
    list
} protocol_opt;

#define DATA_LEN 256

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

    //Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(host_addr);
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    if (connect(socket, (struct sockaddr*) &server_address, sizeof(server_address)) != 0)
    {
        perror("Unable to connect socket.");
        exit(22);
    }
    return;
}

int send_msg(int dest_socket, char* msg_content)
{
    int sent_chars = 0;

    printf("DEBUG msg_content >>> %s\n", msg_content);

    //Send message to server
    sent_chars = send(dest_socket, msg_content, strlen(msg_content), 0);
    if (sent_chars < 0)
    {
        perror("Unable to send message.\n");
        exit(25);
    }

}

//Message reception interface
    //Socket that will receive data
    //Buffer for data
    //Size of data
    //Is a number expected
int rec_msg(int src_socket, char *incoming_buffer, int buff_size)
{
    int msg_len = 0;
    int act_buff_size = buff_size;

    char tmp_buffer[200];

    fd_set set;
    FD_ZERO(&set);
    FD_SET(src_socket, &set);

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(src_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    while(42)
    {
        memset(tmp_buffer, '\0', 200);
        msg_len = recv(src_socket, tmp_buffer, 200, 0);

        if (msg_len < 0 /*&& errno != EWOULDBLOCK && errno != EAGAIN*/)
        {
            perror("recv ");
            exit(25);
        }
        if (msg_len == 0)
        {
            return msg_len;
        }
        if (strlen(incoming_buffer) + strlen(tmp_buffer) > act_buff_size)
        {
            act_buff_size += DATA_LEN;
            incoming_buffer = realloc(incoming_buffer, act_buff_size);
            strcat(incoming_buffer, tmp_buffer);
        }
        else
        {
            strcat(incoming_buffer, tmp_buffer);
        }
    }

    //fprintf(stderr, "DEBUG incoming_buffer >> %s\n", incoming_buffer);
    return msg_len;
}

///TODO actual hash???
//Hash function for authorization
unsigned int hash(unsigned int in)
{
    return in;
}

//Represents the protocol's FSM
int my_client_protocol(int client_socket, char* login, char *host, int port, protocol_opt option)
{
    char* msg_buffer;
    int msg_len;

    ///Authorization part
    /******************************************************/
    //Authorization message contains a number and it's hash
    srand(time(NULL));
    unsigned int hash_in = rand();
    unsigned int hash_out = hash(hash_in);

    //convert to string
    int hash_length = snprintf( NULL, 0, "%d", hash_in);
    char *hash_in_chr = malloc(hash_length + 1);
    snprintf( hash_in_chr, hash_length + 1, "%d", hash_in);

    hash_length = snprintf( NULL, 0, "%d", hash_out);
    char *hash_out_chr = malloc(hash_length +1);
    snprintf( hash_out_chr, hash_length + 1, "%d", hash_out);

    fprintf(stderr, "Requesting authorization with server with hash %s\n", hash_out_chr);

    //Compose auth message
    if (login != NULL)
    {
        msg_len = (strlen(hash_in_chr) + strlen(hash_out_chr) + strlen(login));
    }
    else
    {
        msg_len = (strlen(hash_in_chr) + strlen(hash_out_chr));
    }
    msg_buffer = malloc((msg_len+7)*sizeof(char));
    memset(msg_buffer, '\0', sizeof(char)*(msg_len+7));
    memcpy(msg_buffer, hash_in_chr, strlen(hash_in_chr));
    strcat(msg_buffer, "$");
    strcat(msg_buffer, hash_out_chr);
    strcat(msg_buffer, "$");
    free(hash_in_chr);
    free(hash_out_chr);

    ///Data request part
    /****************************************************/

    if (login != NULL)
    {
        strcat(msg_buffer, login);
    }
    strcat(msg_buffer, "$");

    switch(option)
    {
        case folder :
            strcat(msg_buffer, "F");
            break;
        case name :
            strcat(msg_buffer, "N");
            break;
        case list :
            strcat(msg_buffer, "L");
            break;
    }

    //End of message delimiter
    strcat(msg_buffer, "&");

    send_msg(client_socket, msg_buffer);
    shutdown(client_socket, SHUT_WR);

    ///Receive data
    /***************************************************/
    //Data size
    msg_buffer = realloc(msg_buffer, DATA_LEN*sizeof(char));
    memset(msg_buffer, '\0', 10*sizeof(char));

    fprintf(stderr, "Ready for data\n");

    //Data
    rec_msg(client_socket, msg_buffer, (DATA_LEN+1*sizeof(char)));

    if (option != list)
    {
        printf("%s\n", msg_buffer);
    }
    else
    {
        printf("%s", msg_buffer);
    }

    //fprintf(stderr, ">>>>>%lu %lu\n", strlen(msg_buffer), data_len*sizeof(char));

    close(client_socket);
    free(msg_buffer);
}

int main (int argc, char* argv[])
{
    char *host;
    unsigned int port;
    char *login;
    //Parse arguments
    int opt;
    bool option_used = false;
    protocol_opt option;
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
                option = name;
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
                option = folder;
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
                option = list;
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
    fprintf(stderr, "Connection to server started -- starting protocol.\n");

    //Connection is OK, start protocol
    my_client_protocol(client_socket, login, host, port, option);

    close(client_socket);

    return 0;
}

