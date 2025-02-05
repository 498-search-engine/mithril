#include "http/Connection.h"

#include "http/Request.h"
#include "http/Response.h"
#include "http/SSL.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iterator>
#include <netdb.h>
#include <stdexcept>
#include <unistd.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace mithril::http {

namespace {

constexpr size_t BufferSize = 8192;
constexpr const char* ContentLengthHeader = "Content-Length: ";
constexpr const char* TransferEncodingChunkedHeader = "Transfer-Encoding: chunked";
constexpr const char* HeaderDelimiter = "\r\n\r\n";
constexpr const char* CRLF = "\r\n";

size_t FindCaseInsensitive(const std::string& s, const char* q) {
    auto len = std::strlen(q);
    auto it = std::search(s.begin(), s.end(), q, q + len, [](unsigned char a, unsigned char b) -> bool {
        return std::tolower(a) == std::tolower(b);
    });
    if (it == s.end()) {
        return std::string::npos;
    } else {
        return std::distance(s.begin(), it);
    }
}

void PrintSSLConnectError(SSL* ssl, int status) {
    int sslErr = SSL_get_error(ssl, status);
    std::cerr << "SSL_connect failed with error code: " << sslErr << "\n";

    // Print the entire error queue
    unsigned long err;
    while ((err = ERR_get_error()) != 0) {
        char errBuf[256];
        ERR_error_string_n(err, errBuf, sizeof(errBuf));
        std::cerr << "SSL Error: " << errBuf << "\n";
    }

    // Print verification errors if any
    long verifyResult = SSL_get_verify_result(ssl);
    if (verifyResult != X509_V_OK) {
        std::cerr << "Certificate verification error: " << X509_verify_cert_error_string(verifyResult) << "\n";
    }

    // Print peer certificate info if available
    X509* cert = SSL_get_peer_certificate(ssl);
    if (cert != nullptr) {
        char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
        char* issuer = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
        std::cerr << "Peer certificate:\n";
        std::cerr << "  Subject: " << subject << "\n";
        std::cerr << "  Issuer: " << issuer << "\n";
        OPENSSL_free(subject);
        OPENSSL_free(issuer);
        X509_free(cert);
    }
}

}  // namespace

Connection Connection::NewWithRequest(const Request& req) {
    struct addrinfo* address;
    struct addrinfo hints {};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    bool isSecure = req.Url().service == "https";
    const char* port = req.Url().port.empty() ? (isSecure ? "443" : "80") : req.Url().port.c_str();

    int status = getaddrinfo(req.Url().host.c_str(), port, &hints, &address);
    assert(status != -1);

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(fd != -1);

    status = connect(fd, address->ai_addr, address->ai_addrlen);
    assert(status != -1);
    freeaddrinfo(address);
    return Connection{fd, req};
}

Connection::Connection(int fd, const Request& req)
    : fd_(fd),
      state_(State::Sending),
      rawRequest_(BuildRawRequestString(req)),
      requestBytesSent_(0),
      contentLength_(0),
      headersLength_(0),
      bodyBytesRead_(0),
      isChunked_(false),
      currentChunkSize_(0),
      currentChunkBytesRead_(0),
      ssl_(nullptr),
      isSecure_(req.Url().service == "https"),
      sni_(req.Url().host) {
    if (isSecure_) {
        InitializeSSL();
    }
}

Connection::~Connection() {
    Close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_),
      state_(other.state_),
      rawRequest_(std::move(other.rawRequest_)),
      requestBytesSent_(other.requestBytesSent_),
      contentLength_(other.contentLength_),
      headersLength_(other.headersLength_),
      bodyBytesRead_(other.bodyBytesRead_),
      isChunked_(other.isChunked_),
      currentChunkSize_(other.currentChunkSize_),
      currentChunkBytesRead_(other.currentChunkBytesRead_),
      buffer_(std::move(other.buffer_)),
      body_(std::move(other.body_)),
      ssl_(other.ssl_),
      isSecure_(other.isSecure_),
      sni_(std::move(other.sni_)) {
    other.fd_ = -1;
    other.state_ = State::Consumed;
    other.ssl_ = nullptr;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    Close();

    fd_ = other.fd_;
    state_ = other.state_;
    rawRequest_ = std::move(other.rawRequest_);
    requestBytesSent_ = other.requestBytesSent_;
    contentLength_ = other.contentLength_;
    headersLength_ = other.headersLength_;
    bodyBytesRead_ = other.bodyBytesRead_;
    isChunked_ = other.isChunked_;
    currentChunkSize_ = other.currentChunkSize_;
    currentChunkBytesRead_ = other.currentChunkBytesRead_;
    buffer_ = std::move(other.buffer_);
    body_ = std::move(other.body_);
    ssl_ = other.ssl_;
    isSecure_ = other.isSecure_;
    sni_ = std::move(other.sni_);

    other.fd_ = -1;
    other.state_ = State::Consumed;
    other.ssl_ = nullptr;

    return *this;
}

int Connection::SocketDescriptor() const {
    return fd_;
}

bool Connection::Ready() const {
    return state_ == State::Complete;
}

void Connection::InitializeSSL() {
    auto* ctx = http::internal::SSLCtx;
    assert(ctx != nullptr);

    ssl_ = SSL_new(ctx);
    if (ssl_ == nullptr) {
        // TODO: error handling strategy
        throw std::runtime_error("Failed to create SSL object");
    }

    int status = SSL_set_fd(ssl_, fd_);
    if (status != 1) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        // TODO: error handling strategy
        throw std::runtime_error("Failed to set SSL file descriptor");
    }

    // Set Server Name Indication (SNI)
    SSL_set_tlsext_host_name(ssl_, sni_.c_str());

    status = SSL_connect(ssl_);
    if (status != 1) {
        PrintSSLConnectError(ssl_, status);
        throw std::runtime_error("SSL handshake failed");
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
    state_ = State::Consumed;
    auto header = std::vector<char>{buffer_.begin(), buffer_.begin() + headersLength_};
    buffer_.clear();
    return Response{
        .header = std::move(header),
        .body = std::move(body_),
    };
}

bool Connection::ReadFromSocket() {
    char tempBuffer[BufferSize];
    ssize_t bytesRead;
    do {
        if (isSecure_) {
            bytesRead = SSL_read(ssl_, tempBuffer, BufferSize);
        } else {
            bytesRead = recv(fd_, tempBuffer, BufferSize, MSG_DONTWAIT);
        }

        if (bytesRead > 0) {
            buffer_.insert(buffer_.end(), tempBuffer, tempBuffer + bytesRead);
        } else if (bytesRead == 0) {
            // Got EOF
            return true;
        } else if (bytesRead < 0) {
            if (isSecure_) {
                int err = SSL_get_error(ssl_, bytesRead);
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    // No more data available right now
                    return false;
                }
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available right now
                return false;
            }
            // TODO: error handling strategy
            throw std::runtime_error("Error reading from socket");
        }
    } while (bytesRead > 0);
    return false;
}

void Connection::Process(bool gotEof) {
    if (state_ == State::Complete || state_ == State::Consumed || state_ == State::Closed) {
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
        if ((state_ == State::ReadingBody && bodyBytesRead_ < contentLength_) ||
            (state_ == State::ReadingChunks && currentChunkSize_ != 0)) {
            state_ = State::Closed;
            // TODO: error handling strategy
            throw std::runtime_error("Connection closed before receiving complete response");
        } else {
            state_ = State::Complete;
        }
        Close();
    }
}

bool Connection::ProcessSend() {
    ssize_t bytesSent;
    do {
        if (isSecure_) {
            bytesSent = SSL_write(ssl_, rawRequest_.data() + requestBytesSent_, rawRequest_.size() - requestBytesSent_);
        } else {
            bytesSent = send(fd_, rawRequest_.data(), rawRequest_.size() - requestBytesSent_, MSG_DONTWAIT);
        }

        if (bytesSent > 0) {
            requestBytesSent_ += bytesSent;
        } else if (bytesSent == 0) {
            // Got EOF
            return true;
        } else if (bytesSent < 0) {
            if (isSecure_) {
                int err = SSL_get_error(ssl_, bytesSent);
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    return false;
                }
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false;
            }
            // TODO: error handling strategy
            perror("HTTP Connection send request data");
        }
    } while (requestBytesSent_ < rawRequest_.size());

    // Request fully written, now into reading headers phase
    state_ = State::ReadingHeaders;

    return false;
}

bool Connection::ProcessReceive() {
    bool gotEof = ReadFromSocket();

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
        return;  // Haven't received full headers yet
    }

    // Headers are complete
    headersLength_ = headerEnd - buffer_.begin() + 4;  // Include delimiter
    auto headers = std::string{buffer_.begin(), headerEnd};

    // Check for chunked encoding
    if (FindCaseInsensitive(headers, TransferEncodingChunkedHeader) != std::string::npos) {
        isChunked_ = true;
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
        body_.reserve(contentLength_);
    }

    state_ = State::ReadingBody;
    ProcessBody();  // Process any body data we already have
}

void Connection::ProcessBody() {
    size_t availableBytes = buffer_.size() - headersLength_;
    size_t bytesToRead = std::min(availableBytes, contentLength_ - bodyBytesRead_);

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
                std::search(buffer_.begin() + headersLength_ + bodyBytesRead_, buffer_.end(), "\r\n", "\r\n" + 2);

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
