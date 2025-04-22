#include <mutex>
#include "QueryManager.cpp"
#include "network.h"
#include "rpc_handler.h"
#include <stdexcept>
#include <iostream>
#include <memory>
#include "Util.h"

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --port PORT --index INDEX_PATH [--index INDEX_PATH ...]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port PORT                Set the server port (required)" << std::endl;
    std::cout << "  --index INDEX_PATH         Set an index path (at least one required)" << std::endl;
}

struct MithrilManager {
    private:
    static std::mutex manager_mutex;
    static std::unique_ptr<QueryManager> manager;
    int server_fd;

    void Handle(int client_fd){
        connection_cleaner client(client_fd);
        //get query from socket
        try{
            std::string query = RPCHandler::ReadQuery(client.connectionfd);
            manager_mutex.lock();
            //answer the query and get back vector<pair<uint32_t, uint32_t>> of results
            auto results = manager->AnswerQuery(query);
            manager_mutex.unlock();
    
            //send the results back
            RPCHandler::SendResults(client.connectionfd, results);
        } catch (std::exception& e){
            std::cerr << e.what() << std::endl;
            return;
        }
    }

    public:

    void Listen(){
        while (true) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
    
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            std::cout << "Accepted a client connection\n";
            if (client_fd < 0) {
                std::cerr << "Failed to accept connection." << std::endl;
                close(client_fd);
                continue;
            }
            std::thread t([client_fd, this]() {
                this->Handle(client_fd);
            });
            t.detach(); // Don't join, just let it run
        }
    }

    MithrilManager(int port, const std::vector<std::string>& indexPaths){
        if (indexPaths.empty()) {
            throw std::runtime_error("At least one index path is required");
        }

        server_fd = create_server_sockfd(port, 10);

        if (server_fd == -1) throw std::runtime_error("Failed to create server socket");

        std::cout << "Server running on localhost:" << port << std::endl;
        
        for (const auto& path : indexPaths) {
            std::cout << "Using index path: " << path << std::endl;
        }

        manager = std::make_unique<QueryManager>(indexPaths);

        std::cout << "Successfully created MithrilManager" << std::endl;
    }

    ~MithrilManager(){
        close(server_fd);
    }
};

std::mutex MithrilManager::manager_mutex;
std::unique_ptr<QueryManager> MithrilManager::manager;

std::pair<int, std::vector<std::string>> parseConfFile(const std::string& conf_file){
    std::vector<std::string> result;

    std::string file_contents = ReadFile(conf_file.c_str());
    auto lines = GetLines(file_contents);

    bool port_found = false;
    uint16_t port;

    for (auto i = 0; i < lines.size(); i++){
        auto line = lines[i];

        if (line.empty()) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        auto parts = GetWords(line);

        if (not port_found) {
            port = std::stoul(std::string(parts[0]));
            port_found = true;
        } else {
            result.emplace_back(parts[0]);
        }

    }
    
    return {port, result};
}

int main(int argc, char** argv){
    try {
        int port = -1;
        std::vector<std::string> indexPaths;
        std::string conf_file;
        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
    
            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
            } else if (arg == "--index" && i + 1 < argc) {
                indexPaths.push_back(argv[++i]);
            } else if (arg == "--conf" && i + 1 < argc) {
                conf_file = argv[++i];
                std::tie(port, indexPaths) = parseConfFile(conf_file);
            } else {
                std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        }


        // Validate required arguments
        if (port == -1 || indexPaths.empty()) {
            std::cerr << "Error: --port and at least one --index are required arguments." << std::endl;
            printUsage(argv[0]);
            return 1;
        }

        MithrilManager mm(port, indexPaths);
        mm.Listen();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

