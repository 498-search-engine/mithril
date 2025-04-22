#include <vector>
#include <utility>
#include <string>
#include <sys/socket.h>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include "QueryManager.h"

struct RPCHandler {
    using QueryResults = QueryManager::QueryResult;
    public:
    static std::string ReadQuery(int client_fd){
        uint32_t query_length = 0;

        // Step 1: Receive query length (4 bytes)
        ssize_t bytes_read = recv(client_fd, &query_length, sizeof(query_length), MSG_WAITALL);
        if (bytes_read != sizeof(query_length)) {
            spdlog::error("Failed to read query length, received {} bytes", bytes_read);
            uint32_t result_count = 0;
            send(client_fd, &result_count, sizeof(uint32_t), 0);
            return "";
        }

        // Convert from network byte order (if needed)
        // query_length = ntohl(query_length); // Uncomment if the client used htonl()

        // Step 2: Receive query string
        std::string query(query_length, '\0');
        bytes_read = recv(client_fd, &query[0], query_length, MSG_WAITALL);
        if (bytes_read != query_length) {
            spdlog::error("Failed to read query, expected {} bytes but got {}", query_length, bytes_read);
            uint32_t result_count = 0;
            send(client_fd, &result_count, sizeof(uint32_t), 0);
            return query;
        }
        
        spdlog::info("Received binary query: '{}'", query);
        return query;
    }

    static void SendResults(int sockfd, const QueryResults& data){
        std::string header = std::to_string(data.size()) + "\r\n\r\n";
        sendAll(sockfd, header.c_str(), header.size());
    
        for (const auto& tup : data) {
            //send the uints
            uint32_t send_a = htonl(std::get<0>(tup));
            uint32_t send_b = htonl(std::get<1>(tup));
            sendAll(sockfd, &send_a, sizeof(send_a));
            sendAll(sockfd, &send_b, sizeof(send_b));

            //send the string
            auto url = std::get<2>(tup);
            sendAll(sockfd, url.c_str(), url.size()+1);

            //send the title -> vector<string>
            auto title = std::get<3>(tup);
            std::string num_results = std::to_string(title.size()) + "\r\n\r\n";
            sendAll(sockfd, header.c_str(), header.size());
            for (const auto& word : title){
                sendAll(sockfd, word.c_str(), word.size()+1);
            }
        }
    }

    static QueryResults ReadResults(int sockfd){
        //blocking until read can happen
        QueryResults result;
    
        auto recv_until_delim = [&](const std::string& delim) {
            std::string buffer;
            char c;
            while (true) {
                ssize_t n = recv(sockfd, &c, 1, 0);
                if (n <= 0) throw std::runtime_error("Failed to receive header");
                buffer += c;
                if (buffer.size() >= delim.size() &&
                    buffer.substr(buffer.size() - delim.size()) == delim) {
                    break;
                }
            }
            return buffer;
        };
    
        std::string header = recv_until_delim("\r\n\r\n");
        size_t pos = header.find("\r\n\r\n");
        if (pos == std::string::npos) throw std::runtime_error("Invalid header format");
        uint32_t num_entries = std::stoi(header.substr(0, pos));
    
        for (size_t i = 0; i < num_entries; ++i) {
            uint32_t net_a, net_b;
            recvAll(sockfd, &net_a, sizeof(net_a));
            recvAll(sockfd, &net_b, sizeof(net_b));
    
            uint32_t a = ntohl(net_a);
            uint32_t b = ntohl(net_b);

            std::string url = recv_until_delim("\0");

            std::string title_len = recv_until_delim("\r\n\r\n");
            size_t pos_entry = header.find("\r\n\r\n");
            if (pos_entry == std::string::npos) throw std::runtime_error("Invalid title format");
            uint32_t title_entries = std::stoi(title_len.substr(0, pos));

            std::vector<std::string> title;
            for (size_t i = 0; i < title_entries; ++i){
                std::string word = recv_until_delim("\0");
                title.push_back(word);
            }
    
            result.emplace_back(a, b, url, title);
        }
    
        return result;
    }

    private:
    static void sendAll(int sockfd, const void* buf, size_t len) {
        const char* ptr = static_cast<const char*>(buf);
        size_t total_sent = 0;
        //std::cout << "sending " << *ptr << std::endl;
        while (total_sent < len) {
            ssize_t sent = send(sockfd, ptr + total_sent, len - total_sent, 0);
            if (sent <= 0) throw std::runtime_error("Failed to send data");
            total_sent += sent;
        }
    }

    static void recvAll(int sockfd, void* buf, size_t len) {
        char* ptr = static_cast<char*>(buf);
        size_t total_recv = 0;
        while (total_recv < len) {
            ssize_t recvd = recv(sockfd, ptr + total_recv, len - total_recv, 0);
            if (recvd <= 0) throw std::runtime_error("Failed to receive data");
            total_recv += recvd;
        }
    }
};