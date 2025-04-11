#include "network.h"

#include <cstdint>

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

/**
 * Make a client sockaddr given a remote hostname and port.
 * Parameters:
 *		addr: The sockaddr to modify (this is a C-style function).
 *		hostname: The hostname of the remote host to connect to.
 *		port: The port to use to connect to the remote hostname.
 * Returns:
 *		0 on success, -1 on failure.
 * Example:
 *		struct sockaddr_in client;
 *		int err = make_client_sockaddr(&client, "141.88.27.42", 8888);
 */
int make_client_sockaddr(struct sockaddr_in* addr, const char* hostname, int port) {
    // Step (1): specify socket family.
    // This is an internet socket.
    addr->sin_family = AF_INET;

    // Step (2): specify socket address (hostname).
    // The socket will be a client, so call this unix helper function
    // to convert a hostname string to a useable `hostent` struct.
    struct hostent* host = gethostbyname(hostname);
    if (host == nullptr) {
        printf("Error: Unknown host %s\n", hostname);
        return -1;
    }
    memcpy(&(addr->sin_addr), host->h_addr, (size_t)host->h_length);

    // Step (3): Set the port value.
    // Use ntohs to convert from local byte order to network byte order.
    addr->sin_port = ntohs(uint16_t(port));

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

    // (1) Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // (2) Create a sockaddr_in to specify remote host and port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(port));
    struct hostent* hp;
    hp = gethostbyname(hostname);
    /*
     * gethostbyname returns a structure including the network address
     * of the specified host.
     */
    if (hp == (struct hostent*)0) {
        fprintf(stderr, "%s: unknown host\n", hostname);
        exit(2);
    }
    memcpy((char*)&addr.sin_addr, (char*)hp->h_addr, (size_t)hp->h_length);
    if (make_client_sockaddr(&addr, hostname, port) == -1) {
        return -1;
    }

    // (3) Connect to remote server
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connecting stream socket");
        exit(1);
    }

    return sockfd;
}

// int run_server(int port, int queue_size) {

//   // (1) Create socket
//   int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
//   if (sockfd == -1) {
//     perror("Error opening stream socket");
//     return -1;
//   }

//   // (2) Set the "reuse port" socket option
//   int yesval = 1;
//   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval,
//   sizeof(yesval))
//   ==
//       -1) {
//     perror("Error setting socket options");
//     return -1;
//   }

//   // (3) Create a sockaddr_in struct for the proper port and bind() to
//   it. struct sockaddr_in addr; if (make_server_sockaddr(&addr, port) ==
//   -1) {
//     return -1;
//   }

//   // (3b) Bind to the port.
//   if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
//     perror("Error binding stream socket");
//     return -1;
//   }

//   // (3c) Detect which port was chosen.
//   port = get_port_number(sockfd);
//   printf("Server listening on port %d...\n", port);

//   // (4) Begin listening for incoming connections.
//   if (listen(sockfd, queue_size) == -1) {
//     perror("Error listening");
//     return -1;
//   }

//   // (5) Serve incoming connections one by one forever.
//   while (1) {
//     int connectionfd = accept(sockfd, NULL, NULL);
//     if (connectionfd == -1) {
//       perror("Error accepting connection");
//       return -1;
//     }

//     if (handle_connection(connectionfd) == -1) {
//       return -1;
//     }
//   }
// }