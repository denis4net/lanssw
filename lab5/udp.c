#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"
#include <errno.h>
#include <sys/time.h>
#include <poll.h>

extern int common_readed;

struct client_info {
        int fd;
        size_t bytes_received;
        size_t bytes_must_recv;
        char name[256];
        struct sockaddr_in sockaddr;
};

int udp_mkclient_socket ( int sockfd, uint16_t* port )
{
        int status;
        struct sockaddr_in local_sockaddr;
        socklen_t local_len = sizeof ( local_sockaddr );
        memset ( &local_sockaddr, 0x0, ( size_t ) local_len );
        local_sockaddr.sin_family=AF_INET;
        local_sockaddr.sin_addr.s_addr = htonl ( INADDR_ANY );
        local_sockaddr.sin_port = htons ( 0 );

        int client_sockfd = socket ( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
        if ( client_sockfd < 0 )
                return -1;

        status = bind ( client_sockfd, ( struct sockaddr* ) &local_sockaddr, local_len );
        if ( status < 0 )
                return -2;

        getsockname ( client_sockfd, ( struct sockaddr* ) &local_sockaddr, &local_len );

        if ( port != NULL )
                *port = ntohs ( local_sockaddr.sin_port );

        struct timeval tv;
        tv.tv_usec = 0;
        tv.tv_sec = 5;

        if ( setsockopt ( client_sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof ( tv ) ) ) {
                perror ( "setsockopt(SO_SNDTIMEO) error" );
                return -3;
        }

        if ( setsockopt ( client_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof ( tv ) ) ) {
                perror ( "setsockopt(SO_RCVTIMEO) error" );
                return -4;
        }

        return client_sockfd;
}

int udp_close_client_connection ( struct pollfd* pollfd, struct client_info* client_info )
{
        close ( pollfd->fd );
        pollfd->fd = -1;
        pollfd->events = 0;

        close ( client_info->fd );
        memset ( client_info, 0x0, sizeof ( *client_info ) );

        return 0;
}

int udp_loop ( int sockfd )
{
        int client_sockfd;
        int client_count=0;
        char buffer[BUFSIZE];
        uint32_t status;
        int ready_sockets_count;

        struct sockaddr_in d_sockaddr = {0};
        socklen_t d_socklen = sizeof ( d_sockaddr );

        const int dtablesize = getdtablesize();
        struct pollfd socket_fds[dtablesize];
        memset ( socket_fds, 0x0, sizeof socket_fds );
        struct client_info client_entries[dtablesize];
        memset ( client_entries, 0x0, sizeof client_entries );

        socket_fds[0].fd = sockfd;
        socket_fds[0].events = POLLIN;

        while ( ( ready_sockets_count =  poll ( socket_fds, dtablesize, -1 ) ) >= 0 ) {

                //poll listen socket
                if ( socket_fds[0].revents & POLLIN ) {
                        socket_fds[0].revents = 0x0;

                        uint16_t syn;
                        status = recvfrom ( sockfd, &syn, sizeof ( syn ), MSG_WAITALL,  &d_sockaddr, &d_socklen );

                        debug ( "SYN received from %s:%hu\n", inet_ntoa ( d_sockaddr.sin_addr ),  ntohs ( d_sockaddr.sin_port ) );

                        uint16_t incoming_port;
                        client_sockfd = udp_mkclient_socket ( sockfd, &incoming_port );

                        if ( client_sockfd < 0 ) {
                                perror ( "Can't create udp client socket" );
                                continue;
                        } else {
                                debug ( "Binded %hu for client %s:%hu\n", incoming_port, inet_ntoa ( d_sockaddr.sin_addr ),  ntohs ( d_sockaddr.sin_port ) );
                        }

                        incoming_port = htons ( incoming_port );
                        status = sendto ( sockfd, &incoming_port, sizeof ( incoming_port ), 0x0, &d_sockaddr, sizeof ( d_sockaddr ) );
                        if ( status < 0 ) {
                                perror ( "Can't send data to client via main udp endpoint" );
                                continue;
                        }

                        //searching for free entry in socket_fds
                        for ( int j=1; j<dtablesize; j++ ) {
                                if ( socket_fds[j].events == 0 ) {
                                        socket_fds[j].events = POLLIN;
                                        socket_fds[j].fd = client_sockfd;
                                        debug ( "Incoming connection from %s:%hu\n",  inet_ntoa ( d_sockaddr.sin_addr ) , ntohs ( d_sockaddr.sin_port ) );
                                        memset ( &client_entries[j], 0x0, sizeof client_entries[0] );
                                        client_entries[j].sockaddr = d_sockaddr;
                                        break;
                                }
                        }
                }

                //poll client sockets
                for ( int i=1; i<dtablesize; i++ ) {
                        if ( socket_fds[i].events != 0 && socket_fds[i].revents & POLLIN ) {
                                socket_fds[i].revents &= ~POLLIN;
                                client_sockfd = socket_fds[i].fd;
                                //receive file name, open file, check status, send status to client
                                if ( client_entries[i].name[0] == 0x0 ) {
                                        uint32_t flen, status;

                                        status  = udp_recv_uint32 ( client_sockfd, &flen, & ( client_entries[i].sockaddr ) );
                                        if ( status == -1 ) {
                                                perror ( "Can't receive file name size" );
						udp_close_client_connection ( socket_fds+i, client_entries+i );
                                                continue;
                                        }

                                        status = udp_recv ( client_sockfd,  client_entries[i].name, flen,  & ( client_entries[i].sockaddr ) );
                                        if ( status==-1 ) {
                                                perror ( "Can't receive file name" );
						udp_close_client_connection ( socket_fds+i, client_entries+i );
                                                continue;
                                        }

                                        client_entries[i].fd = open ( client_entries[i].name, O_WRONLY| O_APPEND | O_CREAT, 0755 );
                                        if ( client_entries[i].fd < 0 ) {
                                                udp_send_uint32 ( client_sockfd, errno,  & ( client_entries[i].sockaddr ) ); //send file open status
                                                error ( "%d can't open file %s, errno=%u\n", client_entries[i].name, errno );
						udp_close_client_connection ( socket_fds+i, client_entries+i );
						continue;
                                        } else
                                                udp_send_uint32 ( client_sockfd, 0,  & ( client_entries[i].sockaddr ) );

                                        udp_send_uint32 ( client_sockfd, file_size ( client_entries[i].fd ),  & ( client_entries[i].sockaddr ) );
                                        debug ( "File name: %s, file size %u bytes\n", client_entries[i].name,
                                                file_size ( client_entries[i].fd ) );
                                }
                                //receive data size to receive
                                else if ( client_entries[i].bytes_must_recv == 0 ) {
                                        udp_recv_uint32 ( client_sockfd, & ( client_entries[i].bytes_must_recv ),  & ( client_entries[i].sockaddr ) );

                                        if ( client_entries[i].bytes_must_recv == 0 ) {
                                                
						udp_send_uint32 ( client_sockfd, errno,  & ( client_entries[i].sockaddr ) );
						
						debug ( "Connection with %s:%hu closed\n", inet_ntoa ( d_sockaddr.sin_addr ) , ntohs ( d_sockaddr.sin_port ) );
                                                udp_close_client_connection ( socket_fds+i, client_entries+i );
						continue;
                                        }

                                }
                                //receive data and write to file
                                else {
                                        if ( client_entries[i].bytes_must_recv != client_entries[i].bytes_received ) {
                                                status = udp_recv ( client_sockfd, buffer, sizeof ( buffer ),  & ( client_entries[i].sockaddr ) );
                                                client_entries[i].bytes_received +=  status;

                                                if ( status == -1 )  {
                                                        debug ( "Connection with %s:%hu closed\n", inet_ntoa ( d_sockaddr.sin_addr ) , ntohs ( d_sockaddr.sin_port ) );
                                                        udp_close_client_connection ( socket_fds+i, client_entries+i );
                                                        continue;
                                                } else if ( write ( client_entries[i].fd, buffer,  status ) == -1 )  {
                                                        debug ( "Can't write to file %s: %s\n", client_entries[i].name, strerror ( errno ) );
                                                        udp_close_client_connection ( socket_fds+i, client_entries+i );
                                                        continue;
                                                }
                                        }

                                        //close connection
                                        if ( client_entries[i].bytes_must_recv == client_entries[i].bytes_received ) {
                                                udp_send_uint32 ( client_sockfd, errno,  & ( client_entries[i].sockaddr ) );
                                                debug ( "Connection with %s:%hu closed\n", inet_ntoa ( d_sockaddr.sin_addr ) , ntohs ( d_sockaddr.sin_port ) );
                                                udp_close_client_connection ( socket_fds+i, client_entries+i );
						continue;
                                        }
                                }
                        }
                }
        }

        perror ( "poll() call failure" );
        exit ( EXIT_FAILURE );
}

