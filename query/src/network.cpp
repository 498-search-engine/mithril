#include "network.h"

#include <cstdint>
#include <iostream>

int make_server_sockaddr(struct sockaddr_in* addr, int port) {
    // Step (1): specify socket family.
    // This is an internet socket.
    addr->sin_family = AF_INET;

    // Step (2): specify socket address (hostname).
    // The socket will be a server, so it will only be listening.
    // Let the OS map it to the correct address.
    addr->sin_addr.s_addr = INADDR_ANY;

    // Step (3): Set the port value.
    // If port is 0, the OS will choose the port for us.
    // Use htons to convert from local byte order to network byte order.
    addr->sin_port = htons(uint16_t(port));

    return 0;
}

int get_port_number(int sockfd) {
    struct sockaddr_in addr;
    socklen_t length = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr*)&addr, &length) == -1) {
        // perror("Error getting port of socket");
        return -1;
    }
    // Use ntohs to convert from network byte order to host byte order.
    return ntohs(addr.sin_port);
}


int create_server_sockfd(int port, int queue_size) {

    // (1) Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sockfd == -1) {
        // perror("Error opening stream socket");
        return -1;
    }

    // (2) Set the "reuse port" socket option
    int yesval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1) {
        // perror("Error setting socket options");
        return -1;
    }

    // (3) Create a sockaddr_in struct for the proper port and bind() to it.
    struct sockaddr_in addr;
    if (make_server_sockaddr(&addr, port) == -1) {
        return -1;
    }

    // (3b) Bind to the port.
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        // perror("Error binding stream socket");
        return -1;
    }

    // (3c) Detect which port was chosen.
    port = get_port_number(sockfd);
    // printf("Server listening on port %d...\n", port);

    // (4) Begin listening for incoming connections.
    if (listen(sockfd, queue_size) == -1) {
        // perror("Error listening");
        return -1;
    }

    return sockfd;
}

// ! Add error checking
// TODO: Potential modification is creating a sockfd for a speicifc ip address
// TODO: Hostname is the ip address need to get a fucntion to do this right
int create_client_sockfd(const char* hostname, int port) {
    int sockfd;
    struct addrinfo hints, *res, *p;
    char port_str[6];  // enough to hold "65535\0"

    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;        // IPv4 (use AF_UNSPEC to allow IPv6 too)
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

    int status = getaddrinfo(hostname, port_str, &hints, &res);
    if (status != 0) {
        std::cerr << "failed to getaddrinfo socket: " << gai_strerror(status) << std::endl;
        return -1;
    }

    // Loop through all the results and connect to the first we can
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            std::cerr << "failed to create socket" << std::endl;
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;  // if we get here, we must have connected successfully
    }

    if (p == NULL) {
        std::cerr << "failed to connect to " << hostname << std::endl;
        return -1;
    }

    freeaddrinfo(res);  // all done with this structure

    return sockfd;
}