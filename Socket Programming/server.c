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
#define DBFILE_NAME "db22"

// record structure
struct record {
    int acctnum;
    char name[20];
    float amount;
    int age;
};

int num_of_records; // number of records in db
int *acctnums;      // acctnum array
int lock_flag = -1;
int stop_flag = 0;
int fd;
char servicemap_address[20] = {};
int server_port = 0;
char ip_address[64];

// print error and abort
void errno_abort(const char* header);
// send registration message
int registration();
// create server socket
int create_server_socket(int16_t port);
// preload database
int load_db_index();
// read record
int read_record(int acctnum, struct record* rec);
// update record
int update_record(int acctnum, float amount, struct record* rec);
// find my IP address
int find_ip_address(char* address)
{
    struct ifaddrs * addrs = NULL;
    struct ifaddrs * tmp = NULL;
    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp) 
    {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            if(strcmp(tmp->ifa_name, "lo") != 0) {
                sprintf(address, "%s", inet_ntoa(pAddr->sin_addr));
                break;
            }
        }
        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
}

// test make db
int create_db() 
{
    FILE *fp = fopen(DBFILE_NAME, "w+");
    if(fp == NULL)
        return -1;

    struct record rec;
    rec.acctnum = 111;
    rec.amount = 18.4;
    rec.age = 18;
    sprintf(rec.name, "John");
    
    fwrite(&rec, sizeof(struct record), 1, fp);

    rec.acctnum = 222;
    rec.amount = 34.4;
    rec.age = 18;
    sprintf(rec.name, "Danny");

    fwrite(&rec, sizeof(struct record), 1, fp);
    
    fclose(fp);
    
    return 0;
}

int main(int argc, char* argv[])
{
    // parse command parameters
    if(argc < 3) {
        printf("Usage: server <servicemap_address> <server_port>\n");
        exit(0);
    }
    sprintf(servicemap_address, argv[1], strlen(argv[1]));
    server_port = atoi(argv[2]);

    find_ip_address(ip_address);

    if(load_db_index() < 0) {
        printf("cannot load database.\n");
        exit(0);
    }
    // create server socket
    int serv_sock = create_server_socket(server_port);

    // register to servicemap
    if(registration() < 0)
    {
        printf("registration failed.\n");
        return -1;
    }

    // open db file
    fd = open(DBFILE_NAME, O_RDWR);
    if(fd < 0) {
        printf("db file open failed.\n");
        exit(0);
    }

    // process client's request
    struct sockaddr_in cli_addr;
    socklen_t addr_size;
    int cli_sock;

    addr_size = sizeof(cli_addr);
    while (!stop_flag) {
        // accept client's connection
        cli_sock = accept(serv_sock, (struct sockaddr*)&cli_addr, &addr_size);

        if (cli_sock < 0) 
            errno_abort("accept client");

        int pid = fork();
        if( pid == 0 ) { // child process
            // receive client's request
            char buf[256] = {};
            if (recv(cli_sock, buf, sizeof(buf)-1, 0) < 0)
                errno_abort("recv");
            printf("Service requested from %s\n", inet_ntoa(cli_addr.sin_addr));
            
            int acctnum;
            float amount;
            struct record rec;
            
            int cmd;
            memcpy(&cmd, buf, 4);
            if(cmd == 500) { // query
                memcpy(&acctnum, &buf[4], 4);
                if(read_record(acctnum, &rec) == 0) {
                    // prepare failed message
                    sprintf(buf, "%s %d %.1f\n", rec.name, rec.acctnum, rec.amount);
                }
                else {
                    // prepare success message
                    sprintf(buf, "reading record failed.\n");
                }
                // send message to client
                send(cli_sock, buf, sizeof(buf), 0);
            }
            else if(cmd == 501) { // update
                memcpy(&acctnum, &buf[4], 4);
                memcpy(&amount, &buf[8], 4);

                if(update_record(acctnum, amount, &rec) == 0) {
                    // prepare failed message
                    sprintf(buf, "%s %d %.1f\n", rec.name, rec.acctnum, rec.amount);
                }
                else {
                    // prepare success message
                    sprintf(buf, "updating record failed.\n");
                }
                // send message to client
                send(cli_sock, buf, sizeof(buf), 0);
            }
            else if(cmd == 100) { // qiut
                // do nothing
            }
            else { // unknown command
                // prepare unknown command message
                sprintf(buf, "unknown command.\n");
                // send message to client
                send(cli_sock, buf, sizeof(buf), 0);
            }
    
            shutdown(cli_sock, 2);
            close(cli_sock);            
        }
        else if( pid > 0) { // parent process
        }
        else {
        }    
    }
    close(fd);
    shutdown(serv_sock, 2);
    close(serv_sock);
    return 0;
}

// print error and abort
void errno_abort(const char* header)
{
    perror(header);
    exit(0);
}

// send registration message
int registration()
{
    struct sockaddr_in si_other;
    int sockfd, i=0, slen=sizeof(si_other);
    char buf[256];
    char message[256];
 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        errno_abort("socket");
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(SERVICEMAP_PORT);
     
    if (inet_aton(servicemap_address , &si_other.sin_addr) == 0) 
    {
        errno_abort("inet_aton");
    }
 
    bzero(message, sizeof(message));
    // 
    int h_port = server_port >> 8;
    int l_port = server_port % 256;
    // make address string by replacing dot to comma
    for(int i=0; i<strlen(ip_address); i++) {
        if(ip_address[i] == '.')
            ip_address[i] = ',';
    }
    sprintf(message, "PUT CISBANK %s,%d,%d", ip_address, h_port, l_port);
    // send the message
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &si_other, slen) == -1)
    {
        errno_abort("sendto()");
    }
        
    bzero(buf, sizeof(buf));
    // try to receive some data, this is a blocking call
    if (recvfrom(sockfd, buf, 256, 0, (struct sockaddr *) &si_other, &slen) == -1)
    {
        errno_abort("recvfrom()");
    }
    close(sockfd);

    // check ack
    if(strcmp(buf, "OK") != 0)
        return -1;
    printf("Registration OK from %s\n", inet_ntoa(si_other.sin_addr));
    
    return 0;

}

// create server socket
int create_server_socket(int16_t port)
{
    struct sockaddr_in serv_addr;
    // create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // set reusable option to socket
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    if (sock < 0) {
        errno_abort("server socket");
    }
    // socket structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((unsigned int)port);
    // bind socket
    if (bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        errno_abort("binding socket");
    }
    // listening port
    if (listen(sock, 10) < 0) {
        errno_abort("listening socket");
    }
    return sock;
}

// preload database
int load_db_index()
{
    // get size of db file
    struct stat s;
    int ret;

    ret = stat(DBFILE_NAME, &s);
    if (ret < 0) {
        return -1;
    }
    // get number of records
    num_of_records = s.st_size / sizeof(struct record);
    // fill acctnums
    acctnums = (int*)malloc(sizeof(int) * num_of_records);
    FILE *fp = fopen(DBFILE_NAME, "rb");
    if(fp == NULL) {
        free(acctnums);
        return -1;
    }
    struct record rec;
    for(int i=0; i<num_of_records; i++) {
        fread(&rec, sizeof(struct record), 1, fp);
        acctnums[i] = rec.acctnum;
    }
    fclose(fp);
    return 0;
}

// read record
int read_record(int acctnum, struct record* rec)
{
    // wait until unlock file
    if(lock_flag < 0) {
        usleep(100);
    }
    if(lockf(fd, F_LOCK, sizeof(struct record) * num_of_records) < 0) {
        printf("cannot lock.. %s\n", strerror(errno));
        return -1;
    }
    lock_flag = 0;
    // get record offset
    int idx = 0;
    for(idx = 0; idx <num_of_records; idx++) {
        if(acctnums[idx] == acctnum)
            break;
    }
    if(idx == num_of_records) {
        lockf(fd, F_ULOCK, sizeof(struct record) * num_of_records);
        lock_flag = -1;
        return -1;
    }
    
    // read record
    lseek(fd, sizeof(struct record) * idx, 0);
    read(fd, rec, sizeof(struct record));

    // unlock file
    lockf(fd, F_ULOCK, sizeof(struct record) * num_of_records);
    lock_flag = -1;

    return 0;
}

// update record
int update_record(int acctnum, float amount, struct record* rec)
{
    // wait until unlock file
    if(lock_flag < 0) {
        usleep(100);
    }
    if(lockf(fd, F_LOCK, sizeof(struct record) * num_of_records) < 0) {
        printf("cannot lock.. %s\n", strerror(errno));
        return -1;
    }
    lock_flag = 0;

    // get record offset
    int idx = 0;
    for(idx = 0; idx <num_of_records; idx++) {
        if(acctnums[idx] == acctnum)
            break;
    }
    if(idx == num_of_records) {
        lockf(fd, F_ULOCK, sizeof(struct record) * num_of_records);
        lock_flag = -1;
        return -1;
    }
    
    // read record
    lseek(fd, sizeof(struct record) * idx, 0);
    read(fd, rec, sizeof(struct record));
    // write record
    rec->amount = amount;
    lseek(fd, sizeof(struct record) * idx, 0);
    write(fd, rec, sizeof(struct record));

    // unlock file
    lockf(fd, F_ULOCK, sizeof(struct record) * num_of_records);
    lock_flag = -1;
    
    return 0;
}
