#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "common.h"
#include <poll.h>
#include <bits/poll.h>

extern int common_readed;

struct client_info {
        int fd;
        size_t bytes_received;
        size_t bytes_must_recv;
        char name[256];
};

int tcp_close_client_connection(struct pollfd* pollfd, struct client_info* client_info) {
	close(pollfd->fd);
	pollfd->fd = -1;
	pollfd->events = 0;
	
	close(client_info->fd);
	memset(client_info, 0x0, sizeof(*client_info));
	
	return 0;
}

int tcp_loop ( int sockfd )
{
        int client_sockfd;
        int client_count=0;
        char buffer[BUFSIZE];
        uint32_t status;
        int ready_sockets_count;

        struct sockaddr_in client_addr = {0};
        int size = sizeof ( client_addr );

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
                        client_sockfd = accept ( sockfd, &client_addr, &size ) ;
                        //searching for free entry in socket_fds
                        for ( int j=1; j<dtablesize; j++ ) {
                                if ( socket_fds[j].events == 0 ) {
                                        socket_fds[j].events = POLLIN;
                                        socket_fds[j].fd = client_sockfd;
                                        debug ( "Incoming connection from %s:%hu\n", extract_peer_addr ( client_sockfd ), extract_peer_port ( client_sockfd ) );
                                        memset ( &client_entries[j], 0x0, sizeof client_entries[0] );
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

                                        status  = tcp_recv_uint32 ( client_sockfd, &flen );
                                        if ( status == -1 ) {
                                                perror ( "Can't receive file name size" );
                                                continue;
                                        }

                                        status = recv ( client_sockfd,  client_entries[i].name, flen, MSG_WAITALL );
                                        if ( status==-1 ) {
                                                perror ( "Can't receive file name" );
                                                continue;
                                        }

                                        client_entries[i].fd = open ( client_entries[i].name, O_WRONLY| O_APPEND | O_CREAT, 0755 );
                                        if ( client_entries[i].fd < 0 ) {
                                                tcp_send_uint32 ( client_sockfd, errno ); //send file open status
                                                error ( "%d can't open file %s, errno=%u\n", client_entries[i].name, errno );
                                                close ( client_sockfd );
                                                return -errno;
                                        } else
                                                tcp_send_uint32 ( client_sockfd, 0 );

                                        tcp_send_uint32 ( client_sockfd, file_size ( client_entries[i].fd ) );
                                        debug ( "File name: %s, file size %u bytes\n", client_entries[i].name,
                                                file_size ( client_entries[i].fd ) );
                                }
                                //receive data size to receive
                                else if ( client_entries[i].bytes_must_recv == 0 ) {
                                        tcp_recv_uint32 ( client_sockfd, & ( client_entries[i].bytes_must_recv ) );
                                }
                                //receive data and write to file
                                else {
                                        if ( client_entries[i].bytes_must_recv != client_entries[i].bytes_received ) {
                                                status = recv ( client_sockfd, buffer, sizeof ( buffer ), 0x0 );
                                                client_entries[i].bytes_received +=  status;

                                                if ( status == -1 ||  status == 0 )  {
                                                        debug("Connection with %s:%hu closed\n", extract_peer_addr(client_sockfd), extract_peer_port(client_sockfd) );
							tcp_close_client_connection(socket_fds+i, client_entries+i);
							continue;
                                                } else if ( write ( client_entries[i].fd, buffer,  status ) == -1 )  {
                                                        debug("Can't write to file %s: %s\n", client_entries[i].name, strerror(errno));
							tcp_close_client_connection(socket_fds+i, client_entries+i);
                                                        continue;
                                                }
                                        }

                                        //close connection
                                        if ( client_entries[i].bytes_must_recv == client_entries[i].bytes_received ) {
                                                tcp_send_uint32 ( client_sockfd, errno );
						tcp_close_client_connection(socket_fds+i, client_entries+i);
                                        }
                                }
                        }
                }
        }
        perror ( "poll() call failure" );
        exit ( EXIT_FAILURE );
}
