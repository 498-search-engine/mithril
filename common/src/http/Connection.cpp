#include "http/Connection.h"

#include "Util.h"
#include "http/Request.h"
#include "http/RequestExecutor.h"
#include "http/Resolver.h"
#include "http/Response.h"
#include "http/SSL.h"
#include "http/URL.h"
#include "spdlog/common.h"

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
#include <string>
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

using namespace std::string_view_literals;

namespace {

constexpr size_t MaxHeaderSize = 8192;
constexpr size_t BufferSize = 8192;
constexpr auto HeaderDelimiter = "\r\n\r\n"sv;
constexpr auto CRLF = "\r\n"sv;

void PrintSSLError(SSL* ssl, int status, const char* operation, spdlog::level::level_enum level) {
    int sslErr = SSL_get_error(ssl, status);
    spdlog::log(level, "connection: {} failed with error code {}", operation, sslErr);

    // Print the entire error queue
    unsigned long err;
    while ((err = ERR_get_error()) != 0) {
        char errBuf[256];
        ERR_error_string_n(err, errBuf, sizeof(errBuf));
        spdlog::log(level, "ssl error: {}", errBuf);
    }
}

void PrintSSLConnectError(SSL* ssl, int status) {
    PrintSSLError(ssl, status, "SSL_connect", spdlog::level::warn);

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

}  // namespace

std::optional<Connection> Connection::NewFromRequest(const Request& req) {
    return Connection::NewFromURL(req.GetMethod(), req.Url(), req.Options());
}

std::optional<Connection> Connection::NewFromURL(Method method, const URL& url, RequestOptions options) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        spdlog::error("failed to create socket: {}", strerror(errno));
        return std::nullopt;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        spdlog::error("failed to put socket into non-blocking mode");
        // continue anyway
    }

    return Connection{fd, method, url, std::move(options)};
}

Connection::Connection(int fd, Method method, const URL& url, RequestOptions options)
    : fd_(fd),
      state_(State::Resolving),
      url_(url),
      port_(url.port.empty() ? (url.scheme == "https" ? "443" : "80") : url.port),
      reqOptions_(std::move(options)),
      rawRequest_(BuildRawRequestString(method, url, options)),
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
      address_(std::move(other.address_)),
      state_(other.state_),
      url_(std::move(other.url_)),
      port_(std::move(other.port_)),
      reqOptions_(std::move(other.reqOptions_)),
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
    other.state_ = State::Closed;
    other.ssl_ = nullptr;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    Close();

    fd_ = other.fd_;
    address_ = std::move(other.address_);
    state_ = other.state_;
    url_ = std::move(other.url_);
    port_ = std::move(other.port_);
    reqOptions_ = std::move(other.reqOptions_);
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
        spdlog::error("failed to create SSL object");
        state_ = State::ConnectError;
        return;
    }

    int status = SSL_set_fd(ssl_, fd_);
    if (status != 1) {
        PrintSSLError(ssl_, status, "SSL_set_fd", spdlog::level::err);
        SSL_free(ssl_);
        ssl_ = nullptr;
        state_ = State::ConnectError;
        return;
    }

    // Set Server Name Indication (SNI)
    status = SSL_set_tlsext_host_name(ssl_, url_.host.c_str());
    if (status != 1) {
        PrintSSLError(ssl_, status, "SSL_set_tlsext_host_name", spdlog::level::err);
        SSL_free(ssl_);
        ssl_ = nullptr;
        state_ = State::ConnectError;
        return;
    }

    // Set DNS hostname to verify
    status = SSL_set1_host(ssl_, url_.host.c_str());
    if (status != 1) {
        PrintSSLError(ssl_, status, "SSL_set1_host", spdlog::level::err);
        SSL_free(ssl_);
        ssl_ = nullptr;
        state_ = State::ConnectError;
        return;
    }
}

void Connection::Close() {
    if (isSecure_ && ssl_ != nullptr) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

Response Connection::GetResponse() {
    assert(state_ == State::Complete);
    assert(!headers_.empty());

    // Close socket and shutdown connection
    state_ = State::Closed;
    Close();

    buffer_.clear();
    return Response{
        std::move(headers_),
        std::move(body_),
        std::move(parsedHeader_),
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
#if not defined(NDEBUG)
                PrintSSLError(ssl_, bytesSent, "SSL_write", spdlog::level::warn);
#endif
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
            spdlog::warn("connection: read from socket: {}", strerror(errno));
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
#if not defined(NDEBUG)
                PrintSSLError(ssl_, bytesRead, "SSL_read", spdlog::level::warn);
#endif
                state_ = err == SSL_ERROR_SSL ? State::UnexpectedEOFError : State::SocketError;
                Close();
                return false;
            }
        }
    } while (bytesRead > 0);
    return false;
}

void Connection::Connect() {
    if (state_ == State::Resolving) {
        Resolver::ResolutionResult result{};
        bool resolved = ApplicationResolver->Resolve(url_.host, port_, result);
        if (!resolved) {
            // Address resolution in progress
            return;
        }

        if (result.status != 0) {
            spdlog::warn("failed to get addr for {}:{}", url_.host, port_);
            state_ = State::ConnectError;
            Close();
            return;
        }

        assert(result.addr.has_value());
        address_ = std::move(*result.addr);
        state_ = State::TCPConnecting;
    }

    if (state_ == State::TCPConnecting) {
        int status = connect(fd_, address_.AddrInfo()->ai_addr, address_.AddrInfo()->ai_addrlen);
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

#if not defined(NDEBUG)
            PrintSSLConnectError(ssl_, status);
#endif
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
            if (state_ == State::Sending || state_ == State::ReadingHeaders ||
                (state_ == State::ReadingBody && bodyBytesRead_ < contentLength_) ||
                (state_ == State::ReadingChunks && currentChunkSize_ != 0)) {
                state_ = State::UnexpectedEOFError;
                SPDLOG_DEBUG("connection: closed before receiving complete response from {}:{}", url_.host, url_.port);
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
    return state_ == State::Resolving || state_ == State::TCPConnecting || state_ == State::SSLConnecting;
}

bool Connection::IsActive() const {
    return IsWriting() || IsReading();
}

bool Connection::IsError() const {
    return state_ == State::ConnectError || state_ == State::SocketError || state_ == State::UnexpectedEOFError ||
           state_ == State::InvalidResponseError || state_ == State::ResponseTooBigError ||
           state_ == State::ResponseWrongType || state_ == State::ResponseWrongLanguage;
}

RequestError Connection::GetError() const {
    switch (state_) {
    case State::ResponseTooBigError:
        return RequestError::ResponseTooBig;
    case State::ResponseWrongType:
        return RequestError::ResponseWrongType;
    case State::ResponseWrongLanguage:
        return RequestError::ResponseWrongLanguage;
    case State::ConnectError:
    case State::SocketError:
    case State::UnexpectedEOFError:
    case State::InvalidResponseError:
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
    auto headerEnd = std::search(buffer_.begin(), buffer_.end(), HeaderDelimiter.begin(), HeaderDelimiter.end());

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
    headers_.resize(headersLength_);
    std::memcpy(headers_.data(), buffer_.data(), headersLength_);

    auto parsedHeader = ParseResponseHeader(std::string_view{headers_.data(), headers_.size()});
    if (!parsedHeader.has_value()) {
        spdlog::debug("failed to parse headers for {}", url_.url);
        state_ = State::InvalidResponseError;
        return;
    }
    parsedHeader_ = std::move(*parsedHeader);

    if (!ValidateHeaders(parsedHeader_)) {
        // Headers are not valid for request options. State has been set.
        return;
    }

    // Check for chunked encoding
    if (parsedHeader_.TransferEncoding != nullptr) {
        // Only supported transfer encoding is chunked
        if (!InsensitiveStrEquals(parsedHeader_.TransferEncoding->value, "chunked"sv)) {
            state_ = State::InvalidResponseError;
            return;
        }
        state_ = State::ReadingChunks;
        ProcessChunks();
        return;
    }

    // Look for Content-Length header
    if (parsedHeader_.ContentLength != nullptr) {
        auto contentLength = std::string{parsedHeader_.ContentLength->value};
        try {
            contentLength_ = std::stoul(contentLength);
        } catch (const std::invalid_argument&) {
            state_ = State::InvalidResponseError;
            return;
        } catch (const std::out_of_range&) {
            state_ = State::InvalidResponseError;
            return;
        }

        if (reqOptions_.maxResponseSize > 0 && contentLength_ > reqOptions_.maxResponseSize) {
            // Response is too big
            spdlog::debug("content-length {} for response {} exceeds max response size", contentLength_, url_.url);
            state_ = State::ResponseTooBigError;
            return;
        }

        buffer_.reserve(buffer_.size() + contentLength_);
        body_.reserve(contentLength_);
    } else {
        // No Content-Length or Transfer-Encoding chunked
        state_ = State::InvalidResponseError;
        return;
    }

    state_ = State::ReadingBody;
    ProcessBody();  // Process any body data we already have
}

bool Connection::ValidateHeaders(const ResponseHeader& headers) {
    // Check Content-Type if specified in options AND 2xx status code
    if (!reqOptions_.allowedMimeType.empty() && headers.status / 100 == 2) {
        if (headers.ContentType == nullptr) {
            spdlog::debug("content-type <none> for response {} is not acceptable", url_.url);
            state_ = State::ResponseWrongType;
            return false;
        }

        auto contentType = headers.ContentType->value;
        auto anyMatch =
            std::any_of(reqOptions_.allowedMimeType.begin(),
                        reqOptions_.allowedMimeType.end(),
                        [contentType](std::string_view mimeType) { return ContentTypeMatches(contentType, mimeType); });
        if (!anyMatch) {
            spdlog::debug("content-type {} for response {} is not acceptable", contentType, url_.url);
            state_ = State::ResponseWrongType;
            return false;
        }
    }

    // Check Content-Language if specified in options
    if (!reqOptions_.allowedContentLanguage.empty()) {
        if (headers.ContentLanguage != nullptr) {
            auto contentLanguage = headers.ContentLanguage->value;
            auto anyMatch = std::any_of(
                reqOptions_.allowedContentLanguage.begin(),
                reqOptions_.allowedContentLanguage.end(),
                [contentLanguage](std::string_view lang) { return ContentLanguageMatches(contentLanguage, lang); });
            if (!anyMatch) {
                spdlog::debug("content-language {} for response {} is not acceptable", contentLanguage, url_.url);
                state_ = State::ResponseWrongLanguage;
                return false;
            }
        }
    }

    return true;
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
                std::search(buffer_.begin() + headersLength_ + bodyBytesRead_, buffer_.end(), CRLF.begin(), CRLF.end());

            if (chunkHeaderEnd == buffer_.end()) {
                return;  // Don't have complete chunk header
            }

            auto chunkHeader = std::string{buffer_.begin() + headersLength_ + bodyBytesRead_, chunkHeaderEnd};
            try {
                currentChunkSize_ = std::stoul(chunkHeader, nullptr, 16);
            } catch (const std::invalid_argument&) {
                state_ = State::InvalidResponseError;
                return;
            } catch (const std::out_of_range&) {
                state_ = State::InvalidResponseError;
                return;
            }

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
