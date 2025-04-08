#include "QueryChildren.h"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include "network.h"

QueryChildren::QueryChildren(const std::string& indexPath, int port)
    : indexPath(indexPath), serverPort(port), serverSocket(-1), running(false) {
}

QueryChildren::~QueryChildren() {
    stopServer();
}

bool QueryChildren::initialize() {
    return setupSocket();
}

// bool QueryChildren::setupSocket() {
//     // Create socket
//     serverSocket = socket(AF_INET, SOCK_STREAM, 0);
//     if (serverSocket < 0) {
//         std::cerr << "Error creating socket" << std::endl;
//         return false;
//     }

//     // Setup server address structure
//     memset(&serverAddr, 0, sizeof(serverAddr));
//     serverAddr.sin_family = AF_INET;
//     serverAddr.sin_addr.s_addr = INADDR_ANY;
//     serverAddr.sin_port = htons(serverPort);

//     // Bind socket
//     if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
//         std::cerr << "Error binding socket" << std::endl;
//         return false;
//     }

//     // Listen for connections
//     if (listen(serverSocket, 5) < 0) {
//         std::cerr << "Error listening on socket" << std::endl;
//         return false;
//     }

//     return true;
// }

void QueryChildren::startServer() {
    running = true;
    int sercerSocket = create_server_sockfd(serverPort, 5);
    std::cout << "Server started on port " << serverPort << std::endl;
    acceptConnections();
}

void QueryChildren::acceptConnections() {


    while (running) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // Store client socket
        clientSockets.push_back(clientSocket);

        // TODO: Create a new thread 
        handleClient(clientSocket);
    }
}

void QueryChildren::handleClient(int clientSocket) {
    char buffer[1024] = {0};
    read(clientSocket, buffer, 1024);
    std::cout << "Received from client: " << buffer << std::endl;
    
    // Process query and send response
    const char* response = "Query processed";
    send(clientSocket, response, strlen(response), 0);
}

void QueryChildren::stopServer() {
    running = false;
    cleanupConnections();
    
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
}

void QueryChildren::cleanupConnections() {
    for (int clientSocket : clientSockets) {
        close(clientSocket);
    }
    clientSockets.clear();
} 