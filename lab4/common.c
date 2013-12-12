#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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
#include "common.h"


int udpv4_bind ( const char* ipv4, const char* tcp_port )
{
        unsigned short int port = atoi ( tcp_port );
        in_addr_t ip = inet_addr ( ipv4 );

        struct sockaddr_in sockaddr;
        memset ( &sockaddr, 0x0, sizeof ( sockaddr ) );
        sockaddr.sin_family = PF_INET;
        sockaddr.sin_port = htons ( port );
        sockaddr.sin_addr.s_addr = ip;

        int sockfd = socket ( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
        int opt=1;
        setsockopt ( sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof ( opt ) );
        if ( bind ( sockfd, &sockaddr, sizeof ( sockaddr ) ) )
                return -1;

        //recv timeout set option
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if ( setsockopt ( sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof ( tv ) ) < 0 ) {
                perror ( "setsockopt() error" );
                return -3;
        }

        return sockfd;
}

int tcpv4_bind ( const char* ipv4, const char* tcp_port )
{
	debug("addr=%s, port=%s\n", ipv4, tcp_port);
        unsigned short int port = atoi ( tcp_port );
        in_addr_t ip = inet_addr ( ipv4 );

        struct sockaddr_in sockaddr;
        memset ( &sockaddr, 0x0, sizeof ( sockaddr ) );
        sockaddr.sin_family = PF_INET;
        sockaddr.sin_port = htons ( port );
        sockaddr.sin_addr.s_addr = ip;

        int sockfd = socket ( PF_INET, SOCK_STREAM, IPPROTO_TCP );
        int opt=1;
        setsockopt ( sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof ( opt ) );
        if ( bind ( sockfd, &sockaddr,  sizeof ( sockaddr ) ) )
                return -1;

        return sockfd;
}

const char* extract_peer_addr ( int sockfd )
{
        struct sockaddr_in addr;
        socklen_t len = sizeof ( addr );
        static char s_addr[17];
        getpeername ( sockfd, ( struct sockaddr* ) &addr, &len );
        inet_ntop ( AF_INET, ( void* ) & ( addr.sin_addr ),  s_addr, sizeof ( s_addr ) );
        return ( const char* ) s_addr;
}


const uint16_t extract_peer_port ( int sockfd )
{
        struct sockaddr_storage addr;
        socklen_t len = sizeof ( addr );
        getpeername ( sockfd, ( struct sockaddr* ) &addr, &len );
        struct sockaddr_in* addr_in = ( struct sockaddr_in* ) &addr;
        return ntohs ( addr_in->sin_port );
}

const uint16_t extract_bind_port ( int sockfd )
{
        struct sockaddr_storage addr;
        socklen_t len = sizeof ( addr );
        getsockname ( sockfd, ( struct sockaddr* ) &addr, &len );
        struct sockaddr_in* addr_in = ( struct sockaddr_in* ) &addr;
        return ntohs ( addr_in->sin_port );
}

int tcp_send_uint32 ( int sockfd, uint32_t data )
{
        data = htonl ( data );
        return send ( sockfd, &data, sizeof ( data ), NULL );
}

int tcp_recv_uint32 ( int sockfd, uint32_t* data )
{
        int r = recv ( sockfd, data, sizeof ( uint32_t ), 0x0 );
        *data = ntohl ( *data );
        return r;
}

int udp_send ( int sockfd, uint8_t* data, size_t size, struct sockaddr_in* d_sockaddr )
{
        int32_t recv_status, send_status;
        socklen_t sockaddr_len = sizeof ( struct sockaddr_in );
        int count=0;
        do {
                if ( count >UDP_TRIES )
                        return -1;

                send_status = sendto ( sockfd, data, size, NULL, ( struct sockaddr* ) d_sockaddr,  sockaddr_len );
                if ( send_status < 0 ) {
                        perror ( "Can't send data via udp" );
                        return -1;
                }

                recvfrom ( sockfd, &recv_status, sizeof ( recv_status ), 0, ( struct sockaddr* ) d_sockaddr, &sockaddr_len );
                recv_status = ntohl ( recv_status );
                count++;
        } while ( send_status != recv_status );

        return send_status;
}

int udp_recv ( int sockfd, uint8_t* data, size_t size, struct sockaddr_in* sockaddr )
{
        uint32_t recv_status, send_status;
        socklen_t sockaddr_len = sizeof ( struct sockaddr_in );
        int tries=0;

        do {
                if ( tries>UDP_TRIES )
                        return -1;

                recv_status = recvfrom ( sockfd, data, size, MSG_WAITALL, ( struct sockaddr* ) sockaddr, &sockaddr_len );
                send_status = htonl(recv_status);
		sendto ( sockfd, &send_status, sizeof ( send_status ), 0, ( struct sockaddr* ) sockaddr, sockaddr_len );
		tries++;
        } while (recv_status!=size );

        return recv_status;
}

int udp_send_uint32 ( int sockfd, uint32_t data, struct sockaddr_in* d_sockaddr )
{
        data = htonl ( data );
        return udp_send ( sockfd, &data, sizeof ( data ), d_sockaddr );
}

int udp_recv_uint32 ( int sockfd, uint32_t *data, struct sockaddr_in* d_sockaddr )
{
        int status = udp_recv ( sockfd, data, sizeof ( uint32_t ), d_sockaddr );
        *data = ntohl ( *data );
        return status;
}

const char* addr_to_ip_string ( const struct sockaddr_in addr )
{
        static char s_addr[17];
        inet_ntop ( AF_INET, ( void* ) & ( addr.sin_addr.s_addr ),  s_addr, sizeof ( s_addr ) );
        return s_addr;
}

inline off_t file_size ( int fd )
{
        struct stat _stat;
        fstat ( fd, &_stat );
        return _stat.st_size;
}
