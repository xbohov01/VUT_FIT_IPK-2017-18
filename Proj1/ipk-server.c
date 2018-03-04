/*
ipk-server.c
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
#include <pwd.h>

typedef enum
{
    name,
    folder,
    list
} protocol_opt;

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

//Binds socket
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

//Designates socket to listen
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

//Handles reception of messages
//Ensures no fragments of previous messages stay in buffer
void receive_message(int rec_socket, char* incoming_message)
{
    int msg_len = 0;

    //Erase message buffer
    memset(incoming_message, '\0', strlen(incoming_message));

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

    //Truncate buffer if something was left from last message
    bool end = false;
    for (int i = 0; i < strlen(incoming_message); i++)
    {
        if (incoming_message[i] == '&')
        {
            end = true;
            i++;
        }
        if (end == true)
        {
            incoming_message[i] = '\0';
        }
    }

    //fprintf(stderr, "Message (%lu) received: %s.\n", strlen(incoming_message), incoming_message);

    return;
}

//Sends message from server
int send_message(int send_socket, char* outgoing_buffer)
{
    int sent_chars = 0;

    //fprintf(stderr,"DEBUG outgoing_buffer >>> %s\n", outgoing_buffer);

    //Send message to server
    sent_chars = send(send_socket, outgoing_buffer, strlen(outgoing_buffer), 0);
    if (sent_chars < 0)
    {
        perror("Unable to send message.\n");
        exit(25);
    }
}

//Listens for incoming connections
//Incoming connection is given a active socket to use
int accept_connection(int target_socket, struct sockaddr_in addr)
{
    int i = 0;
    int comm_socket;
    socklen_t addr_len = sizeof(addr);

    fprintf(stderr, "Waiting for new connection.\n");

    while (42)
    {
        comm_socket = accept(target_socket, (struct sockaddr*)&addr, &addr_len);
        //Client has connected
        if (comm_socket > 0)
        {
            return comm_socket;
        }
    }
}

//Checks if string has given prefix
//Apparently C doesn't have a function for this
bool strpref(char *haystack, char *prefix)
{
    size_t pre_len = strlen(prefix);
    if (strncmp(haystack, prefix, pre_len) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }

}

///TODO actual hash???
//Hash function for authorization
unsigned int hash(unsigned int in)
{
    return in;
}

///Server-side protocol
int my_server_protocol(int server_socket)
{
    char *msg_buffer;
    int buffer_len = 1024;
    msg_buffer = malloc(buffer_len*sizeof(char));
    memset(msg_buffer, '\0', sizeof(msg_buffer));

    ///Check hash
    /*************************************************************/
    fprintf(stderr, "Checking hash from client.\n");
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

    ///Reading data request
    /*************************************************************/
    fprintf(stderr, "Reading data request.\n");

    //Message format > (0-9)*$(0-9)*$[xlogin00|name]$[N|F|L]
    bool name = false;
    bool folder = false;
    bool list = false;
    char *name_login = malloc(50*sizeof(char));
    memset(name_login, '\0', 50*sizeof(char));
    int delims_found = 0;
    int k = 0;

    for (int i = 0; i < strlen(msg_buffer); i++)
    {
        if (msg_buffer[i] == '$')
        {
            delims_found++;
            continue;
        }

        if (delims_found < 2)
        {
            continue;
        }

        if (delims_found == 2)
        {
            if (msg_buffer[i] == '@')
            {
                continue;
            }
            name_login[k] = msg_buffer[i];
            name_login[k+1] = '\0';
            k++;
        }
        if (delims_found == 3)
        {
            if (msg_buffer[i] == 'N')
            {
                name = true;
            }
            else if (msg_buffer[i] == 'F')
            {
                folder = true;
            }
            else if (msg_buffer[i] == 'L')
            {
                list = true;
            }
        }

    }

    fprintf(stderr, "Data request read -- getting data.\n");

    ///Get data
    /*************************************************************/
    int size = 200;
    int line_len = 0;
    char *line = malloc(size*sizeof(char));
    memset(line, '\0', size*sizeof(char));
    memset(msg_buffer, '\0', buffer_len);

    struct passwd *passwd_file = NULL;

    //Get data
    if (name == true)
    {
        passwd_file = getpwnam(name_login);
        if (passwd_file == NULL)
        {
            printf ("Can't open passwd file.\n");
            exit (28);
        }
        fprintf(stderr, "Getting data for user: %s\n", name_login);

        memcpy(line, passwd_file->pw_gecos, strlen(passwd_file->pw_gecos));
        strcat(line, "\0");

        strcat(msg_buffer, line);

        fprintf(stderr, "Data ready.\n");
    }
    else if (folder == true)
    {
        passwd_file = getpwnam(name_login);
        if (passwd_file == NULL)
        {
            printf ("Can't open passwd file.\n");
            exit (28);
        }
        fprintf(stderr, "Getting home folder for user: %s\n", name_login);

        memcpy(line, passwd_file->pw_dir, strlen(passwd_file->pw_dir));
        strcat(line, "\0");

        strcat(msg_buffer, line);

        fprintf(stderr, "Data ready.\n");
    }
    else if (list == true)
    {
        //Opens stream to read list of users
        setpwent();
        //Get entries
        //Returns NULL when there are no more entries
        while ((passwd_file = getpwent()) != NULL)
        {
            //Login filter
            if (strpref(passwd_file->pw_name, name_login) == false)
            {
                continue;
            }
            //Get username
            if (strlen(msg_buffer) == 0)
            {
                //Buffer is blank so first entry is copied
                memcpy(msg_buffer, passwd_file->pw_name, strlen(passwd_file->pw_name));
                strcat(msg_buffer, "\n");
            }
            else
            {
                //Other entries can be concatenated
                //Have to check if buffer has enough space
                if ((strlen(msg_buffer)+strlen(passwd_file->pw_name)) < (buffer_len*sizeof(char)))
                {
                    strcat(msg_buffer, passwd_file->pw_name);
                }
                else
                {
                    buffer_len += 1000;
                    msg_buffer = realloc(msg_buffer, buffer_len*sizeof(char));
                    strcat(msg_buffer, passwd_file->pw_name);
                }
                strcat(msg_buffer, "\n");
            }

        }

        strcat(msg_buffer, "\0");
        //Close the stream
        endpwent();
    }

    ///Prepare data for sending
    /**************************************************************/
    //Prepare message with data length
    int data_len;
    data_len = strlen(msg_buffer);
    char* data_len_str = malloc(10 * sizeof(char));
    memset(data_len_str, '\0', 10 * sizeof(char));
    sprintf(data_len_str, "%d", data_len);

    fprintf(stderr, "Data length = %d\n", data_len);

    //Send data lenght
    send_message(server_socket, data_len_str);

    fprintf(stderr, "Data size message sent -- sending data.\n");

    //Give client time to get ready
    //If messages get sent too close together TCP may merge them
    sleep(1);

    //Send data
    send_message(server_socket, msg_buffer);

    fprintf(stderr, "Data sent.\n");

    //Free resources
    free(data_len_str);
    free(name_login);
    free(line);
    free(msg_buffer);

    close(server_socket);
    return 0;
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

    while(42)
    {
        int act_socket = accept_connection(server_socket, sock_address);
        //Create a child to handle new connection
        int pid = fork();
        if (pid == 0)
        {
            int child_pid = getpid();
            close(server_socket);
            fprintf(stderr, "Starting protocol now -- handled by: %d.\n", child_pid);
            my_server_protocol(act_socket);
            fprintf(stderr, "Protocol done -- handled by: %d.\n", child_pid);
            exit(0);
        }
        else
        {
            close(act_socket);
        }
    }


}
