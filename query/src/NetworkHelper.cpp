
#include <array>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/socket.h>

inline void send_message(int fd, const std::string& message) {

    ssize_t total_bytes_sent = 0;
    ssize_t message_length = ssize_t(message.length());

    while (total_bytes_sent < message_length) {
        ssize_t bytes_sent =
            send(fd, message.c_str() + total_bytes_sent, (size_t)(message_length - total_bytes_sent), 0);

        if (bytes_sent == -1) {
            throw std::runtime_error("Failed to send data!");
        }

        total_bytes_sent += bytes_sent;
    }
}
