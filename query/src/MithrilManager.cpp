#include <mutex>
#include "QueryManager.cpp"
#include "network.h"
#include "rpc_handler.h"
#include <stdexcept>
#include <iostream>
#include <memory>

const int PORT = 8080;

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

    MithrilManager(int argc, char** argv){

        std::vector<std::string> indexPaths;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
    
            if (arg == "--index" && i + 1 < argc) {
                indexPaths.push_back(argv[++i]);
            } else {
                std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
                throw std::runtime_error("Failed to make MithrilManager");
            }


            server_fd = create_server_sockfd(PORT, 10);

            if (server_fd == -1) throw std::runtime_error("Failed to make MithrilManager");

            std::cout << "Server running on localhost:" << PORT << std::endl;
            //std::cout << "Using index path: " << indexPath << std::endl;

            manager = make_unique<QueryManager>(indexPaths);

            std::cout << "Successfully created MithrilManager" << std::endl;
        }
    }

    ~MithrilManager(){
        close(server_fd);
    }
};

std::mutex MithrilManager::manager_mutex;
std::unique_ptr<QueryManager> MithrilManager::manager;

int main(int argc, char** argv){
    try {
        MithrilManager mm(argc, argv);
        mm.Listen();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    
}
