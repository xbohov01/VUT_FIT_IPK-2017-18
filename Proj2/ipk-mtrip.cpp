#include <cstdlib>
#include <algorithm> 
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
#include <math.h>
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

//Calculates how many test iterations fit into given timeout
int calculateTestSet(int timeout)
{
    fprintf(stderr, "Calculating test iterations for %d second time limit\n", timeout);
    int tests = 1;
    int iterations = 1;
    for (iterations; tests < timeout; iterations++)
    {
        tests += iterations;
    }
    //Correct for the last loop
    tests -= iterations;
    iterations -= 2;
    fprintf(stderr, "%d tests in %d iterations for %d second time limit\n", tests, iterations, timeout);
    return iterations;
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

    //Socket timeout
    //TODO DYNAMIC ADJUST
    //int timeoutSec = round(testTimeout / 55);
    int timeoutSec = 1;
    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    setsockopt(meterSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

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
    int numberOfTests = calculateTestSet(testTimeout);
    int failedTests = 0;
    int firstFailed = 0;

    double rtt[numberOfTests];

    //Tests
    for (int test = 1; test <= numberOfTests; test++)
    {
        fprintf(stderr, "Test %d start\n", test);

        startTime = high_resolution_clock::now();

        bool secondChance = false;
        bool failedTest = false;

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
            bytesReceived = recvfrom(meterSocket, recBuffer, probeSize, 0, (struct sockaddr *) &reflectorAddress, &reflectAddrLen);        
            if (bytesReceived < 0)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    perror("recvfrom()");
                    exit(1);
                }
                else 
                {
                    if (secondChance == false)
                    {
                        fprintf(stderr, "Test %d: packet failed. Giving second chance.\n", test);
                        secondChance = true;
                        continue;
                    }
                    else
                    {
                        failedTest = true;
                        failedTests++;
                        if (firstFailed == 0)
                        {
                            firstFailed = test;
                        }
                        fprintf(stderr, "Test %d: packet failed. No second chance, aborting.\n", test);
                        break;
                    }
                    
                }
                
            }

            //If reflected message is not the same it means the probe has failed
            if (strcmp(recBuffer, sendBuffer.c_str()) != 0)
            {
                fprintf(stderr, "Message not equal %lu\n", strlen(recBuffer));
                if (firstFailed == 0)
                {
                    firstFailed = test;
                }
            }

        }

        endTime = high_resolution_clock::now();

        //Calculate RTT of all probes
        //Correction for failure timeout
        duration<double> roundTripTime = duration_cast<duration<double>>(endTime - startTime);
        if (secondChance == true)
        {
            rtt[test-1] = roundTripTime.count();
            rtt[test-1] -= timeoutSec * 1.0;
            if (failedTest == true)
            {
                rtt[test-1] -= timeoutSec * 1.0;
            }
        }
        else
        {
            rtt[test-1] = roundTripTime.count();
        }
        
        fprintf(stderr, "Test %d done RTT = %f\n", test, rtt[test-1]);
    }   

    //Check and process data

    fprintf(stderr, "##########RESULTS##########\n");

    if ((failedTests+1) == numberOfTests)
    {
        fprintf(stderr, "All tests seem to have failed, check connection.\n");
    }
    else
    {
        //Average RTT
        double rttSum = 0.0;
        double averageRtt = 0.0;
        for (int i = 0; i < numberOfTests; i++)
        {
            rttSum += rtt[i];
        }
        averageRtt = rttSum/numberOfTests;

        fprintf(stderr, "AVERAGE RTT: %f seconds\n", averageRtt);

        //Number of bytes in message + UDP header
        int sizeOfAttempt = probeSize * sizeof(char) + 8;
        
        //Because it goes around??
        sizeOfAttempt *= 2;

        //Avg speed
        double mbits[numberOfTests];
        double speed[numberOfTests];
        double speedSum = 0.0;

        int limit;
        if (firstFailed > 0)
        {
            limit = firstFailed;
        }
        else
        {
            limit = numberOfTests;
        }

        for (int i = 1; i <= limit; i++)
        {
            int sizeOfTest = sizeOfAttempt * i;
            sizeOfTest *= 8;
            mbits[i-1] = sizeOfTest / 1000000.0;
            speed[i-1] = mbits[i-1] / rtt[i-1];
            speedSum += speed[i-1];
        }
        double avgSpeed = speedSum / limit;
        fprintf(stderr, "AVG SPEED: %f Mbits/s\n", avgSpeed);

        //Min speed
        double min = 0.0;
        min = *std::min_element(speed, speed+numberOfTests);
        fprintf(stderr, "MIN SPEED: %f Mbits/s\n", min);

        //Max speed
        //If something failed then the previous is the best
        //If nothing failed find the max
        if (firstFailed > 0)
        {
             fprintf(stderr, "MAX SPEED: %f Mbits/s\n", speed[limit-1]);
        }  
        else
        {
            double max = 0.0;
            max = *std::max_element(speed, speed+numberOfTests);
            fprintf(stderr, "MAX SPEED: %f Mbits/s\n", max);
        }   

        //Standard deviation
        //Variance
        double variance = 0.0;
        for (int i = 0; i < limit; i++)
        {
            double difference = speed[i] - avgSpeed;
            variance += difference * difference;
        }
        variance /= limit;
        double stdDeviation = sqrt(variance);
        fprintf(stderr, "STD DEVIATION: %f \n", stdDeviation);
    }

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

        if (size > 32768)
        {
            cerr << "WARNING: Probe size you set is higher than 32768, which may cause tests to fail.\nConsider using smaller size\n";
        }
        if (timeout < 55)
        {
            cerr << "WARNING: The timeout for measurement you set is too short, which may cause inaccurate results.\nRecommended timeout is 60 seconds.\n";
        }

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