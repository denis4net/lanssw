#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <signal.h>

#define BUFSIZE 1024

int signal_handler(int code);

int main(int argc, char** argv)
{
      signal(SIGPIPE, signal_handler);
  
      if( argc != 3 )
      {
	fprintf(stderr, "use: server <listen_ip> <port>\n");
	return 10;
      }
            
      unsigned short int port = atoi( argv[2] );
      in_addr_t ip = inet_addr( argv[1] );
      
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
      
      char buffer[BUFSIZE];
      int size = sizeof(client_addr);
      
      while( ( clientfd = accept( sockfd, &client_addr, &size ) ) != -1 )
      {
	printf("Connection opened with %s\n", inet_ntoa(client_addr.sin_addr));
	uint32_t flen;
	int status;
	
	status  = recv(clientfd, &flen, sizeof(flen), MSG_WAITALL);
	if(status == -1)
	{
	  perror("Can't receive file name size");
	  return 5;
	}
	
	char fname[flen+1];
	memset(fname, 0x0, sizeof(fname));
	status = recv(clientfd, fname, flen, MSG_WAITALL);
	assert(status != -1 );
	
	printf("File name: %s\n", fname);
	
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