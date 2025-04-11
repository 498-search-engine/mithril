
#include <netdb.h>       // gethostbyname(), struct hostent
#include <stdio.h>       // perror(), fprintf()
#include <stdlib.h>      // atoi()
#include <string.h>      // memcpy()
#include <unistd.h>      // stderr
#include <arpa/inet.h>   // htons(), ntohs()
#include <netinet/in.h>  // struct sockaddr_in
#include <sys/socket.h>  // getsockname()

// static const int MESSAGE_CHUNK_SIZE = 1000;

// static const char *RESPONSE_OK = "200";

// static const char *RESPONSE_ERROR = "400";

struct connection_cleaner {
    connection_cleaner(int connectionfd_in) : connectionfd(connectionfd_in) {}
    ~connection_cleaner() { close(connectionfd); }
    int connectionfd;
};

int create_client_sockfd(const char* hostname, int port);

int create_server_sockfd(int port, int queue_size);