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

#define BUFSIZE 1024
#define RECV_STATUS_CYCLES 1024

int signal_handler(int code);

inline off_t file_size(int fd)
{
  struct stat _stat;
  fstat(fd, &_stat);
  return _stat.st_size;
}

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
      
      int client_sockfd;
      printf("Start listening at %s:%s\n", argv[1], argv[2] );
      
      char buffer[BUFSIZE];
      int size = sizeof(client_addr);
      
      while( ( client_sockfd = accept( sockfd, &client_addr, &size ) ) != -1 )
      {
	printf("Connection opened with %s\n", inet_ntoa(client_addr.sin_addr));
	uint32_t flen, status;
	
	status  = recv(client_sockfd, &flen, sizeof(flen), MSG_WAITALL);
	if(status == -1)
	{
	  perror("Can't receive file name size");
	  return 5;
	}
	
	char fname[flen+1];
	memset(fname, 0x0, sizeof(fname));
	status = recv(client_sockfd, fname, flen, MSG_WAITALL);
	assert(status != -1 );
	
	int fd = open(fname, O_WRONLY| O_APPEND | O_CREAT, 0755);
	assert(fd!=-1);
	
	
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
	while(common_readed != data_size)
	{
	  status = recv( client_sockfd, buffer, sizeof(buffer), 0x0);
	  common_readed +=  status;
	  
	 if( display_status == RECV_STATUS_CYCLES)
	 {
	   printf("\033[0J%6d bytes received",  common_readed);
	    display_status=0;
	 }
	 else
	   display_status++;
	 
	 if(  status == -1 ||  status == 0 ) 
	 {
	    perror("Client disconected");
	    break;
	  }
	  if( write(fd, buffer,  status) == -1 )
	  {
	    perror("Can't write to file");
	    break;
	  }
	}
	if( common_readed == data_size )
	  printf("\nAll data received\n");
	
	close(fd);
	close(client_sockfd);
	printf("Connection closed\n\n");
      }
      perror("Socket accept error");
      
      return 0;
}

int signal_handler(int code)
{
 fprintf(stderr, "Pipe broken"); 
}