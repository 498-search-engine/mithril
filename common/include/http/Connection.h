#ifndef COMMON_HTTP_CONNECTION_H
#define COMMON_HTTP_CONNECTION_H

#include "http/Response.h"

#include <string>
#include <vector>

namespace mithril::http {

enum class Scheme : uint8_t { HTTP, HTTPS };

class Connection {
public:
    Connection(const std::string& hostname, Scheme scheme);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    int SocketDescriptor() const;
    bool Ready() const;

    void Process();
    Response Response();

private:
    enum class State : uint8_t { Pending, ReadingHeaders, Complete };

    int fd_;
    State state_;
    std::vector<char> buffer_;
};

}  // namespace mithril::http

#endif
