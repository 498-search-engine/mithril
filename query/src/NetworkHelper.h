
#include <array>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <vector>

inline void send_message(int fd, const std::string &message) {

  ssize_t total_bytes_sent = 0;
  ssize_t message_length = ssize_t(message.length());

  while (total_bytes_sent < message_length) {
    ssize_t bytes_sent = send(fd, message.c_str() + total_bytes_sent,
                              (size_t)(message_length - total_bytes_sent), 0);

    if (bytes_sent == -1) {
      throw std::runtime_error("Failed to send data!");
    }

    total_bytes_sent += bytes_sent;
  }
}

// Contains: `data`, `connection_closed`
struct Receive {
  // Effects: Receives data from a fd
  Receive(int fd) {
    const int buffer_size = 4096; // Fixed buffer size of 4096 bytes
    char buffer[buffer_size + 1]; // Buffer to store the received data

    ssize_t bytesReceived = recv(fd, buffer, buffer_size, 0);

    if (bytesReceived < 0) {
      throw std::runtime_error("Failed to receive data!");
    } else if (bytesReceived == 0) {
      std::cerr << "Connection closed by the peer!" << std::endl;
      connection_closed = true;
      return;
    }

    buffer[bytesReceived] = '\0';
    data = std::string(buffer, (size_t)bytesReceived);
  }

  // Effects: Waits for exactly <num_bytes> from a fd
  Receive(int fd, int num_bytes) {

    std::vector<char> buffer((size_t)num_bytes + 1, '\0');

    // Receive data with MSG_WAITALL to ensure full num_bytes are received
    ssize_t bytesReceived =
        recv(fd, buffer.data(), (size_t)num_bytes, MSG_WAITALL);

    // Error handling
    if (bytesReceived < 0) {
      throw std::runtime_error("Failed to receive data!");
    } else if (bytesReceived == 0) {
      connection_closed = true;
      return;
    }

    buffer[(size_t)bytesReceived] = '\0';
    data = std::string(buffer.begin(), buffer.begin() + bytesReceived);
  };

  bool connection_closed = false;
  std::string data = "";
};
