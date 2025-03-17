#ifndef COMMON_HTTP_CONNECTION_H
#define COMMON_HTTP_CONNECTION_H

#include "http/Request.h"
#include "http/Resolver.h"
#include "http/Response.h"
#include "http/URL.h"

#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <optional>
#include <string>
#include <vector>
#include <openssl/ssl.h>

namespace mithril::http {

class RequestExecutor;
enum class RequestError : uint8_t;

class Connection {
public:
    static std::optional<Connection> NewFromRequest(const Request& req);
    static std::optional<Connection> NewFromURL(Method method, const URL& url, RequestOptions options = {});

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
     * @brief Returns whether the connection is actively sending/receiving data
     * from the remote server.
     */
    bool IsActive() const;

    /**
     * @brief Returns whether the connection encountered an error and is no
     * longer active.
     */
    bool IsError() const;

    /**
     * @brief Returns the RequestError associated with the encountered error.
     * Result is unspecified if IsError() is false.
     */
    RequestError GetError() const;

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
        Resolving,       // Resolving address
        TCPConnecting,   // Establishing connection with connect syscall
        SSLConnecting,   // Establishing SSL connection with SSL_connect
        Sending,         // Writing HTTP request to network
        ReadingHeaders,  // Reading HTTP response headers
        ReadingChunks,   // Reading HTTP response body (chunked encoding)
        ReadingBody,     // Reading HTTP response body (not chunked)
        Complete,        // HTTP response complete
        Closed,          // Socket closed

        ConnectError,           // Error while establishing connection
        SocketError,            // Error while reading/writing from socket
        UnexpectedEOFError,     // Got unexpected EOF while reading response
        InvalidResponseError,   // Generic bad response data
        ResponseTooBigError,    // Response body or header was too big
        ResponseWrongLanguage,  // Response Content-Language header was unacceptable
    };

    /**
     * @brief Construct a new connection to be executed.
     *
     * @param fd Socket fd
     * @param method HTTP method
     * @param url URL to fetch
     * @param options Request options
     */
    Connection(int fd, Method method, const URL& url, RequestOptions options);

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

    bool ValidateHeaders(const ResponseHeader& headers);

    int SocketDescriptor() const;
    bool IsSecure() const;
    bool IsWriting() const;
    bool IsReading() const;

    int fd_;
    ResolvedAddr address_;
    State state_;

    URL url_;
    std::string port_;
    RequestOptions reqOptions_;
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
