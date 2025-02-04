#ifndef COMMON_HTTP_CONNECTION_H
#define COMMON_HTTP_CONNECTION_H

#include "http/Request.h"
#include "http/Response.h"

#include <vector>

namespace mithril::http {

enum class Scheme : uint8_t { HTTP, HTTPS };

class RequestExecutor;

class Connection {
public:
    static Connection NewWithRequest(const Request& req);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    /**
     * @brief Processes data from the connection.
     */
    void Process(bool gotEof);

    /**
     * @brief Returns whether the HTTP response is ready to be consumed.
     */
    bool Ready() const;

    /**
     * @brief Extract the ready response from the connection. The connection
     * must be ready, as determined by Ready(). Subsequent calls are undefined.
     */
    Response GetResponse();

private:
    friend class RequestExecutor;

    enum class State : uint8_t { Sending, ReadingHeaders, ReadingChunks, ReadingBody, Complete, Consumed, Closed };

    Connection(int fd, std::string rawRequest);

    void Close();

    bool ProcessSend();
    bool ProcessReceive();

    bool ReadFromSocket();
    void ProcessHeaders();
    void ProcessBody();
    void ProcessChunks();

    int SocketDescriptor() const;
    bool IsWriting() const;
    bool IsReading() const;

    int fd_;
    State state_;

    std::string rawRequest_;
    size_t requestBytesSent_;

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
