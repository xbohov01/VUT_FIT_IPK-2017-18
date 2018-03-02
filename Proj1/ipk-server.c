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
#include <fcntl.h>

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

//Gets next message in buffer
//Returns length of message
int get_message(char *buffer, char* message)
{
    char *c = malloc(sizeof(char));
    if (strlen(buffer) == 0)
    {
        free(c);
        return 0;
    }
    bzero(message, sizeof(message));
    for (int i = 0; i < strlen(buffer); i++)
    {
        c[0] = buffer[i];
        if (c[0] == '&')
        {
            strcat(message, "\0");
            //shift buffer
            memcpy(buffer, &buffer[i+1], (strlen(buffer)-i));
            free(c);
            if (strchr(buffer, '&') == NULL)
            {
                memset(buffer, '\0', sizeof(buffer));
            }
            return i;
        }
        else
        {
            printf("%s\n", c);
            strcat(message, c);
        }
    }
}

void receive_message(int rec_socket, char* incoming_message)
{
    static char *buffer;
    char *message;
    message = malloc(128);
    if (buffer == NULL)
    {
        buffer = malloc(1024);
        memset(buffer, '\0', sizeof(buffer));
    }



    if (get_message(buffer, message) > 0)
    {
        fprintf(stderr, "Message (%lu) received: %s.\n", strlen(message), message);
        return;
    }

    memset(buffer, '\0', sizeof(buffer));
    int msg_len = 0;

    //No messages left in buffer, listen for new message
    for (;;)
    {
        msg_len = recv(rec_socket, incoming_message, 1024, 0);
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
    strcat(message, "\0");
    strcat(buffer, message);
    //get_message(buffer, message);
    fprintf(stderr, "Message (%lu) received: %s.\n", strlen(message), incoming_message);

    free(message);
    return;
}

int send_message(int send_socket, char* outgoing_buffer)
{
    int sent_chars = 0;

    printf("DEBUG outgoing_buffer >>> %s\n", outgoing_buffer);

    //Send message to server
    sent_chars = send(send_socket, outgoing_buffer, strlen(outgoing_buffer), MSG_EOR);
    if (sent_chars < 0)
    {
        perror("Unable to send message.\n");
        exit(25);
    }
}

///TODO actual hash???
//Hash function for authorization
unsigned int hash(unsigned int in)
{
    return in;
}

int my_server_protocol(int server_socket)
{
    char *msg_buffer;
    msg_buffer = malloc(1024);
    memset(msg_buffer, '\0', sizeof(msg_buffer));

    ///Check hash
    /*************************************************************/
    fprintf(stderr, "Reading hash from client.\n");
    receive_message(server_socket, msg_buffer);

    unsigned int number;
    unsigned int hash_in;
    int divider;
    char number_chr[20];
    char hash_chr[20];

    ///TODO remake to dynamic
    //Split message
    for (int i = 0; i < strlen(msg_buffer); i++)
    {
        if (msg_buffer[i] == '$')
        {
            divider = i+1;
            number_chr[i] = '\0';
            break;
        }
        else
        {
            number_chr[i] = msg_buffer[i];
        }
    }
    int j = 0;
    memset(hash_chr, '\0', sizeof(hash_chr));
    for (int i = divider; i < strlen(msg_buffer); i++)
    {
        if (msg_buffer[i] == '\0')
        {
            hash_chr[j] = '\0';
            break;
        }
        hash_chr[j] = msg_buffer[i];
        j++;
    }

    hash_in = atoi(hash_chr);
    number = atoi(number_chr);

    fprintf(stderr, "Hash read -- checking now.\n");

    unsigned int hash_ref;
    hash_ref = hash(number);
    if (hash_ref == hash_in)
    {
        fprintf(stderr, "Hash checks out -- ready for request.\n");
    }
    else
    {
        fprintf(stderr, "Hash failure -- closing connection.\n");
        close(server_socket);
    }

    ///Ask for data request
    /*************************************************************/
    fprintf(stderr, "Asking client for data request.\n");
    memset(msg_buffer, '\0', sizeof(msg_buffer));

    strcat(msg_buffer, "ok&");

    send_message(server_socket, msg_buffer);

    fprintf(stderr, "Waiting for data request.\n");

    receive_message(server_socket, msg_buffer);

    ///Get data
    /*************************************************************/

    //fprintf(stderr, "DEBUG msg_buffer >>>>> %s\n", msg_buffer);
    return 0;
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
        //Client has connected
        if (comm_socket > 0)
        {
            fprintf(stderr, "Incoming connection -- starting protocol now.\n");
            my_server_protocol(comm_socket);
            fprintf(stderr, "Protocol done -- ready for next connection.\n");
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

    //accept_connection(server_socket, sock_address);
    my_server_protocol(server_socket, port, sock_address);
}
