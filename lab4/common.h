#ifndef GENERIC_NET
#define GENERIC_NET
int udpv4_bind(const char* ipv4, const char* tcp_port);
int tcpv4_bind(const char* ipv4, const char* tcp_port);

inline off_t file_size(int fd);
int udp_send(int sockfd, void* buf, int size);
int udp_recv(int scokfd, void* buf, int size);
int tcp_send(int sockfd, void* buf, int size);
int tpc_recv(int sockfd, void* buf, int size);

#endif