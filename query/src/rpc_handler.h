#include "QueryManager.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include "core/pair.h"
#include <vector>
#include <arpa/inet.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

struct RPCHandler {
    using QueryResults = QueryManager::QueryResult;

public:
    static std::string ReadQuery(int client_fd) {
        uint32_t query_length = 0;

        // Step 1: Receive query length (4 bytes)
        ssize_t bytes_read = recv(client_fd, &query_length, sizeof(query_length), MSG_WAITALL);
        if (bytes_read != sizeof(query_length)) {
            spdlog::error("length of query length was supposed to be {}", sizeof(query_length));
            spdlog::error("query length was read to be {}", query_length);
            spdlog::error("Failed to read query length, received {} bytes", bytes_read);
            uint32_t result_count = 0;
            //send(client_fd, &result_count, sizeof(uint32_t), 0);
            SendResults(client_fd, {}, 0);
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
            //send(client_fd, &result_count, sizeof(uint32_t), 0);
            SendResults(client_fd, {}, 0);
            return query;
        }

        spdlog::info("Received binary query: '{}'", query);
        return query;
    }

    static void SendResults(int sockfd, const QueryResults& data, const size_t total_size){
        std::string size_header = std::to_string(total_size) + "\r\n\r\n";
        sendAll(sockfd, size_header.c_str(), size_header.size());
    
        std::string header = std::to_string(data.size()) + "\r\n\r\n";
        sendAll(sockfd, header.c_str(), header.size());

        for (const auto& tup : data) {
            // Send doc ID and score
            uint32_t send_a = htonl(std::get<0>(tup));
            uint32_t send_b = htonl(std::get<1>(tup));
            sendAll(sockfd, &send_a, sizeof(send_a));
            sendAll(sockfd, &send_b, sizeof(send_b));

            // Send URL
            auto url = std::get<2>(tup);
            url += "\r\n\r\n";
            sendAll(sockfd, url.c_str(), url.size());

            // Send title
            auto title = std::get<3>(tup);
            std::string num_results = std::to_string(title.size()) + "\r\n\r\n";
            sendAll(sockfd, num_results.c_str(), num_results.size());
            for (const auto& word : title) {
                auto cpy = word + "\r\n\r\n";
                sendAll(sockfd, cpy.c_str(), cpy.size());
            }

            // send positions data
            const auto& positions = std::get<4>(tup);
            std::string num_terms = std::to_string(positions.size()) + "\r\n\r\n";
            sendAll(sockfd, num_terms.c_str(), num_terms.size());

            // Send each term and its positions
            for (const auto& [term, pos_vec] : positions) {
                // Send term
                auto term_str = term + "\r\n\r\n";
                sendAll(sockfd, term_str.c_str(), term_str.size());

                // Send number of positions
                std::string num_pos = std::to_string(pos_vec.size()) + "\r\n\r\n";
                sendAll(sockfd, num_pos.c_str(), num_pos.size());

                // Send each position
                for (uint16_t pos : pos_vec) {
                    uint16_t net_pos = htons(pos);
                    sendAll(sockfd, &net_pos, sizeof(net_pos));
                }
            }
        }
    }

    static QueryResults ReadResults(int sockfd, size_t& total_size){
        //blocking until read can happen
        QueryResults result;

        auto recv_until_delim = [&](const std::string& delim, std::string what) {
            std::string buffer;
            char c;
            while (true) {
                ssize_t n = recv(sockfd, &c, 1, 0);
                if (n <= 0)
                    throw std::runtime_error("Failed to receive " + what);
                buffer += c;
                if (buffer.size() >= delim.size() && buffer.substr(buffer.size() - delim.size()) == delim) {
                    break;
                }
            }
            return buffer;
        };
        
        std::string size_header = recv_until_delim("\r\n\r\n", "header");
        size_t size_pos = size_header.find("\r\n\r\n");
        if (size_pos == std::string::npos) throw std::runtime_error("Invalid size header format");
        total_size = std::stoi(size_header.substr(0, size_pos));

        std::string header = recv_until_delim("\r\n\r\n", "header");
        size_t pos = header.find("\r\n\r\n");
        if (pos == std::string::npos) throw std::runtime_error("Invalid header format");
        uint32_t num_entries = std::stoi(header.substr(0, pos));
        // std::cout << num_entries <<  " entries" << std::endl;

        for (size_t i = 0; i < num_entries; ++i) {
            uint32_t net_a, net_b;
            recvAll(sockfd, &net_a, sizeof(net_a));
            recvAll(sockfd, &net_b, sizeof(net_b));

            uint32_t a = ntohl(net_a);
            uint32_t b = ntohl(net_b);

            std::string url_full = recv_until_delim("\r\n\r\n", "url");
            std::string url = url_full.substr(0, url_full.size() - 4);
            // std::cout << url << std::endl;
            std::string title_len = recv_until_delim("\r\n\r\n", "title_length");
            size_t pos_entry = title_len.find("\r\n\r\n");
            if (pos_entry == std::string::npos) throw std::runtime_error("Invalid title format");
            uint32_t title_entries = std::stoi(title_len.substr(0, pos_entry));
            //std::cout << title_entries << " title entries" << std::endl;
            std::vector<std::string> title;
            for (size_t i = 0; i < title_entries; ++i) {
                std::string cpy = recv_until_delim("\r\n\r\n", "title word");
                std::string word = cpy.substr(0, cpy.size() - 4);
                // std::cout << word << " ";
                title.push_back(word);
            }

            QueryManager::TermPositionMap positions;

            // Read number of terms with positions
            std::string terms_count_str = recv_until_delim("\r\n\r\n", "terms count");
            size_t terms_pos = terms_count_str.find("\r\n\r\n");
            if (terms_pos == std::string::npos)
                throw std::runtime_error("Invalid terms count format");
            size_t num_terms = std::stoi(terms_count_str.substr(0, terms_pos));

            // Read each term's positions
            for (size_t j = 0; j < num_terms; ++j) {
                // Read term
                std::string term_str = recv_until_delim("\r\n\r\n", "term");
                std::string term = term_str.substr(0, term_str.size() - 4);  // Remove delimiter

                // Read position count
                std::string pos_count_str = recv_until_delim("\r\n\r\n", "position count");
                size_t pos_count_end = pos_count_str.find("\r\n\r\n");
                if (pos_count_end == std::string::npos)
                    throw std::runtime_error("Invalid position count format");
                size_t num_positions = std::stoi(pos_count_str.substr(0, pos_count_end));

                // Read positions
                std::vector<uint16_t> term_positions;
                term_positions.reserve(num_positions);

                for (size_t k = 0; k < num_positions; ++k) {
                    uint16_t net_pos;
                    recvAll(sockfd, &net_pos, sizeof(net_pos));
                    term_positions.push_back(ntohs(net_pos));
                }

                positions[term] = std::move(term_positions);
            }

            // std::cout << std::endl;
            result.emplace_back(a, b, url, title, std::move(positions));
        }

        return result;
    }

private:
    static void sendAll(int sockfd, const void* buf, size_t len) {
        const char* ptr = static_cast<const char*>(buf);
        size_t total_sent = 0;
        // std::cout << "sending " << *ptr << std::endl;
        while (total_sent < len) {
            ssize_t sent = send(sockfd, ptr + total_sent, len - total_sent, 0);
            if (sent <= 0)
                throw std::runtime_error("Failed to send data");
            total_sent += sent;
        }
    }

    static void recvAll(int sockfd, void* buf, size_t len) {
        char* ptr = static_cast<char*>(buf);
        size_t total_recv = 0;
        while (total_recv < len) {
            ssize_t recvd = recv(sockfd, ptr + total_recv, len - total_recv, 0);
            if (recvd <= 0)
                throw std::runtime_error("Failed to receive data");
            total_recv += recvd;
        }
    }
};