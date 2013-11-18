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
#include <unistd.h>
#include <errno.h>

#define BUFSIZE 1024
#define RECV_STATUS_CYCLES 1024

int signal_handler(int code);

inline off_t file_size(int fd)
{
  struct stat _stat;
  fstat(fd, &_stat);
  return _stat.st_size;
}

int tcpv4_bind(const char* ipv4, const char* tcp_port)
{
   unsigned short int port = atoi( tcp_port );
      in_addr_t ip = inet_addr( ipv4 );

      struct sockaddr_in sockaddr;   
      memset( &sockaddr, 0x0, sizeof(sockaddr) );
      sockaddr.sin_family = PF_INET;
      sockaddr.sin_port = htons( port ); 
      sockaddr.sin_addr.s_addr = ip;

      int sockfd = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
      int opt=1;
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      if( bind( sockfd, &sockaddr, sizeof( sockaddr ) ) )
	return -1;

      return sockfd;
}

int main(int argc, char** argv)
{
      signal(SIGPIPE, signal_handler);
  
      if( argc != 3 ) {
	fprintf(stderr, "use: server <listen_ip> <port>\n");
	return 1;
      }
    
      int sockfd = tcpv4_bind(argv[1], argv[2]);
      if(sockfd == -1)  {
	perror("Can't init socket");
	return 2;
      }
      
      if( listen( sockfd, 1 ) ) {
	perror("can't start listening");
	return 2;
      }
      
      int client_sockfd;
      printf("Start listening at %s:%s\n", argv[1], argv[2] );
      
      char buffer[BUFSIZE];
      struct sockaddr_in client_addr;
      int size = sizeof(client_addr);
      
      while( ( client_sockfd = accept( sockfd, &client_addr, &size ) ) != -1 ) {
	int opt=1;
	setsockopt(client_sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)); 
	printf("Connection opened with %s\n", inet_ntoa(client_addr.sin_addr));
	uint32_t flen, status;
	
	status  = recv(client_sockfd, &flen, sizeof(flen), MSG_WAITALL);
	if(status == -1) {
	  perror("Can't receive file name size");
	  return 5;
	}
	
	char fname[flen+1];
	memset(fname, 0x0, sizeof(fname));
	status = recv(client_sockfd, fname, flen, MSG_WAITALL);
	if(status==-1) {
	  perror("Can't receive file name");
	  continue;
	}
	
	int fd = open(fname, O_WRONLY| O_APPEND | O_CREAT, 0755);
	//send file open status
	status = errno;
	send(client_sockfd, &status, sizeof(status), 0);
	
	//send file offset
	{
	  uint32_t offset = file_size(fd);
	  send(client_sockfd, &offset, sizeof(offset), 0x0);
	  printf("File name: %s, file size %u bytes\n", fname, offset);	  
	}
	
	//receiving data size
	uint32_t data_size, common_readed, display_status;
	recv(client_sockfd, &data_size, sizeof(data_size), 0x0);
	
	display_status=0;
	common_readed=0;
	while(common_readed != data_size) {
	  status = recv( client_sockfd, buffer, sizeof(buffer), 0x0);
	  common_readed +=  status;
	  
	 if( display_status == RECV_STATUS_CYCLES)  {
	   printf("\033[0G%10.2lf KB received",  common_readed/1024.0);
	    display_status=0;
	 }
	 else
	   display_status++;
	 
	 if(  status == -1 ||  status == 0 )  {
	    perror("\nClient disconected");
	    break;
	  }
	 else if( write(fd, buffer,  status) == -1 )  {
	    perror("\nCan't write to file");
	    break;
	  }
	}
	
	if( common_readed == data_size ) {
	  status=0;
	  send(client_sockfd, &status, sizeof(status), 0x0);
	  printf("\nAll data received\n");
	}
	else {
	  fprintf(stderr, "\nNot all data received from client\n");
	}
	close(fd);
	close(client_sockfd);
	printf("\nConnection closed\n\n");
      }
      
      perror("Socket accept error");
      return 0;
}

int signal_handler(int code)
{
 fprintf(stderr, "Pipe broken"); 
}