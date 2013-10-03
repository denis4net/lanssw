#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

#include <libgen.h>
#include <string.h>

#define BUFSIZE 1024

int main(int argc, char** argv)
{  
  if( argc != 4 )
  {
    fprintf(stderr, "use: client <server_ip> <port> <file_to_send>\n");
    return 2;
  }
  /* Get file name */
  char* fname = strdup(argv[3]);
  fname = basename(fname);
  
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
  
  if( connect(sockfd, &server_addr, sizeof(server_addr)) == -1 ){
    perror("Can't connect to server");
    return 4;
  }
  
  /* send file name size and file name*/
  {
    uint32_t len = strlen(fname);
    send(sockfd, &len, sizeof(len));
    send(sockfd, fname, strlen(fname) );
  }
  /* send file content*/
  {
    size_t readed=0;
    uint8_t buf[BUFSIZE];
    while( (readed = read(fd, buf, BUFSIZE)) != 0 || readed != -1 )
    {
	  send(sockfd, buf, readed, 0);
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

