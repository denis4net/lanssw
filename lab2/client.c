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

int main(int argc, char** argv)
{  
  if( argc != 5 )
  {
    fprintf(stderr, "use: client <server_ip> <port> <file_to_send> <destination_file_name>\n");
    return 2;
  }
  /* Get file name */
  char* fname = strdup(argv[4]);
  
  /* Init addr struct */
  in_addr_t ip = inet_addr( argv[1] );
  unsigned short int port = atoi( argv[2] );
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  
  /* open file for read */
  int fd = open(argv[3], O_RDONLY);
  assert(fd != -1);
  
  int sockfd = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
  if( sockfd == -1 )
  {
      perror("Can't create socket");
      return 3;
  }
  
  if( connect(sockfd, &server_addr, sizeof(server_addr)) == -1 )
  {
    perror("Can't connect to server");
    return 4;
  }

  /* send file name size and file name*/
  {
    uint32_t len = strlen(fname);
    send(sockfd, &len, sizeof(len), 0x0);
    send(sockfd, fname, strlen(fname), 0x0);
  }
  /* send file content*/
  {
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
    size_t sended;
    int32_t status;
    
    uint8_t buf[BUFSIZE];
    
    while( (readed = read(fd, buf, BUFSIZE)) != 0 && readed != -1 )
    {
	sended=0;
	do 
	{
	    status = send(sockfd, buf+sended, readed-sended, 0x0);
	    if(status == -1 )
	    {
	      perror("Can't send data to server");
	      break;
	    }
	    readed += status;
	} while(sended != readed);
	
	/*connection breaked*/
	if(status == -1)
	  break;
	else
	  printf("\033[0JSended %3u\%  to server", readed/send_size);
	
    }
    
    if( readed == -1 )
    {
      perror("Can't read file");
    }
  }  
  close(fd);
  
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);  
  return 0;
}

