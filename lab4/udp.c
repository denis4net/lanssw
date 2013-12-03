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


extern int common_readed;

int udp_mkclient_socket ( int sockfd, uint16_t* port )
{
        int status;
        struct sockaddr_in local_sockaddr;
        socklen_t local_len = sizeof(local_sockaddr);
        memset ( &local_sockaddr, 0x0, ( size_t ) local_len );
	local_sockaddr.sin_family=AF_INET;
	local_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_sockaddr.sin_port = htons(0);
	
        int client_sockfd = socket ( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
        if ( client_sockfd < 0 )
                return -1;

        status = bind ( client_sockfd, ( struct sockaddr* ) &local_sockaddr, local_len );
        if ( status < 0 )
                return -2;
	
	getsockname(client_sockfd, ( struct sockaddr* ) &local_sockaddr, &local_len);
	
	if(port != NULL)
		*port = ntohs ( local_sockaddr.sin_port );
	
	struct timeval tv;
        tv.tv_usec = 0;
	tv.tv_sec = 5;
        
        if ( setsockopt(client_sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) ) {
                perror ( "setsockopt(SO_SNDTIMEO) error" );
                return -3;
        }
        
	if ( setsockopt(client_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) ) {
                perror ( "setsockopt(SO_RCVTIMEO) error" );
                return -4;
        }
	
        return client_sockfd;
}

int udp_client_handler ( int client_sockfd, struct sockaddr_in d_sockaddr )
{
        uint8_t buffer[BUFSIZE];
        uint32_t flen, status;
	
	debug("Background handler for client %s:%hu\n", 
	       inet_ntoa(d_sockaddr.sin_addr), ntohs(d_sockaddr.sin_port));	
        
	status  = udp_recv_uint32 ( client_sockfd, &flen,  &d_sockaddr );
        if ( status == -1 || flen == 0) {
                perror ( "Can't receive file name size or filename string empty. ACK Time out exceeded." );
                return -1;
        }

        char fname[flen+1];
        memset ( fname, 0x0, sizeof ( fname ) );
	
        if ( udp_recv ( client_sockfd, fname, flen,  &d_sockaddr ) <0 ) {
                perror ( "Can't receive file name. ACK Time out exceeded" );
                return -2;
        }
        
        debug("Opening file %s:", fname);
	int fd = open ( fname, O_WRONLY| O_APPEND | O_CREAT, 0755 );
        if ( fd < 0 ) {
                udp_send_uint32 ( client_sockfd, errno, &d_sockaddr ); //send file open status
                error ( stderr, "%d can't open file %s, errno=%u\n", fname, errno );
                close ( client_sockfd );
                return -errno;

        } else {
                debug("done\n");
		udp_send_uint32 ( client_sockfd, 0, &d_sockaddr );
	}
        //send file offset
        {
                udp_send_uint32 ( client_sockfd, file_size ( fd ), &d_sockaddr );
                debug ( " File name: %s, file size %u bytes\n",fname, file_size ( fd ) );
        }

        //receiving data size
        uint32_t data_size;
        udp_recv_uint32 ( client_sockfd, &data_size, &d_sockaddr );

        while ( common_readed != data_size ) {
                status = udp_recv ( client_sockfd, buffer, sizeof ( buffer ), &d_sockaddr );
                common_readed +=  status;

                if ( status == -1 ||  status == 0 )  {
                        perror ( "Client disconected or exceeded count of max tries when data has been sended via udp" );
                        break;
                } else if ( write ( fd, buffer,  status ) == -1 )  {
                        perror ( "Can't write to file" );
                        break;
                }
        }

        if ( common_readed == data_size ) {
                debug("All data received");
        } else {
                error ( stderr, "Not all data received from client via UDP endpoint\n" );
        }

	udp_send_uint32 ( client_sockfd, errno, &d_sockaddr);
	
        debug ( "Closing  UDP endpoint with %s:%hu\n",
                 inet_ntoa ( d_sockaddr.sin_addr ), ntohl ( d_sockaddr.sin_port ) );

        close ( fd );
        close ( client_sockfd );
}

int udp_loop ( int sockfd )
{
        int status;
        uint16_t recv_data;
        struct sockaddr_in d_sockaddr;
        socklen_t d_len = sizeof ( d_sockaddr );;

        while ( status = recvfrom ( sockfd, &recv_data, sizeof ( recv_data ), MSG_WAITALL,  &d_sockaddr, &d_len )  > 0 ) {
                //Receiving SYN
		if ( ntohs ( recv_data ) == UDP_SYN ) {
			debug("SYN received from %s:%hu\n", inet_ntoa(d_sockaddr.sin_addr),  ntohs (d_sockaddr.sin_port) );
			//Making udp endpoint and binding on free udp port
			uint16_t port;
                        const int client_sockfd = udp_mkclient_socket ( sockfd, &port );
			if ( client_sockfd < 0 ) {
                                perror ( "Can't create udp client socket" );
                                continue;
                        } else {
				debug("Binded %hu for client %s:%hu\n", port, inet_ntoa(d_sockaddr.sin_addr),  ntohs (d_sockaddr.sin_port));
			}
			
                        //Send ACK
                        port = htons(port);
                        status = sendto(sockfd, &port, sizeof(port), 0x0, &d_sockaddr, sizeof(d_sockaddr));
			if( status < 0) {
				perror("Can't send data to client via main udp endpaoint");
				continue;
			}
			
			pid_t pid = fork();
			switch( pid ) {
				case -1:
					perror("Can't create child process");
					break;
				case 0:
					udp_client_handler(client_sockfd, d_sockaddr);
					exit(EXIT_SUCCESS);
					break;
				default:
					memset(&d_sockaddr, 0x0, sizeof(d_sockaddr));
					continue;
			}
                }
        }
        
        perror("Main udp recv loop error");
        
        return 0;
}

