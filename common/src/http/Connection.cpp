#include "http/Connection.h"

#include "Util.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Response.h"
#include "http/SSL.h"
#include "http/URL.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace mithril::http {

namespace {

constexpr size_t MaxHeaderSize = 8192;
constexpr size_t BufferSize = 8192;
constexpr const char* ContentLengthHeader = "Content-Length: ";
constexpr const char* ContentLanguageHeader = "Content-Language: ";
constexpr const char* TransferEncodingChunkedHeader = "Transfer-Encoding: chunked";
constexpr const char* HeaderDelimiter = "\r\n\r\n";
constexpr const char* CRLF = "\r\n";

void PrintSSLError(SSL* ssl, int status, const char* operation) {
    int sslErr = SSL_get_error(ssl, status);
    spdlog::warn("connection: {} failed with error code {}", operation, sslErr);

    // Print the entire error queue
    unsigned long err;
    while ((err = ERR_get_error()) != 0) {
        char errBuf[256];
        ERR_error_string_n(err, errBuf, sizeof(errBuf));
        spdlog::warn("ssl error: {}", errBuf);
    }
}

void PrintSSLConnectError(SSL* ssl, int status) {
    PrintSSLError(ssl, status, "SSL_connect");

    // Print verification errors if any
    long verifyResult = SSL_get_verify_result(ssl);
    if (verifyResult != X509_V_OK) {
        spdlog::warn("certificate verification error: {}", X509_verify_cert_error_string(verifyResult));
    }

    // Print peer certificate info if available
    X509* cert = SSL_get_peer_certificate(ssl);
    if (cert != nullptr) {
        char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
        char* issuer = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
        spdlog::warn("peer certificate: subject={} issuer={}", subject, issuer);
        OPENSSL_free(subject);
        OPENSSL_free(issuer);
        X509_free(cert);
    }
}

bool ContentLanguageMatches(std::string_view header, std::string_view lang) {
    if (lang.empty()) {
        return true;
    }

    auto semiPos = header.find(';');
    if (semiPos != std::string_view::npos) {
        header = header.substr(0, semiPos);
    }

    if (lang.back() == '*') {
        lang.remove_suffix(1);
        return InsensitiveStartsWith(header, lang);
    } else {
        return InsensitiveStrEquals(header, lang);
    }
}

}  // namespace

std::optional<Connection> Connection::NewFromRequest(const Request& req) {
    return Connection::NewFromURL(req.GetMethod(), req.Url(), req.Options());
}

std::optional<Connection> Connection::NewFromURL(Method method, const URL& url, const RequestOptions& options) {
    struct addrinfo* address;
    struct addrinfo hints {};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    bool isSecure = url.scheme == "https";
    const char* port = url.port.empty() ? (isSecure ? "443" : "80") : url.port.c_str();

    int status = getaddrinfo(url.host.c_str(), port, &hints, &address);
    if (status == -1 || address == nullptr) {
        spdlog::warn("failed to get addr for {}:{}", url.host, port);
        return std::nullopt;
    }

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        freeaddrinfo(address);
        spdlog::error("failed to create socket: {}", strerror(errno));
        return std::nullopt;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        spdlog::error("failed to put socket into non-blocking mode");
        // continue anyway
    }

    return Connection{fd, address, method, url, options};
}

Connection::Connection(int fd, struct addrinfo* address, Method method, const URL& url, const RequestOptions& options)
    : fd_(fd),
      address_(address),
      state_(State::TCPConnecting),
      url_(url),
      reqOptions_(options),
      rawRequest_(BuildRawRequestString(method, url)),
      requestBytesSent_(0),
      contentLength_(0),
      headersLength_(0),
      bodyBytesRead_(0),
      currentChunkSize_(0),
      currentChunkBytesRead_(0),
      ssl_(nullptr),
      isSecure_(url.scheme == "https") {
    if (isSecure_) {
        InitializeSSL();
    }
}

Connection::~Connection() {
    Close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_),
      address_(other.address_),
      state_(other.state_),
      url_(std::move(other.url_)),
      reqOptions_(other.reqOptions_),
      rawRequest_(std::move(other.rawRequest_)),
      requestBytesSent_(other.requestBytesSent_),
      contentLength_(other.contentLength_),
      headersLength_(other.headersLength_),
      bodyBytesRead_(other.bodyBytesRead_),
      currentChunkSize_(other.currentChunkSize_),
      currentChunkBytesRead_(other.currentChunkBytesRead_),
      buffer_(std::move(other.buffer_)),
      body_(std::move(other.body_)),
      ssl_(other.ssl_),
      isSecure_(other.isSecure_) {
    other.fd_ = -1;
    other.address_ = nullptr;
    other.state_ = State::Closed;
    other.ssl_ = nullptr;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    Close();

    fd_ = other.fd_;
    address_ = other.address_;
    state_ = other.state_;
    url_ = std::move(other.url_);
    reqOptions_ = other.reqOptions_;
    rawRequest_ = std::move(other.rawRequest_);
    requestBytesSent_ = other.requestBytesSent_;
    contentLength_ = other.contentLength_;
    headersLength_ = other.headersLength_;
    bodyBytesRead_ = other.bodyBytesRead_;
    currentChunkSize_ = other.currentChunkSize_;
    currentChunkBytesRead_ = other.currentChunkBytesRead_;
    buffer_ = std::move(other.buffer_);
    body_ = std::move(other.body_);
    ssl_ = other.ssl_;
    isSecure_ = other.isSecure_;

    other.fd_ = -1;
    other.address_ = nullptr;
    other.state_ = State::Closed;
    other.ssl_ = nullptr;

    return *this;
}

int Connection::SocketDescriptor() const {
    return fd_;
}

bool Connection::IsComplete() const {
    return state_ == State::Complete;
}

void Connection::InitializeSSL() {
    auto* ctx = http::internal::SSLCtx;
    assert(ctx != nullptr);

    ERR_clear_error();

    ssl_ = SSL_new(ctx);
    if (ssl_ == nullptr) {
        // TODO: error handling strategy
        throw std::runtime_error("Failed to create SSL object");
    }

    int status = SSL_set_fd(ssl_, fd_);
    if (status != 1) {
        PrintSSLError(ssl_, status, "SSL_set_fd");
        SSL_free(ssl_);
        ssl_ = nullptr;
        // TODO: error handling strategy
        throw std::runtime_error("Failed to set SSL file descriptor");
    }

    // Set Server Name Indication (SNI)
    status = SSL_set_tlsext_host_name(ssl_, url_.host.c_str());
    if (status != 1) {
        PrintSSLError(ssl_, status, "SSL_set_tlsext_host_name");
        SSL_free(ssl_);
        ssl_ = nullptr;
        throw std::runtime_error("Failed to set SNI hostname");
    }

    // Set DNS hostname to verify
    status = SSL_set1_host(ssl_, url_.host.c_str());
    if (status != 1) {
        PrintSSLError(ssl_, status, "SSL_set1_host");
        SSL_free(ssl_);
        ssl_ = nullptr;
        throw std::runtime_error("Failed to set certificate verification hostname");
    }
}

void Connection::Close() {
    if (isSecure_ && ssl_ != nullptr) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (address_ != nullptr) {
        // TODO(dsage): only do this if we manage the lifetime of address_
        freeaddrinfo(address_);
        address_ = nullptr;
    }
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

Response Connection::GetResponse() {
    assert(state_ == State::Complete);

    // Close socket and shutdown connection
    state_ = State::Closed;
    Close();

    auto header = std::vector<char>{buffer_.begin(), buffer_.begin() + headersLength_};
    buffer_.clear();
    return Response{
        std::move(header),
        std::move(body_),
    };
}

bool Connection::WriteToSocketRaw() {
    assert(!isSecure_);
    ssize_t bytesSent;

    do {
        bytesSent = send(fd_, rawRequest_.data(), rawRequest_.size() - requestBytesSent_, MSG_DONTWAIT);
        if (bytesSent > 0) {
            requestBytesSent_ += bytesSent;
        } else if (bytesSent == 0) {
            // Got EOF
            return true;
        } else if (bytesSent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false;
            }
            spdlog::error("connection: write to socket: {}", strerror(errno));
            state_ = State::SocketError;
            Close();
            return false;
        }
    } while (requestBytesSent_ < rawRequest_.size());

    // Request fully written, now into reading headers phase
    state_ = State::ReadingHeaders;

    return false;
}

bool Connection::WriteToSocketSSL() {
    assert(isSecure_);
    ssize_t bytesSent;

    ERR_clear_error();

    do {
        bytesSent = SSL_write(ssl_, rawRequest_.data() + requestBytesSent_, rawRequest_.size() - requestBytesSent_);
        if (bytesSent > 0) {
            requestBytesSent_ += bytesSent;
        } else {
            assert(bytesSent <= 0);
            int err = SSL_get_error(ssl_, bytesSent);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                // Can't write more data right now
                return false;
            } else if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL) {
                return true;
            } else {
                PrintSSLError(ssl_, bytesSent, "SSL_write");
                state_ = State::SocketError;
                Close();
                return false;
            }
        }
    } while (requestBytesSent_ < rawRequest_.size());

    // Request fully written, now into reading headers phase
    state_ = State::ReadingHeaders;

    return false;
}

bool Connection::ReadFromSocketRaw() {
    assert(!isSecure_);
    char tempBuffer[BufferSize];
    ssize_t bytesRead;
    do {
        bytesRead = recv(fd_, tempBuffer, BufferSize, MSG_DONTWAIT);

        if (bytesRead > 0) {
            buffer_.insert(buffer_.end(), tempBuffer, tempBuffer + bytesRead);
        } else if (bytesRead == 0) {
            // Got EOF
            return true;
        } else if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available right now
                return false;
            }
            // TODO: error logging
            spdlog::error("connection: read from socket: {}", strerror(errno));
            state_ = State::SocketError;
            Close();
            return false;
        }
    } while (bytesRead > 0);
    return false;
}

bool Connection::ReadFromSocketSSL() {
    assert(isSecure_);
    char tempBuffer[BufferSize];
    ssize_t bytesRead;

    ERR_clear_error();

    do {
        bytesRead = SSL_read(ssl_, tempBuffer, BufferSize);
        if (bytesRead > 0) {
            buffer_.insert(buffer_.end(), tempBuffer, tempBuffer + bytesRead);
        } else {
            assert(bytesRead <= 0);
            int err = SSL_get_error(ssl_, bytesRead);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                // No more data available right now
                return false;
            } else if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL) {
                return true;
            } else {
                PrintSSLError(ssl_, bytesRead, "SSL_read");
                state_ = err == SSL_ERROR_SSL ? State::UnexpectedEOFError : State::SocketError;
                Close();
                return false;
            }
        }
    } while (bytesRead > 0);
    return false;
}

void Connection::Connect() {
    if (state_ == State::TCPConnecting) {
        int status = connect(fd_, address_->ai_addr, address_->ai_addrlen);
        if (status == 0) {
            state_ = isSecure_ ? State::SSLConnecting : State::Sending;
        } else if (status < 0) {
            if (errno == EINPROGRESS || errno == EALREADY) {
                // Establishing connection in progress
                return;
            } else if (errno == EISCONN) {
                // Already connected
                state_ = isSecure_ ? State::SSLConnecting : State::Sending;
            } else {
                // Some other error occurred
                spdlog::warn("connection: connect: {}", strerror(errno));
                state_ = State::ConnectError;
                Close();
                return;
            }
        }
    }

    if (state_ == State::SSLConnecting) {
        int status = SSL_connect(ssl_);
        if (status == 1) {
            // Transition into sending request state
            state_ = State::Sending;
        } else if (status < 0) {
            int error = SSL_get_error(ssl_, status);
            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                // Establishing SSL connection in progress
                return;
            }

            // Actual SSL error occurred
            PrintSSLConnectError(ssl_, status);
            state_ = State::ConnectError;
            Close();
            return;
        }
    }
}

void Connection::Process(bool gotEof) {
    if (IsComplete() || state_ == State::Closed || IsError()) {
        return;
    }

    if (IsWriting()) {
        // Write request data to socket
        gotEof |= ProcessSend();
    }

    if (IsReading()) {
        // Read response data from socket
        gotEof |= ProcessReceive();
    }

    if (gotEof) {
        if (!IsError()) {
            if ((state_ == State::ReadingBody && bodyBytesRead_ < contentLength_) ||
                (state_ == State::ReadingChunks && currentChunkSize_ != 0)) {
                state_ = State::UnexpectedEOFError;
                spdlog::warn("connection: closed before receiving complete response from {}:{}", url_.host, url_.port);
            } else {
                state_ = State::Complete;
            }
        }
        Close();
    }
}

bool Connection::ProcessSend() {
    if (isSecure_) {
        return WriteToSocketSSL();
    } else {
        return WriteToSocketRaw();
    }
}

bool Connection::ProcessReceive() {
    bool gotEof;
    if (isSecure_) {
        gotEof = ReadFromSocketSSL();
    } else {
        gotEof = ReadFromSocketRaw();
    }

    // Process based on current state
    switch (state_) {
    case State::ReadingHeaders:
        ProcessHeaders();
        break;

    case State::ReadingBody:
        ProcessBody();
        break;

    case State::ReadingChunks:
        ProcessChunks();
        break;

    default:
        break;
    }

    return gotEof;
}

bool Connection::IsConnecting() const {
    return state_ == State::TCPConnecting || state_ == State::SSLConnecting;
}

bool Connection::IsActive() const {
    return IsWriting() || IsReading();
}

bool Connection::IsError() const {
    return state_ == State::ConnectError || state_ == State::SocketError || state_ == State::UnexpectedEOFError ||
           state_ == State::ResponseTooBigError || state_ == State::ResponseWrongLanguage;
}

RequestError Connection::GetError() const {
    switch (state_) {
    case State::ResponseTooBigError:
        return RequestError::ResponseTooBig;
    case State::ResponseWrongLanguage:
        return RequestError::ResponseWrongLanguage;
    case State::ConnectError:
    case State::SocketError:
    case State::UnexpectedEOFError:
    default:
        return RequestError::ConnectionError;
    }
}

bool Connection::IsSecure() const {
    return isSecure_;
}

bool Connection::IsWriting() const {
    return state_ == State::Sending;
}

bool Connection::IsReading() const {
    return state_ == State::ReadingHeaders || state_ == State::ReadingBody || state_ == State::ReadingChunks;
}

void Connection::ProcessHeaders() {
    // Look for header delimiter
    auto headerEnd = std::search(buffer_.begin(), buffer_.end(), HeaderDelimiter, HeaderDelimiter + 4);

    if (headerEnd == buffer_.end()) {
        if (buffer_.size() > MaxHeaderSize) {
            // Header is too big
            spdlog::debug("header length for response {} exceeds max header size {}", buffer_.size(), MaxHeaderSize);
            state_ = State::ResponseTooBigError;
            return;
        }
        return;  // Haven't received full headers yet
    }

    // Headers are complete
    headersLength_ = headerEnd - buffer_.begin() + 4;  // Include delimiter
    auto headers = std::string{buffer_.begin(), headerEnd};

    // Check for chunked encoding
    if (FindCaseInsensitive(headers, TransferEncodingChunkedHeader) != std::string::npos) {
        state_ = State::ReadingChunks;
        ProcessChunks();
        return;
    }

    // Look for Content-Length header
    auto contentLengthPos = FindCaseInsensitive(headers, ContentLengthHeader);
    if (contentLengthPos != std::string::npos) {
        contentLengthPos += strlen(ContentLengthHeader);
        auto lengthEnd = headers.find(CRLF, contentLengthPos);
        contentLength_ = std::stoul(headers.substr(contentLengthPos, lengthEnd - contentLengthPos));
        buffer_.reserve(buffer_.size() + contentLength_);
        body_.reserve(contentLength_);
        if (reqOptions_.maxResponseSize > 0 && contentLength_ > reqOptions_.maxResponseSize) {
            // Response is too big
            spdlog::debug("content-length {} for response {} exceeds max response size", contentLength_, url_.url);
            state_ = State::ResponseTooBigError;
            return;
        }
    }

    if (!reqOptions_.allowedContentLanguage.empty()) {
        // Look for Content-Language header
        auto contentLanguagePos = FindCaseInsensitive(headers, ContentLanguageHeader);
        if (contentLanguagePos != std::string::npos) {
            contentLanguagePos += strlen(ContentLanguageHeader);
            auto langEnd = headers.find(CRLF, contentLanguagePos);
            auto contentLanguage = headers.substr(contentLanguagePos, langEnd - contentLanguagePos);
            auto anyMatch = std::any_of(
                reqOptions_.allowedContentLanguage.begin(),
                reqOptions_.allowedContentLanguage.end(),
                [contentLanguage](std::string_view lang) { return ContentLanguageMatches(contentLanguage, lang); });
            if (!anyMatch) {
                spdlog::debug("content-language {} for response {} is not acceptable", contentLanguage, url_.url);
                state_ = State::ResponseWrongLanguage;
                return;
            }
        }
    }

    state_ = State::ReadingBody;
    ProcessBody();  // Process any body data we already have
}

void Connection::ProcessBody() {
    size_t receivedBodyBytes = buffer_.size() - headersLength_;
    assert(receivedBodyBytes >= bodyBytesRead_);
    size_t bytesToRead = receivedBodyBytes - bodyBytesRead_;

    if (bytesToRead == 0) {
        return;
    }

    // Copy actual body data to body buffer
    body_.insert(body_.end(),
                 buffer_.begin() + headersLength_ + bodyBytesRead_,
                 buffer_.begin() + headersLength_ + bodyBytesRead_ + bytesToRead);

    bodyBytesRead_ += bytesToRead;

    if (bodyBytesRead_ == contentLength_) {
        state_ = State::Complete;
    }
}

void Connection::ProcessChunks() {
    while (true) {
        if (currentChunkSize_ == 0) {
            // Need to read next chunk size
            auto chunkHeaderEnd =
                std::search(buffer_.begin() + headersLength_ + bodyBytesRead_, buffer_.end(), CRLF, CRLF + 2);

            if (chunkHeaderEnd == buffer_.end()) {
                return;  // Don't have complete chunk header
            }

            auto chunkHeader = std::string{buffer_.begin() + headersLength_ + bodyBytesRead_, chunkHeaderEnd};
            currentChunkSize_ = std::stoul(chunkHeader, nullptr, 16);
            bodyBytesRead_ += (chunkHeaderEnd - (buffer_.begin() + headersLength_ + bodyBytesRead_)) + 2;

            if (currentChunkSize_ == 0) {
                // Final chunk
                state_ = State::Complete;
                return;
            }
        }

        if (reqOptions_.maxResponseSize > 0 && bodyBytesRead_ > reqOptions_.maxResponseSize) {
            // Response is too big
            spdlog::debug(
                "chunked response with size {} for request {} exceeds max response size", bodyBytesRead_, url_.url);
            state_ = State::ResponseTooBigError;
            return;
        }

        size_t availableBytes = buffer_.size() - (headersLength_ + bodyBytesRead_);
        size_t bytesToRead = std::min(availableBytes, currentChunkSize_ - currentChunkBytesRead_);

        // Copy chunk data to body buffer, excluding delimiters
        body_.insert(body_.end(),
                     buffer_.begin() + headersLength_ + bodyBytesRead_,
                     buffer_.begin() + headersLength_ + bodyBytesRead_ + bytesToRead);

        currentChunkBytesRead_ += bytesToRead;
        bodyBytesRead_ += bytesToRead;

        if (currentChunkBytesRead_ == currentChunkSize_) {
            // Chunk complete, move past trailing CRLF
            bodyBytesRead_ += 2;
            currentChunkSize_ = 0;
            currentChunkBytesRead_ = 0;
        }

        if (availableBytes == bytesToRead) {
            return;  // No more data to process
        }
    }
}

}  // namespace mithril::http
