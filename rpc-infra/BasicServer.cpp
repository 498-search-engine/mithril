#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "rpc_handler.h"

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);         // port 8080
    addr.sin_addr.s_addr = INADDR_ANY;   // listen on all interfaces

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port 8080...\n";

    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
        perror("accept");
        return 1;
    }

    auto data = RPCHandler::Read(client_fd);
    std::cout << "Received:\n";
    for (const auto [num, str] : data) {
        std::cout << "  [" << num << "] " << str << "\n";
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
