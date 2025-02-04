#ifndef COMMON_HTTP_CONNECTION_H
#define COMMON_HTTP_CONNECTION_H

#include "http/Request.h"
#include "http/Response.h"

#include <string>
#include <vector>

namespace mithril::http {

enum class Scheme : uint8_t { HTTP, HTTPS };

class Connection {
public:
    static Connection NewWithRequest(const Request& req);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    int SocketDescriptor() const;

    /**
     * @brief Returns whether the HTTP response is ready to be consumed.
     */
    bool Ready() const;

    void Close();

    /**
     * @brief Processes data from the connection.
     */
    void Process(bool gotEof);

    Response GetResponse();

private:
    enum class State : uint8_t { Pending, ReadingHeaders, ReadingChunks, ReadingBody, Complete, Consumed, Closed };

    Connection(int fd);

    bool ReadFromSocket();
    void ProcessHeaders();
    void ProcessBody();
    void ProcessChunks();

    int fd_;
    State state_;

    size_t contentLength_;
    size_t headersLength_;
    size_t bodyBytesRead_;
    bool isChunked_;
    size_t currentChunkSize_;
    size_t currentChunkBytesRead_;

    std::vector<char> buffer_;
    std::vector<char> body_;
};

}  // namespace mithril::http

#endif
