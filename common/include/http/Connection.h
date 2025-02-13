#ifndef COMMON_HTTP_CONNECTION_H
#define COMMON_HTTP_CONNECTION_H

#include "http/Request.h"
#include "http/Response.h"
#include "http/URL.h"

#include <netdb.h>
#include <optional>
#include <vector>
#include <openssl/ssl.h>

namespace mithril::http {

enum class Scheme : uint8_t { HTTP, HTTPS };

class RequestExecutor;

class Connection {
public:
    static std::optional<Connection> NewFromRequest(const Request& req);
    static std::optional<Connection> NewFromURL(Method method, const URL& url);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    /**
     * @brief Attempt to stablish the connection. Never blocks, but connection
     * is not guaranteed to be established until IsActive() returns true.
     */
    void Connect();

    /**
     * @brief Processes data from the connection.
     */
    void Process(bool gotEof);

    /**
     * @brief Returns whether the connection is being established.
     */
    bool IsConnecting() const;

    /**
     * @brief Returns whether the connection is actively sending/receiveing data
     * from the remote server.
     */
    bool IsActive() const;

    /**
     * @brief Returns whether the connection encountered an error and is no
     * longer active.
     */
    bool IsError() const;

    /**
     * @brief Returns whether the HTTP response is ready to be consumed.
     */
    bool IsComplete() const;

    /**
     * @brief Extract the ready response from the connection. The connection
     * must be complete, as determined by IsComplete(). Subsequent calls are
     * invalid.
     */
    Response GetResponse();

private:
    friend class RequestExecutor;

    enum class State : uint8_t {
        TCPConnecting,   // Establishing connection with connect syscall
        SSLConnecting,   // Establishing SSL connection with SSL_connect
        Sending,         // Writing HTTP request to network
        ReadingHeaders,  // Reading HTTP response headers
        ReadingChunks,   // Reading HTTP response body (chunked encoding)
        ReadingBody,     // Reading HTTP response body (not chunked)
        Complete,        // HTTP response complete
        Closed,          // Socket closed

        ConnectError,        // Error while establishing connection
        SocketError,         // Error while reading/writing from socket
        UnexpectedEOFError,  // Got unexpected EOF while reading response
    };

    /**
     * @brief Construct a new connection to be executed.
     *
     * @param fd Socket fd
     * @param address Address to connect to. Connection takes ownership of this
     * addrinfo and will call freeaddrinfo when appropriate.
     * @param req Request to be executed on the connection
     */
    Connection(int fd, struct addrinfo* address, Method method, const URL& url);

    void InitializeSSL();

    void Close();

    bool ProcessSend();
    bool ProcessReceive();

    bool WriteToSocketRaw();
    bool WriteToSocketSSL();

    bool ReadFromSocketRaw();
    bool ReadFromSocketSSL();

    void ProcessHeaders();
    void ProcessBody();
    void ProcessChunks();

    int SocketDescriptor() const;
    bool IsSecure() const;
    bool IsWriting() const;
    bool IsReading() const;

    int fd_;
    struct addrinfo* address_;
    State state_;

    URL url_;
    std::string rawRequest_;
    size_t requestBytesSent_;

    size_t contentLength_;
    size_t headersLength_;
    size_t bodyBytesRead_;
    size_t currentChunkSize_;
    size_t currentChunkBytesRead_;

    std::vector<char> buffer_;
    std::vector<char> body_;

    SSL* ssl_;
    bool isSecure_;
};

}  // namespace mithril::http

#endif
