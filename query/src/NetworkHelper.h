
#include <array>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/socket.h>

inline void send_message(int fd, const std::string& message);

// Contains: `data`, `connection_closed`
struct Receive {

    Receive(const Receive&) = default;
    Receive(Receive&&) = default;
    Receive& operator=(const Receive&) = default;
    Receive& operator=(Receive&&) = default;
    
    // Effects: Receives data from a fd
    Receive(int fd);

    // Effects: Waits for exactly <num_bytes> from a fd
    Receive(int fd, int num_bytes);

    bool connection_closed = false;
    std::string data = "";
};
