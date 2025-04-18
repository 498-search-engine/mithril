#include <mutex>
#include "QueryManager.cpp"
#include "network.h"
#include "rpc_handler.h"

const PORT = 8080;


int main(int argc, char** argv){
    MithrilManager mm;
    try {
        mm(argc, argv);
    } catch (std::exception& e) {
        std:cerr << e.what() << std::endl;
    }

    mm.Listen();
}

struct MithrilManager {
    private:
    static std::mutex manager_mutex;
    static QueryManager manager;
    int server_fd;

    void Handle(int client_fd){
        connection_cleaner client(client_fd);
        //get query from socket
        try{
            std::string query = RPCHandler::ReadQuery(client.connectionfd);
        } catch (std::exception& e){
            std::cerr << e.what() << std::endl;
            return;
        }
        
        //answer the query and get back vector<pair<uint32_t, uint32_t>> of results
        auto results = manager.AnswerQuery(query);

        //send the results back
        RPCHandler::SendResults(results);
    }

    public:

    void Listen(int clientfd){
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
            std::thread t([client_sockfd]() {
                Handle(client_fd);
            });
            t.detach(); // Don't join, just let it run
        }
    }

    MithrilManager(int argc, char** argv){
        std::string indexPath;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
    
            if (arg == "--index" && i + 1 < argc) {
                indexPath = argv[++i];
            } else {
                std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
                printUsage(argv[0]);
                throw std::runtime_error("Failed to make MithrilManager");
            }

            if (indexPath.empty()) throw std::runtime_error("Failed to make MithrilManager");

            server_fd = create_server_sockfd(PORT, 10);

            if (server_fd == -1) throw std::runtime_error("Failed to make MithrilManager");

            std::cout << "Server running on localhost:" << PORT << std::endl;
            std::cout << "Using index path: " << indexPath << std::endl;

            std::vector<std::string> indexPaths = {indexPath};
            manager(indexPaths);

            std::cout << "Successfully created MithrilManager" << std::endl;
        }
    }

    ~MithrilManager(){
        close(server_fd);
    }
}