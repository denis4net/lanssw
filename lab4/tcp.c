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

extern int common_readed;

int tcp_session ( int client_sockfd )
{
        fcntl ( client_sockfd, F_SETOWN, getpid() );

	uint8_t buffer[BUFSIZE];
        uint32_t flen, status;

        status  = recv_uint32( client_sockfd, &flen);
        if ( status == -1 ) {
                perror ( "Can't receive file name size" );
                return -1;
        }

        char fname[flen+1];
        memset ( fname, 0x0, sizeof ( fname ) );
        status = recv ( client_sockfd, fname, flen, MSG_WAITALL );
        if ( status==-1 ) {
                perror ( "Can't receive file name" );
                return -2;
        }

        int fd = open ( fname, O_WRONLY| O_APPEND | O_CREAT, 0755 );
        send_uint32 ( client_sockfd, errno ); //send file open status

        //send file offset
        {
                send_uint32 ( client_sockfd, file_size ( fd ) );
                printf ( "File name: %s, file size %u bytes\n", fname, file_size ( fd ) );
        }

        //receiving data size
        uint32_t data_size, display_status;
        recv_uint32 ( client_sockfd, &data_size);
	
        while ( common_readed != data_size ) {
                status = recv ( client_sockfd, buffer, sizeof ( buffer ), 0x0 );
                common_readed +=  status;

                if ( status == -1 ||  status == 0 )  {
                        perror ( "\nClient disconected" );
                        break;
                } else if ( write ( fd, buffer,  status ) == -1 )  {
                        perror ( "\nCan't write to file" );
                        break;
                }
        }

        if ( common_readed == data_size ) {
                status=0;
                send ( client_sockfd, &status, sizeof ( status ), 0x0 );
		//printf( "\nAll data received\n" );
        } else {
                fprintf ( stderr, "\nNot all data received from client\n" );
        }
	 printf ( "\nClosing connection with %s:%hu\n", 
		  get_peer_addr(client_sockfd), get_peer_port(client_sockfd) );
	 
        close ( fd );
        close ( client_sockfd );
}

int tcp_loop ( int sockfd )
{
	int client_sockfd;
        int client_count=0;
	char buffer[BUFSIZE];
        struct sockaddr_in client_addr;
    
        int size = sizeof ( client_addr );

        while ( ( client_sockfd = accept ( sockfd, &client_addr, &size ) ) != -1 ) {
		printf("Incomming connection from %s:%hu via TCP\n", get_peer_addr(sockfd), get_peer_port(sockfd));
		
                pid_t pid = fork();
                switch ( pid ) {
                case -1:
                        perror ( "Can't create child process" );
                        break;
                case 0:
                        close ( sockfd );
                        tcp_session(client_sockfd);
                        exit ( EXIT_SUCCESS );
                        break;
                default:
			close(client_sockfd);
                        client_count++;
                        break;
                }
        }

        perror ( "Socket accept error" );
        exit ( EXIT_FAILURE );
}