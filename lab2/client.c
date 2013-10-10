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

#define BUFSIZE 1024

inline off_t file_size(int fd)
{
  struct stat _stat;
  fstat(fd, &_stat);
  return _stat.st_size;
}

int tcpv4_connect(char* ipv4, char *tcp_port)
{
    /* Init addr struct */
  in_addr_t ip = inet_addr( ipv4  );
  unsigned short int port = atoi( tcp_port );
  
  struct sockaddr_in server_addr;
  memset((void*)&server_addr, 0x0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
    
  int sockfd = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
  if( sockfd == -1 )
    return -1;
  
  if( connect(sockfd, &server_addr, sizeof(server_addr)) == -1 )
    return -1;

  return sockfd;
}

int main(int argc, char** argv)
{  
  if( argc != 5 )
  {
    fprintf(stderr, "use: client <server_ip> <port> <file_to_send> <destination_file_name>\n");
    return 2;
  }
  /* Get file name */
  char* fname = strdup(argv[4]);
  int fd = open(argv[3], O_RDONLY);
  assert(fd != -1);

  int sockfd = tcpv4_connect(argv[1], argv[2]);
  if(sockfd == -1 )
  {
    perror("Can't connect to host");
    return 3;
  }
  /* send file name size and file name*/
  {
    uint32_t len = strlen(fname);
    send(sockfd, &len, sizeof(len), 0x0);
    send(sockfd, fname, strlen(fname), 0x0);
  }
  /* send file content*/
  {
    int32_t status;
     recv(sockfd, &status, sizeof(status), MSG_WAITALL);
    if(status != 0 )
    {
      fprintf(stderr, "Error on server side: %s\n\n", strerror(status));
      return 4;
    }
    /*recv file offset and seek in file */
    uint32_t offset;
    recv(sockfd, &offset, sizeof(offset), MSG_WAITALL);
    printf("Seeking file to %u\n", offset);
    lseek(fd, (off_t) offset, SEEK_SET);
    /*send data size for receiving */
    off_t fsize = file_size(fd); 
    uint32_t send_size = fsize-offset;
    printf("Sending %u bytes data size to server\n", send_size);
    send(sockfd, &send_size, sizeof(send_size), 0x0);
    /* start reciving*/
    size_t readed=0;
    size_t sended=0;
    
    uint8_t buf[BUFSIZE];
    while( (readed = read(fd, buf, BUFSIZE)) != 0 && readed != -1 )
    {
	status = send(sockfd, buf, readed, 0x0);
	if(status == -1 )
	{
	  perror("Can't send data to server");
	  break;
	}
	else
	{
	  sended+=status;
	  if( !  (sended % 1024 ) )
	    printf("\033[0G%3.2lf sended", (double) ((sended+offset)*100.0)/fsize );
	}
    }
    
    if( readed == -1 )
      perror("Can't read file");
    
    /* recv status status */
    recv(sockfd, &status, sizeof(status), MSG_WAITALL);
    if(status != 0 )
      fprintf(stderr, "Error on server side: %s\n\n", strerror(status));
  }  
  close(fd);
  
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);  
  return 0;
}

