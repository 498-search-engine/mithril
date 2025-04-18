#include <vector>
#include <utility>
#include <string>
#include <sys/socket.h>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>

struct RPCHandler {
    public:
    static std::string ReadQuery(int client_fd){
        uint32_t query_length = 0;

        // Step 1: Receive query length (4 bytes)
        ssize_t bytes_read = recv(client_fd, &query_length, sizeof(query_length), MSG_WAITALL);
        if (bytes_read != sizeof(query_length)) {
            spdlog::error("Failed to read query length, received {} bytes", bytes_read);
            uint32_t result_count = 0;
            send(client_fd, &result_count, sizeof(uint32_t), 0);
            return;
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
            return;
        }
        
        spdlog::info("Received binary query: '{}'", query);
        return query;
    }

    static void SendResults(int sockfd, const std::vector<std::pair<uint32_t, std::string>>& data){
        std::string header = std::to_string(data.size())+"\r\n\r\n";
        sendAll(sockfd, header.c_str(), header.size());

        for (const auto& [num, str] : data) {
            //send id
            uint32_t send_num = htonl(num);
            sendAll(sockfd, &send_num, sizeof(send_num));

            //send c-str
            sendAll(sockfd, str.c_str(), str.size()+1);
        }
    }

    static std::vector<std::pair<uint32_t, std::string>> ReadResults(int sockfd){
        //blocking until read can happen
        std::vector<std::pair<uint32_t, std::string>> result;

        auto recv_until_delim = [&](const std::string& delim) {
            std::string buffer;
            char c;
            while (true) {
                std::cout << "reading\n";
                ssize_t n = recv(sockfd, &c, 1, 0);
                std::cout << "read\n";
                if (n <= 0) throw std::runtime_error("Failed to receive header");
                buffer += c;
                std::cout << "curr buffer " << buffer << std::endl;
                if (buffer.size() >= delim.size() &&
                    buffer.substr(buffer.size() - delim.size()) == delim) {
                    break;
                }
            }
            return buffer;
        };

        std::cout << "about to recv header\n";
        std::string header = recv_until_delim("\r\n\r\n");
        size_t pos = header.find("\r\n\r\n");
        if (pos == std::string::npos) throw std::runtime_error("Invalid header format");
        std::cout << "got header\n";
        uint32_t num_entries = std::stoi(header.substr(0, pos));

        std::cout << "going through entries\n";
        for (size_t i = 0; i < num_entries; ++i) {
            uint32_t net_num;
            recvAll(sockfd, &net_num, sizeof(net_num));
            uint32_t num = ntohl(net_num);

            std::string str;
            char c;
            while (true) {
                ssize_t n = recv(sockfd, &c, 1, 0);
                if (n <= 0) throw std::runtime_error("Failed to receive string");
                if (c == '\0') break;
                str += c;
            }

            result.emplace_back(num, str);
        }

        return result;
    }

    private:
    static void sendAll(int sockfd, const void* buf, size_t len) {
        const char* ptr = static_cast<const char*>(buf);
        size_t total_sent = 0;
        std::cout << "sending " << *ptr << std::endl;
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