#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>

#define SERVICEMAP_PORT   20437 // student ID
char servicemap_address[20] = {};

// print error and abort
void errno_abort(const char* header);
// request server address
int request_server_info(char* group_name, char* ip, int* port);
int parse_address(char* address, char* ip, int* port);
// request server address
int request_to_server(char* ip, uint16_t port, char* request, int receive_flag);

int main(int argc, char* argv[])
{
    if(argc < 2) {
        printf("Usage: client <servicemap_address>\n");
        exit(0);
    }
    sprintf(servicemap_address, argv[1], strlen(argv[1]));

    char ip[64] = {};
    int port = 0;
    if(request_server_info("CISBANK", ip, &port) < 0) {
        printf("Request server info failed.\n");
        exit(0);
    }

    char message[256];
    int i = 0;
    while(1) {
        printf(">");
        // get key input
        bzero(message, sizeof(message));
		do
		{
			message[i] = getchar();
			i++;
		}
		while (message[i-1] != '\n');
		i = 0;
                
        char szCmd[64] = {};
        char szAcctnum[64] = {};
        char szAmount[64] = {};
        int startIndex = 0, index;
        for(index = 0; index < strlen(message); index++) {
            if(message[index] == ' ') {
                memcpy(szCmd, message, index - startIndex);
                break;
            }
        }
        if(index == strlen(message)) {
            memcpy(szCmd, message, index - startIndex);
        }
        else {
            startIndex = index + 1;
            for(index = startIndex; index < strlen(message); index++) {
                if(message[index] == ' ') {
                    memcpy(szAcctnum, &message[startIndex], index - startIndex);
                    break;
                }
            }
            if(index == strlen(message)) {
                memcpy(szAcctnum, &message[startIndex], index - startIndex);
            }
            else {
                startIndex = index + 1;
                if(startIndex < strlen(message)) {
                    memcpy(szAmount, &message[startIndex], strlen(message) - startIndex);
                }
            }
        }
        

        int acctnum = atoi(szAcctnum);
        float amount = atof(szAmount);
        int commandID = 0;
        // process command
        char buf[16] = {};
        if(strncasecmp(szCmd, "query", strlen("query")) == 0) { // query
            commandID = 500;
            memcpy(buf, &commandID, 4);
            memcpy(&buf[4], &acctnum, 4);
            // send request to server
            request_to_server(ip, port, buf, 0);
        }
        else if(strncasecmp(szCmd, "update", strlen("update")) == 0) { // update
            commandID = 501;
            memcpy(buf, &commandID, 4);
            memcpy(&buf[4], &acctnum, 4);
            memcpy(&buf[8], &amount, 4);
            // send request to server
            request_to_server(ip, port, buf, 0);
        }
        else if(strncasecmp(szCmd, "quit", strlen("quit")) == 0) { // quit
            commandID = 100;
            memcpy(buf, &commandID, 4);
            // send request to server
            request_to_server(ip, port, buf, -1);
            break;
        }
        else {
            // unknown command
            printf("unknown command.\n");
        }
    }
    return 0;
}

// print error and abort
void errno_abort(const char* header)
{
    perror(header);
    exit(0);
}

// request to server
int request_to_server(char* ip, uint16_t port, char* request, int receive_flag)
{
    struct hostent* hinfo;
    struct sockaddr_in server_addr;
    // create client socket
    int cli_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_socket < 0) {
        printf("client socket creation failed!\n");
        return -1;
    }
    // convert host name to ip address
    hinfo = gethostbyname(ip);
    if (hinfo == NULL)
        return -1;
    // initialize socket structure
    memcpy(&server_addr.sin_addr, hinfo->h_addr, hinfo->h_length);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    // connecting to host
    char msg[256];
    if (connect(cli_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        sprintf(msg, "cannnot connect to peer %s.\n", ip);
        printf("%s", msg);
        close(cli_socket);
        return -1;
    }
    // sprintf(msg, "connected to peer %s.\n", ip);
    // printf("%s", msg);

    // send request
    send(cli_socket, request, 16, 0);

    if(receive_flag == 0) {
        // receive result
        char buf[256] = {};
        recv(cli_socket, buf, sizeof(buf), 0);
        
        printf("%s", buf);
    }
    shutdown(cli_socket, 2); 
    close(cli_socket);

    return 0;
}

int parse_address(char* address, char* ip, int* port) 
{
    int comma_count = 0;
    int savedIdx = 0;
    int i;
    // get ip address part
    for(i=0; i<strlen(address); i++) {
        if(address[i] == ',') {
            comma_count++;
        }
        if(comma_count == 4) {
            memcpy(ip, address, i);
            savedIdx = i + 1;
            break;
        }
    }
    // replace comma to dot
    for(i=0; i<strlen(ip); i++) {
        if(ip[i] == ',')
            ip[i] = '.';
    }
    // get high port 
    char hport[4] = {};
    for(i=savedIdx; i<strlen(address); i++) {
        if(address[i] == ',') {
            memcpy(hport, &address[savedIdx], i - savedIdx);
            savedIdx = i + 1;
            break;
        }
    }
    (*port) = atoi(hport);
    (*port) = (*port) << 8;
    // get low port
    (*port) += atoi(&address[savedIdx]);

    return 0;
}

// request server address
int request_server_info(char* group_name, char* ip, int* port)
{
    struct sockaddr_in si_other;
    int sockfd, i=0, slen=sizeof(si_other);
    char buf[256];
    char message[256];
 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        errno_abort("socket");
    }
    // set timeout to socket
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        errno_abort("socket");
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(SERVICEMAP_PORT);
     
    if (inet_aton(servicemap_address , &si_other.sin_addr) == 0) 
    {
        printf("inet_aton() error.\n");
        exit(0);
    }
 
    bzero(message, sizeof(message));
    sprintf(message, "GET %s", group_name);
    // send the message
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &si_other, slen) == -1)
    {
        errno_abort("sendto()");
    }
        
    bzero(buf, sizeof(buf));
    // try to receive some data, this is a blocking call
    if (recvfrom(sockfd, buf, 256, 0, (struct sockaddr *) &si_other, &slen) == -1)
    {
        return -1;
    }
    close(sockfd);

    if(strncasecmp(buf, "FAIL", sizeof("FAIL")) == 0) {
        return -1;
    }
    // parse address
    if(parse_address(buf, ip, port) < 0)
        return -1;
    printf("Service provided by %s at %d\n", ip, *port);

    return 0;
}

