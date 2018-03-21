#include <cstdlib>
#include <iostream>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

using namespace std;

void exit_handler(int code)
{
    //TODO delete everything
    exit(code);
}

int reflector(int port)
{
    //Get socket
    int reflectSocket;
    reflectSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (reflectSocket <= 0)
    {
        perror("Unable to create socket.");
        exit(1);
    }

    int optval = 1;
    setsockopt(reflectSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    //Bind socket
    struct sockaddr_in reflectAddr;

    bzero((char *) &reflectAddr, sizeof(reflectAddr));
    reflectAddr.sin_family = AF_INET;
    reflectAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    reflectAddr.sin_port = htons(port);

    if (bind(reflectSocket, (struct sockaddr*) &reflectAddr, sizeof(reflectAddr)) < 0)
    {
        perror("Unable to bind socket.\n");
        exit(22);
    }

    struct sockaddr_in meterAddr;
    socklen_t meterAddrLen = sizeof(meterAddr);
    struct hostent *meterHost;
    char *meterIp;
    int bytesReceived, bytesSent;

    char buffer[1024];

    //Receive and reflect
    while (42)
    {
        fprintf(stderr, "Waiting to reflect\n");
        //Receive
        bytesReceived = recvfrom(reflectSocket, buffer, 1024, 0, (struct sockaddr *) &meterAddr, &meterAddrLen);
        if (bytesReceived < 0) 
            perror("ERROR: recvfrom:");

        //Resolve sender
        meterHost = gethostbyaddr((const char *)&meterAddr.sin_addr.s_addr, sizeof(meterAddr.sin_addr.s_addr), AF_INET);
        meterIp = inet_ntoa(meterAddr.sin_addr);
        fprintf(stderr, "Probe from %s\n", meterIp);
    }

    return 0;
}


int meter(char* host, int port, int probeSize, int testTimeout)
{
    //Get UDP socket
    int meterSocket;
    meterSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (meterSocket <= 0)
    {
        perror("Unable to create socket.");
        exit(1);
    }

    //Resolve address
    struct hostent *host_info;
    struct in_addr **address_list;
    char address[15];

    host_info = gethostbyname(host);
    if (host_info == NULL)
    {
        perror("Unable to resolve host name.\n");
        exit(23);
    }
    else
    {
        address_list = (struct in_addr **) host_info->h_addr_list;
        strcpy(address, inet_ntoa(*address_list[0]));
        fprintf(stderr, "Host %s resolved as %s\n", host, address);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(address);
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    //Generate probe
    fprintf(stderr, "Generating probe of size %d\n", probeSize);
    string sendBuffer;

    //Send
    //TODO Timer
    int bytesSent, bytesReceived;

    fprintf(stderr, "Sending probe to reflector at %s\n", address);
    bytesSent = sendto(meterSocket, sendBuffer.c_str(), sendBuffer.length(), 0, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bytesSent < 0)
    {
        fprintf(stderr, "%d ", errno);
        perror("sendto()");
        exit(1);
    }

    //Receive reflection
    char *recBuffer;
    recBuffer = (char*) malloc(sizeof(char)*probeSize+1);

    //Check and process data

    //CLEANUP
    close(meterSocket);
    free(recBuffer);

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        cerr << "Not enough arguments\n";  
        return 1;   
    }

    int rv = 0;
    int c;
    char *host;
    string portString;
    string sizeString;
    string timeoutString;

    //Branch argument check for meter and reflector
    if (strcmp(argv[1], "meter") == 0)
    {
        //Meter getopt
        while((c = getopt(argc, argv, "h:p:s:t:")) != -1)
        {
            switch (c)
            {
                case 'h':
                    host = optarg;
                    break;
                
                case 'p':
                    portString = optarg;
                    break;

                case 's':
                    sizeString = optarg;
                    break;

                case 't':
                    timeoutString = optarg;
                    break;

                case '?':
                    perror("Something wrong with arguments");
                    exit(1);
                
                default:
                    perror("Something wrong with arguments");
                    exit(1);
            }
        }

        int port = stoi(portString);
        int size = stoi(sizeString);
        int timeout = stoi(timeoutString);

        rv = meter(host, port, size, timeout);
    }
    else if (strcmp(argv[1], "reflect") == 0)
    {
        //Reflector getopt
        while((c = getopt(argc, argv, "p:")) != -1)
        {
            switch (c)
            {
                case 'p':
                    portString = optarg;
                    break;

                case '?':
                    perror("Something wrong with arguments");
                    exit(1);
                
                default:
                    perror("Something wrong with arguments");
                    exit(1);
            }
        }

        int port = stoi(portString);

        rv = reflector(port);
    }
    else
    {
        cerr << "Type of execution not specified\nDo you want to run meter or reflect?\n";
        exit(1);
    }

    return rv;
}