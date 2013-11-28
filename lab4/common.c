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

const char* get_peer_addr(int sockfd)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	static char s_addr[17];
	getpeername(sockfd, (struct sockaddr*) &addr, &len);
	inet_ntop(AF_INET, (void*) &(addr.sin_addr),  s_addr, sizeof(s_addr));
	return (const char*) s_addr;
}

const uint16_t get_peer_port(int sockfd)
{
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	getpeername(sockfd, (struct sockaddr*) &addr, &len);
	struct sockaddr_in* addr_in = (struct sockaddr_in*) &addr;
	return ntohs(addr_in->sin_port);
}

int send_uint32(int sockfd, uint32_t data)
{
		data = htonl(data);
		return send(sockfd, &data, sizeof(data), NULL);
}

int recv_uint32(int sockfd, uint32_t* data)
{
	int r = recv(sockfd, data, sizeof(uint32_t), 0x0);
	*data = ntohl(*data);
	return r;
}

inline off_t file_size(int fd)
{
    struct stat _stat;
    fstat(fd, &_stat);
    return _stat.st_size;
}
