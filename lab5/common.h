#ifndef GENERIC_NET
#define GENERIC_NET

#define UDP_TRIES 10
#define UDP_SYN (uint16_t)  0xFFFD
#define UDP_ACK (uint8_t) 0xFFFC

#define BUFSIZE 1024
#define RECV_STATUS_CYCLES 1024

#define debug(format, ...)  printf( "%u:" format, getpid(), ##__VA_ARGS__ )
#define error(x, ...) fprintf(stderr, "%u: Error: ", getpid()); fprintf(x, ##__VA_ARGS__ )

int udpv4_bind(const char* ipv4, const char* tcp_port);
int tcpv4_bind(const char* ipv4, const char* tcp_port);
uint32_t file_size(int fd);

const char* extract_peer_addr(int sockfd);
const uint16_t extract_peer_port(int sockfd);
const uint16_t extract_bind_port(int sockfd) ;

int tcp_send_uint32(int sockfd, uint32_t data);
int tcp_recv_uint32(int sockfd, uint32_t* data);

int udp_send ( int sockfd, uint8_t* data, size_t size, struct sockaddr_in* d_sockaddr );
int udp_recv ( int sockfd, uint8_t* data, size_t size, struct sockaddr_in* sockaddr );
int udp_send_uint32 ( int sockfd, uint32_t data, struct sockaddr_in* d_sockaddr );
int udp_recv_uint32 ( int sockfd, uint32_t *data, struct sockaddr_in* d_sockaddr );

const char* addr_to_ip_string ( const struct sockaddr_in addr);

#endif