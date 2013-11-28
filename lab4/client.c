#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <libgen.h>
#include <string.h>
#include <signal.h>
#include "common.h"

int sockfd;
static char opt_addr[17];
static char opt_port[6];
static int opt_via_tcp = 1;
static char opt_local_fname[256];
static char opt_dest_fname[256];

int signal_handler ( int code )
{
        if ( code == SIGUSR1 ) {
                uint8_t d = 0x1;
                int status = send ( sockfd, &d, sizeof ( d ), MSG_OOB );
                printf ( "\nSIGUSR1 catched. Urgent data sended\n" );
        }
}

int tcp_worker ( int sockfd, int fd )
{
        /* send file name size and file name*/
        {
                uint32_t len = strlen ( opt_dest_fname );
                send_uint32 ( sockfd,  len );
                send ( sockfd, opt_dest_fname, strlen ( opt_dest_fname ), 0x0 );
        }
        /* send file content*/
        {
                int32_t status;
                recv_uint32 ( sockfd, &status);
                if ( status != 0 ) {
                        fprintf ( stderr, "Error on server side: %s\n\n", strerror ( status ) );
                        return 4;
                }
                /*recv file offset and seek in file */
                uint32_t offset;
                recv_uint32 ( sockfd, &offset);
                printf ( "Seeking file to %u\n", offset );
                lseek ( fd, ( off_t ) offset, SEEK_SET );
                /*send data size for receiving */
                off_t fsize = file_size ( fd );
                uint32_t send_size = fsize-offset;

                if ( offset == fsize )
                        printf ( "File exist on server" );
                else if ( offset !=0 )
                        printf ( "Redownloading. Sending %u bytes data size to server\n", send_size );
                else if ( send_size != 0 )
                        printf ( "Sending %u bytes data size to server\n", send_size );


                send_uint32 ( sockfd, send_size);
                /* start reciving*/
                size_t readed=0;
                size_t sended=0;

                uint8_t buf[BUFSIZE];
                while ( ( readed = read ( fd, buf, BUFSIZE ) ) != 0 && readed != -1 && send_size!=0 ) {
                        status = send ( sockfd, buf, readed, 0x0 );
                        if ( status == -1 ) {
                                perror ( "Can't send data to server" );
                                break;
                        } else {
                                sended+=status;
                                if ( ! ( sended % 1024 ) )
                                        printf ( "\033[0G%3.2lf sended", ( double ) ( ( sended+offset ) *100.0 ) /fsize );
                        }
                }

                if ( readed == -1 )
                        perror ( "Can't read file" );

                /* recv status status */
                recv_uint32 ( sockfd, &status);
                if ( status != 0 )
                        fprintf ( stderr, "Error on server side: %s\n\n", strerror ( status ) );
        }
        return 0;
}

int udp_worker ( int sockfd, int fd )
{

        return 0;
}

void help()
{
        fprintf ( stderr, "use: client -a <listen_ip> -p <port> [-t] [-u]\n" );
}

void parse_options ( int argc, char** argv )
{
        int opt;
        while ( ( opt = getopt ( argc, argv, "a:p:uth" ) ) != -1 ) {
                switch ( opt ) {
                case 'a':
                        strcpy ( opt_addr, optarg );
                        break;

                case 'p':
                        strcpy ( opt_port, optarg );
                        break;

                case 'u':
                        opt_via_tcp = 0;
                        break;

                case 't':
                        opt_via_tcp = 1;
                        break;

                case 'h':
                        help();
                        exit ( EXIT_SUCCESS );
                        break;

                case '?':
                        fprintf ( stderr, "Incorrect argument %c\n", optopt );
                        help();
                        exit ( EXIT_FAILURE );
                        break;
                }
        }
}

int main ( int argc, char** argv )
{
        signal ( SIGUSR1, signal_handler );
        parse_options ( argc, argv );
        printf ( "Client pid %u\n", getpid() );
        /* Get file name */
        int fd = open ( opt_local_fname, O_RDONLY );
        assert ( fd != -1 );

        sockfd = ( opt_via_tcp ) ? tcpv4_bind ( opt_addr, opt_port ) : udpv4_bind ( opt_addr, opt_port );
        if ( opt_via_tcp ) {
                int status = connect ( sokfd, NULL, NULL );
                if ( status== -1 ) {
                        perror ( "Can't connect to host" );
                        return 3;
                }

                tcp_worker ( sockfd, fd );
        } else
                udp_worker ( sockfd, fd );

        close ( fd );
        shutdown ( sockfd, SHUT_RDWR );
        close ( sockfd );
        return 0;
}

