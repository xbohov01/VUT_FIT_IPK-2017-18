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
//Time measurement
#include <chrono>

using namespace std;
using namespace std::chrono;

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
    int bufferSize = 32768;

    char buffer[bufferSize];

    //Receive and reflect
    while (42)
    {
        fprintf(stderr, "Waiting to reflect\n");
        //Receive
        bytesReceived = recvfrom(reflectSocket, buffer, bufferSize, 0, (struct sockaddr *) &meterAddr, &meterAddrLen);
        if (bytesReceived < 0) 
            perror("recvfrom()");

        //Resolve sender
        meterHost = gethostbyaddr((const char *)&meterAddr.sin_addr.s_addr, sizeof(meterAddr.sin_addr.s_addr), AF_INET);
        meterIp = inet_ntoa(meterAddr.sin_addr);
        fprintf(stderr, "Probe from %s (%lu)\n", meterIp, strlen(buffer));

        //Reflect message
        bytesSent = sendto(reflectSocket, buffer, bufferSize, 0, (struct sockaddr*)&meterAddr, meterAddrLen);
        if (bytesSent < 0)
            perror("sendto()");
    }

    return 0;
}

string generateProbe(int size)
{
    string probe = "";
    char letters[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    probe.append(1, '<');
    for (int i = 0; i < size-2; i++)
    {
        probe.append(1, letters[i%26]);
        //printf("%s >>%d<<\n", probe.c_str(), i);
    }
    probe.append(1, '>');
    probe.append(1, '\0');

    return probe;
}

int meter(char* host, int port, int probeSize, int testTimeout)
{
    //Time
    high_resolution_clock::time_point startTime;
    high_resolution_clock::time_point endTime;

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

    struct sockaddr_in reflectorAddress;
    reflectorAddress.sin_family = AF_INET;
    reflectorAddress.sin_port = htons(port);
    reflectorAddress.sin_addr.s_addr = inet_addr(address);
    memset(reflectorAddress.sin_zero, '\0', sizeof(reflectorAddress.sin_zero));

    //Generate probe
    fprintf(stderr, "Generating probe of size %d\n", probeSize);
    string sendBuffer = generateProbe(probeSize);
    //fprintf(stderr, "Probe: %s\n", sendBuffer.c_str());

    //Buffer
    char *recBuffer;
    recBuffer = (char*) malloc(sizeof(char)*probeSize+1);
    memset(recBuffer, '\0', probeSize+1);

    //Send
    //TODO Timer
    int bytesSent, bytesReceived;
    socklen_t reflectAddrLen = sizeof(reflectorAddress);

    //CHANGE
    int numberOfTests = 10;

    double rttTimes[numberOfTests];

    //Tests
    for (int test = 1; test <= numberOfTests; test++)
    {
        fprintf(stderr, "Test %d start\n", test);

        startTime = high_resolution_clock::now();

        //Send probes
        for (int attempt = 1; attempt <= test; attempt++)
        {
            //fprintf(stderr, "Test %d: Sending probe (%d/%d) to reflector at %s\n", test, attempt, test, address);

            bytesSent = sendto(meterSocket, sendBuffer.c_str(), sendBuffer.length(), 0, (struct sockaddr *)&reflectorAddress, reflectAddrLen);
            if (bytesSent < 0)
            {
                fprintf(stderr, "%d ", errno);
                perror("sendto()");
                exit(1);
            }

            //Receive reflection
            //while (strlen(recBuffer) < probeSize)
            //{
                bytesReceived = recvfrom(meterSocket, recBuffer, probeSize, 0, (struct sockaddr *) &reflectorAddress, &reflectAddrLen);        
                //fprintf(stderr, "%lu\n", strlen(recBuffer));
                if (bytesReceived < 0)
                {
                    perror("recvfrom()");
                    exit(1);
                }
            //}
            

            if (strcmp(recBuffer, sendBuffer.c_str()) != 0)
            {
                //fprintf(stderr, "Message not equal\n%s\n", recBuffer);
                fprintf(stderr, "Message not equal %lu\n", strlen(recBuffer));
            }

        }

        endTime = high_resolution_clock::now();

        //Calculate RTT of all probes
        duration<double> roundTripTime = duration_cast<duration<double>>(endTime - startTime);

        rttTimes[test-1] = roundTripTime.count();

        fprintf(stderr, "Test %d done RTT = %f\n", test, roundTripTime.count());
    }   

    //Check and process data

    fprintf(stderr, "##########RESULTS##########\n");

    //Average RTT
    double rttSum = 0.0;
    double averageRtt = 0.0;
    for (int i = 0; i < numberOfTests; i++)
    {
        rttSum += rttTimes[i];
    }
    averageRtt = rttSum/numberOfTests;

    fprintf(stderr, "AVERAGE RTT: %f seconds\n", averageRtt);

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