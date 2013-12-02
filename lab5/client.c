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
#include <sys/time.h>
#include <getopt.h>

int g_sockfd;
static char opt_addr[17];
static char opt_port[6];
static int opt_via_tcp = 1;
static char opt_local_fname[256];
static char opt_dest_fname[256];

int signal_handler ( int code )
{
        if ( code == SIGUSR1 ) {
                uint8_t d = 0x1;
                int status = send ( g_sockfd, &d, sizeof ( d ), MSG_OOB );
                debug ( "\nSIGUSR1 catched. Urgent data sended\n" );
        }
}

int udp_socket()
{
        struct sockaddr_in sockaddr;
        socklen_t socklen = sizeof ( sockaddr );

        memset ( &sockaddr, 0x0, sizeof ( sockaddr ) );
        sockaddr.sin_family=AF_INET;
        sockaddr.sin_addr.s_addr = htonl ( INADDR_ANY );
        sockaddr.sin_port = 0;

        int sockfd = socket ( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
        if ( sockfd < 0 )
                return -1;

        if ( bind ( sockfd, ( struct sockaddr* ) &sockaddr, socklen ) <0 ) {
                perror ( "bind error" );
                return -2;
        }

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        if ( setsockopt ( sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof ( tv ) ) < 0 ) {
                perror ( "setsockopt(SO_RCVTIMEO) error" );
                return -3;
        }

        if ( setsockopt ( sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof ( tv ) ) ) {
                perror ( "setsockopt(SO_SNDTIMEO) error" );
                return -4;
        }

        return sockfd;
}

struct sockaddr_in mk_sockaddr ( const char* ipv4, const char* tcp_port )
{
        unsigned short int port = atoi ( tcp_port );
        in_addr_t ip = inet_addr ( ipv4 );

        struct sockaddr_in sockaddr;
        memset ( &sockaddr, 0x0, sizeof ( sockaddr ) );
        sockaddr.sin_family = PF_INET;
        sockaddr.sin_port = htons ( port );
        sockaddr.sin_addr.s_addr = ip;

        return sockaddr;
}

int tcpv4_connect ( const char* ipv4_addr, const char* tcp_port )
{
        unsigned short int port = atoi ( tcp_port );
        in_addr_t ip = inet_addr ( ipv4_addr );

        struct sockaddr_in sockaddr;
        memset ( &sockaddr, 0x0, sizeof ( sockaddr ) );
        sockaddr.sin_family = PF_INET;
        sockaddr.sin_port = htons ( port );
        sockaddr.sin_addr.s_addr = ip;

        int sockfd = socket ( PF_INET, SOCK_STREAM, IPPROTO_TCP );
        int opt=1;
        setsockopt ( sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof ( opt ) );

        if ( connect ( sockfd, ( struct sockaddr* ) &sockaddr, sizeof ( sockaddr ) ) )
                return -1;

        return sockfd;
}

int tcp_worker ( int sockfd, int fd )
{
        /* send file name size and file name*/
        {
                uint32_t len = strlen ( opt_dest_fname );
                tcp_send_uint32 ( sockfd,  len );
                send ( sockfd, opt_dest_fname, strlen ( opt_dest_fname ), 0x0 );
        }
        /* send file content*/
        {
                uint32_t status;
                tcp_recv_uint32 ( sockfd, &status );
                if ( status != 0 ) {
                        error ( stderr, "Error on server side: %s (%u)\n\n", strerror ( status ), status );
                        return 4;
                }
                /*recv file offset and seek in file */
                uint32_t offset;
                tcp_recv_uint32 ( sockfd, &offset );

                if ( offset == file_size ( fd ) ) {
                        debug ( "File already uploaded\n" );
                        exit ( EXIT_SUCCESS );
                }
                debug ( "Seeking file to %ukB, file size %ukB\n", offset/1024, file_size ( fd ) /1024 );
                lseek ( fd, ( off_t ) offset, SEEK_SET );
                /*send data size for receiving */
                off_t fsize = file_size ( fd );
                uint32_t send_size = fsize-offset;

                if ( offset == fsize )
                        debug ( "File exist on server" );
                else if ( offset !=0 )
                        debug ( "Redownloading. Sending %u bytes data size to server\n", send_size );
                else if ( send_size != 0 )
                        debug ( "Sending %u bytes data size to server\n", send_size );


                tcp_send_uint32 ( sockfd, send_size );
                /* start reciving*/
                size_t readed=0, sended=0;

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
                tcp_recv_uint32 ( sockfd, &status );
                debug("Writing to file on server side status: %s\n\n", strerror ( status ) );
        }
        return 0;
}

int udp_worker ( int sockfd, int fd )
{
        /**
         * SYN transaction
         */
        /***********************************************************************************************************************************/
        struct sockaddr_in server_addr = mk_sockaddr ( opt_addr, opt_port );
        socklen_t server_addr_len = sizeof ( server_addr );

        uint16_t data;
        debug ( "Send SYN packet to UDP server %s:%s\n", opt_addr, opt_port );
        data = htons ( UDP_SYN );

        if ( sendto ( sockfd, &data, sizeof ( data ), 0x0, &server_addr, server_addr_len ) < 0 ) {
                perror ( "Can't send udp SYN" );
                return -1;
        }

        if ( recvfrom ( sockfd, &data, sizeof ( data ), MSG_WAITALL, &server_addr,  &server_addr_len ) < 0 ) {
                perror ( "Can't receive ACK from udp server" );
                return -2;
        }
        /***********************************************************************************************************************************/
        server_addr.sin_port = data;
        debug ( "ACK received from udp server. Otcet blusting server %s:%hu port\n", addr_to_ip_string ( server_addr ) ,ntohs ( server_addr.sin_port ) );
        /* Start data transfering */
        /* send file name size and file name*/
        {
                uint32_t len = strlen ( opt_dest_fname );

                if ( udp_send_uint32 ( sockfd,  len, &server_addr ) < 0 )
                        perror ( "Can't send file name size" );

                if ( udp_send ( sockfd, opt_dest_fname, strlen ( opt_dest_fname ),  &server_addr ) < 0 )
                        perror ( "Can't send file name" );
        }
        /* send file content*/
        {
                uint32_t status;
                if ( udp_recv_uint32 ( sockfd, &status,  &server_addr ) < 0 )
                        perror ( "Can't open file status from server" );

                if ( status != 0 ) {
                        error ( stderr, "Error on server side: %s (%u)\n\n", strerror ( status ), status );
                        return 4;
                }
                /*recv file offset and seek in file */
                uint32_t offset;
                if ( udp_recv_uint32 ( sockfd, &offset,  &server_addr ) < 0 )
                        perror ( "Can't recv file offset" );

                if ( offset == file_size ( fd ) ) {
                        debug ( "File already uploaded\n" );
                        exit ( EXIT_SUCCESS );
                }
                lseek ( fd, ( off_t ) offset, SEEK_SET );
                /*send data size for receiving */
                off_t fsize = file_size ( fd );
                uint32_t send_size = fsize-offset;

                if ( offset == fsize )
                        debug ( "File exist on server" );
                else if ( offset !=0 )
                        debug ( "Redownloading. Sending %u bytes data size to server\n", send_size );
                else if ( send_size != 0 )
                        debug ( "Sending %u bytes data size to server\n", send_size );


                udp_send_uint32 ( sockfd, send_size,  &server_addr );
                /* start reciving*/
                size_t readed=0;
                size_t sended=0;

                uint8_t buf[BUFSIZE];
                while ( ( readed = read ( fd, buf, BUFSIZE ) ) != 0 && readed != -1 && send_size!=0 ) {
                        status = udp_send ( sockfd, buf, readed,  &server_addr );
                        if ( status == -1 ) {
                                perror ( "Can't send data to server" );
                                break;
                        } else {
                                sended+=status;
                                if ( ! ( sended % 1024 ) )
                                        printf ( "\033[0G%3.2lf  sended", ( double ) ( ( sended+offset ) *100.0 ) /fsize );
                        }
                }

                if ( readed == -1 )
                        perror ( "Can't read file" );

                /* recv status status */
                udp_recv_uint32 ( sockfd, &status,  &server_addr );
                debug("Writing to file on server side status: %s\n\n", strerror ( status ) );
        }

        return 0;
}

void help()
{
        error ( stderr, "use: client -a <listen_ip> -p <port> -s <file_name> -d <file_name>  [-t] [-u]\n" );
}

void parse_options ( int argc, char** argv )
{
        int opt;
        memset ( opt_dest_fname, 0x0, sizeof ( opt_dest_fname ) );
        memset ( opt_local_fname, 0x0, sizeof ( opt_local_fname ) );

        while ( ( opt = getopt ( argc, argv, "a:p:s:d:uth" ) ) != -1 ) {
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

                case 's':
                        strcpy ( opt_local_fname, optarg );

                        break;

                case 'd':
                        strcpy ( opt_dest_fname, optarg );

                        break;

                case '?':
                        error ( stderr, "Incorrect argument %c\n", opt );
                        help();
                        exit ( EXIT_FAILURE );
                        break;
                }
        }

        if ( strlen ( opt_dest_fname ) < 1 ) {
                error ( stderr, "Destination file name empty" );
                exit ( EXIT_FAILURE );
        }
        if ( strlen ( opt_local_fname ) < 1 ) {
                error ( stderr, "Destination file name empty" );
                exit ( EXIT_FAILURE );
        }
}

int main ( int argc, char** argv )
{
        signal ( SIGUSR1, signal_handler );
        parse_options ( argc, argv );
        /* Get file name */
        int fd = open ( opt_local_fname, O_RDONLY );
        assert ( fd != -1 );

        g_sockfd = ( opt_via_tcp ) ? tcpv4_connect ( opt_addr, opt_port ) : udp_socket();
        if ( g_sockfd < 0 ) {
                perror ( "Can't create socket" );
                exit ( EXIT_FAILURE );
        }

        if ( opt_via_tcp ) {
                tcp_worker ( g_sockfd, fd );
        } else
                udp_worker ( g_sockfd, fd );

        close ( fd );
        shutdown ( g_sockfd, SHUT_RDWR );
        close ( g_sockfd );
        return 0;
}

