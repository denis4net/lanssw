#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

int signal_handler(int code);

int main(int argc, char** argv)
{
      signal(SIGPIPE, signal_handler);
  
      if( argc != 3 )
      {
	fprintf(stderr, "use: %s <ip> <port> <tcp_mss_size>\n", argv[0]);
	return 10;
      }
            
      const unsigned short int port = atoi( argv[2] );
      const in_addr_t ip = inet_addr( argv[1] );
      const unsigned int mss = argv[3];

      struct sockaddr_in sockaddr;
      struct sockaddr_in client_addr;
      
      memset( &sockaddr, 0x0, sizeof(sockaddr) );
      sockaddr.sin_family = PF_INET;
      sockaddr.sin_port = htons( port ); 
      sockaddr.sin_addr.s_addr = ip;
      
      int sockfd = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
      
      if( bind( sockfd, &sockaddr, sizeof( sockaddr ) ) )
      {
	perror("can't bind socket");
	return 1;
      }
      
      if( listen( sockfd, 1 ) )
      {
	perror("can't start listening");
	return 2;
      }
      
      int clientfd;
      printf("Start listening at %s:%s\n", argv[1], argv[2] );
      
      char buffer[256];
      int size = sizeof(client_addr);
      
      if( mss > 0 && setsockopt( sockfd, IPPROTO_TCP, TCP_MAXSEG, &mss, sizeof mss ) != 0 ) {
            fprintf(stderr, "Can't set TCP MSS\n");
            exit(1);
      }

      while( ( clientfd = accept( sockfd, &client_addr, &size ) ) != -1 )
      {
	printf("Connection opened with %s\n", inet_ntoa(client_addr.sin_addr));
	while(1)
	{
	  int count = recv( clientfd, buffer, sizeof(buffer), 0x0);
	  printf("\tReceived %d bytes\n", count);
	  
        if( count == -1 || count == 0 )
	    break;

	  int code = send( clientfd, buffer, count, 0x0);
	}
	
	close(clientfd);
	printf("Connection closed\n");
      }
      
      perror("socket accept error");
      
      return 0;
}

int signal_handler(int code)
{
      fprintf(stderr, "pipe closed"); 
}