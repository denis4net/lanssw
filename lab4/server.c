#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include "common.h"

int signal_handler ( int code );
extern int tcp_loop ( const char*, const char* );
extern int udp_loop (const char*, const char* );

int common_readed=0;
static int sockfd;
/* parse options */
char opt_addr[17];
char opt_port[6];
static int opt_via_tcp = 1;


void help()
{
        error ( stderr, "use: server -a <listen_ip> -p <port> [-t] [-u]\n" );
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
                        error ( stderr, "Incorrect argument %c\n", optopt );
                        help();
                        exit ( EXIT_FAILURE );
                        break;
                }
        }
}

int main ( int argc, char** argv )
{
        signal ( SIGURG, signal_handler );
        signal ( SIGPIPE, signal_handler );
        signal ( SIGCHLD, signal_handler );

        parse_options ( argc, argv );

        if ( sockfd == -1 )  {
                perror ( "Can't init socket" );
                return 2;
        }

        pid_t pid = fork();
	if(pid==0)
		tcp_loop (opt_addr, opt_port );
	
        udp_loop (opt_addr, opt_port );
	int status;
	wait(&status);
        return 0;
}

int signal_handler ( int code )
{
        uint8_t buf;
        int status;
        int pid;

        switch ( code ) {
        case SIGPIPE:
                error ( stderr, "%d:Pipe broken. Check your network connection", getpid() );
                break;
        case SIGURG:
                recv ( sockfd, &buf, sizeof ( buf ), MSG_OOB );
                debug ( "Urgent data received\n" );
                debug ( "%10.2lf KB received\n",  common_readed/1024.0 );
                break;
        case SIGCHLD:
                pid = wait ( &status );
                debug ( "Chid process %d terminated with exit code %d\n", pid, status );
                break;
        }

        return 0;
}
