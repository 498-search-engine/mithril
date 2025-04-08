#ifndef QUERY_CHILDREN_H
#define QUERY_CHILDREN_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>

class QueryChildren {
private:
    std::string indexPath;
    int serverPort;
    int serverSocket;
    struct sockaddr_in serverAddr;
    std::vector<int> clientSockets;
    bool running;

public:
    // Constructor
    QueryChildren(const std::string& indexPath, int port);

    // Destructor
    ~QueryChildren();

    // Getters
    std::string getIndexPath() const {
        return indexPath;
    }
    
    int getPort() const {
        return serverPort;
    }

    // Server functionality
    bool initialize();
    void startServer();
    void stopServer();

private:
    // Helper methods
    bool setupSocket();
    void handleClient(int clientSocket);
    void acceptConnections();
    void cleanupConnections();
};

#endif // QUERY_CHILDREN_H
