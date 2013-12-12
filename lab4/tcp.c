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

int tcp_session_handler ( int client_sockfd )
{

        debug ( "%d Incomming connection from %s:%hu via TCP\n", getpid(),
                 extract_peer_addr ( client_sockfd ), extract_peer_port ( client_sockfd ) );

        fcntl ( client_sockfd, F_SETOWN, getpid() );

        uint8_t buffer[BUFSIZE];
        uint32_t flen, status;

        status  = tcp_recv_uint32 ( client_sockfd, &flen );
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
        if ( fd < 0 ) {
                tcp_send_uint32 ( client_sockfd, errno ); //send file open status
                error ( "%d can't open file %s, errno=%u\n", fname, errno );
                close ( client_sockfd );
                return -errno;

        } else
                tcp_send_uint32 ( client_sockfd, 0 );

        //send file offset
        {
                tcp_send_uint32 ( client_sockfd, file_size ( fd ) );
                debug ( "%d File name: %s, file size %u bytes\n", getpid(), fname, file_size ( fd ) );
        }

        //receiving data size
        uint32_t data_size, display_status;
        tcp_recv_uint32 ( client_sockfd, &data_size );

        while ( common_readed != data_size ) {
                status = recv ( client_sockfd, buffer, sizeof ( buffer ), 0x0 );
                common_readed +=  status;

                if ( status == -1 ||  status == 0 )  {
                        perror ( "Client disconected" );
                        break;
                } else if ( write ( fd, buffer,  status ) == -1 )  {
                        perror ( "Can't write to file" );
                        break;
                }
        }

        if ( common_readed == data_size ) {
                debug("All data received\n");
        } else {
                error ( stderr, "%d Not all data received from client\n", getpid() );
        }
        
        debug ( "%d Closing connection with %s:%hu\n",
                 getpid(),
                 extract_peer_addr ( client_sockfd ), extract_peer_port ( client_sockfd ) );

	tcp_send_uint32 ( client_sockfd, errno);
	
        close ( fd );
        close ( client_sockfd );
}

int tcp_loop (const char* opt_addr, const char* opt_port)
{
	int sockfd =  tcpv4_bind ( opt_addr, opt_port );
	if( listen ( sockfd, 1 ) <  0)
		perror("listen");
	
        int client_sockfd;
        int client_count=0;
        char buffer[BUFSIZE];
        struct sockaddr_in client_addr;

        int size = sizeof ( client_addr );

        while ( ( client_sockfd = accept ( sockfd, &client_addr, &size ) ) != -1 ) {


                pid_t pid = fork();
                switch ( pid ) {
                case -1:
                        perror ( "Can't create child process" );
                        break;
                case 0:
                        close ( sockfd );
                        tcp_session_handler ( client_sockfd );
                        exit ( EXIT_SUCCESS );
                        break;
                default:
                        close ( client_sockfd );
                        client_count++;
                        break;
                }
        }

        perror ( "Socket accept error" );
        exit ( EXIT_FAILURE );
}
