#ifndef GENERIC_NET
#define GENERIC_NET

#define BUFSIZE 1024
#define RECV_STATUS_CYCLES 1024

int udpv4_bind(const char* ipv4, const char* tcp_port);
int tcpv4_bind(const char* ipv4, const char* tcp_port);
inline off_t file_size(int fd);

const char* get_peer_addr(int sockfd);
const uint16_t get_peer_port(int sockfd);

int send_uint32(int sockfd, uint32_t data);
int recv_uint32(int sockfd, uint32_t* data);

#endif