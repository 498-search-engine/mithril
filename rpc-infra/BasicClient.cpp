#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "rpc_handler.h"  

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::vector<std::pair<uint32_t, uint32_t>> data = {
        {1, 2},
        {2, 3},
        {42, 45}
    };

    RPCHandler::Send(sockfd, data);
    std::cout << "Data sent!\n";

    close(sockfd);
    return 0;
}
