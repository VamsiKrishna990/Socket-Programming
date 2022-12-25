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

#define SERVERPORT 20437 // student ID
#define MAX_ADDRESS_COUNT   16
struct address_record {
    char name[20];
    char address[64];
};
struct address_record table[MAX_ADDRESS_COUNT]; // we are using table with 16 entries
int num_of_addresses;

void errno_abort(const char* header)
{
    perror(header);
    exit(0);
}

int main(int argc, char* argv[])
{
    struct sockaddr_in si_me, si_other;
     
    int sockfd, i, slen = sizeof(si_other) , recv_len;
    char buf[256];
     
    // create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        errno_abort("socket");
    }
     
    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));
     
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(SERVERPORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
     
    // bind socket to port
    if(bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        errno_abort("bind");
    }
     
    printf("Servicemap started...\n");
    // keep listening for data
    while(1)
    {
		fflush(stdout);
        bzero (buf, 256);        
        // try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(sockfd, buf, 256, 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
            errno_abort("recvfrom()");
        }
         
        // print details of the client/peer and the data received
        printf("Received from %s: %s\n", inet_ntoa(si_other.sin_addr), buf);

        // parse message
        char cmd[64] = {};
        char name[64] = {};
        char address[64] = {};
        int startIndex = 0, index;
        for(index = 0; index < strlen(buf); index++) {
            if(buf[index] == ' ') {
                memcpy(cmd, buf, index - startIndex);
                break;
            }
        }
        startIndex = index + 1;
        for(index = startIndex; index < strlen(buf); index++) {
            if(buf[index] == ' ') {
                memcpy(name, &buf[startIndex], index - startIndex);
                break;
            }
        }
        if(index == strlen(buf)) {
            memcpy(name, &buf[startIndex], index - startIndex);
        }
        else {
            startIndex = index + 1;
            if(startIndex < strlen(buf)) {
                memcpy(address, &buf[startIndex], strlen(buf) - startIndex);
            }
        }
        memset(buf, 0, sizeof(buf));
        // process command
        if(strncasecmp(cmd, "GET", strlen("GET")) == 0) { // from client
            int found = -1;
            // lookup address table
            for(int i=0; i<num_of_addresses; i++) {
                if(strcmp(table[i].name, name) == 0) {
                    sprintf(buf, "%s", table[i].address);
                    found = 0;
                    break;
                }
            }
            if(found != 0) {
                sprintf(buf, "FAIL");
            }
        }
        else if(strncasecmp(cmd, "PUT", strlen("PUT")) == 0) { // from server
            if(num_of_addresses < MAX_ADDRESS_COUNT) {
                sprintf(table[num_of_addresses].name, "%s", name);
                sprintf(table[num_of_addresses].address, "%s", address);
                sprintf(buf, "OK");
                num_of_addresses++;
            }
            else {
                sprintf(buf, "FAIL");
            }
        }
        else {
            // unknown command
            sprintf(buf, "FAIL");
        }

        // now reply the client 
        if (sendto(sockfd, buf, strlen(buf)+1, 0, (struct sockaddr*) &si_other, slen) == -1)
        {
            errno_abort("sendto()");
        }
		
		bzero(buf, sizeof(buf));
    }
 
    close(sockfd);
    return 0;
}

