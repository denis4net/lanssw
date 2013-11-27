#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>


int udpv4_bind(const char* ipv4, const char* tcp_port)
{
    unsigned short int port = atoi( tcp_port );
    in_addr_t ip = inet_addr( ipv4 );

    struct sockaddr_in sockaddr;
    memset( &sockaddr, 0x0, sizeof(sockaddr) );
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_port = htons( port );
    sockaddr.sin_addr.s_addr = ip;
    
    int sockfd = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
    int opt=1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if( bind( sockfd, &sockaddr, sizeof( sockaddr ) ) )
        return -1;
    return sockfd;
}

int tcpv4_bind(const char* ipv4, const char* tcp_port)
{
    unsigned short int port = atoi( tcp_port );
    in_addr_t ip = inet_addr( ipv4 );

    struct sockaddr_in sockaddr;
    memset( &sockaddr, 0x0, sizeof(sockaddr) );
    sockaddr.sin_family = PF_INET;
    sockaddr.sin_port = htons( port );
    sockaddr.sin_addr.s_addr = ip;

    int sockfd = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    int opt=1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if( bind( sockfd, &sockaddr, sizeof( sockaddr ) ) )
        return -1;
    return sockfd;
}

int udp_send(int sockfd, void* buf, int size);
int udp_recv(int scokfd, void* buf, int size);
int tcp_send(int sockfd, void* buf, int size);
int tpc_recv(int sockfd, void* buf, int size);

inline off_t file_size(int fd)
{
    struct stat _stat;
    fstat(fd, &_stat);
    return _stat.st_size;
}
