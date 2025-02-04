#include "http/Connection.h"

#include "http/Response.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace mithril::http {

namespace {

constexpr size_t BufferSize = 8192;
constexpr const char* ContentLengthHeader = "Content-Length: ";
constexpr const char* TransferEncodingHeader = "Transfer-Encoding: chunked";
constexpr const char* HeaderDelimiter = "\r\n\r\n";
constexpr const char* CRLF = "\r\n";

}  // namespace

Connection Connection::NewWithRequest(const Request& req) {
    assert(req.Url().service == "http");

    struct addrinfo* address;
    struct addrinfo hints {};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int status = getaddrinfo(req.Url().host.c_str(), req.Url().port.c_str(), &hints, &address);
    assert(status != -1);

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(fd != -1);

    status = connect(fd, address->ai_addr, address->ai_addrlen);
    assert(status != -1);
    freeaddrinfo(address);

    // Construct request
    std::string rawRequest = BuildRawRequestString(req);
    send(fd, rawRequest.c_str(), rawRequest.length(), 0);
    return Connection{fd};
}

Connection::Connection(int fd)
    : fd_(fd),
      state_(State::Pending),
      contentLength_(0),
      headersLength_(0),
      bodyBytesRead_(0),
      isChunked_(false),
      currentChunkSize_(0),
      currentChunkBytesRead_(0) {}

Connection::~Connection() {
    Close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_),
      state_(other.state_),
      contentLength_(other.contentLength_),
      headersLength_(other.headersLength_),
      bodyBytesRead_(other.bodyBytesRead_),
      isChunked_(other.isChunked_),
      currentChunkSize_(other.currentChunkSize_),
      currentChunkBytesRead_(other.currentChunkBytesRead_),
      buffer_(std::move(other.buffer_)),
      body_(std::move(other.body_)) {
    other.fd_ = -1;
    other.state_ = State::Consumed;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    Close();

    fd_ = other.fd_;
    state_ = other.state_;
    contentLength_ = other.contentLength_;
    headersLength_ = other.headersLength_;
    bodyBytesRead_ = other.bodyBytesRead_;
    isChunked_ = other.isChunked_;
    currentChunkSize_ = other.currentChunkSize_;
    currentChunkBytesRead_ = other.currentChunkBytesRead_;
    buffer_ = std::move(other.buffer_);
    body_ = std::move(other.body_);

    other.fd_ = -1;
    other.state_ = State::Consumed;
}

int Connection::SocketDescriptor() const {
    return fd_;
}

bool Connection::Ready() const {
    return state_ == State::Complete;
}

void Connection::Close() {
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
        bytesRead = recv(fd_, tempBuffer, BufferSize, MSG_DONTWAIT);
        if (bytesRead > 0) {
            buffer_.insert(buffer_.end(), tempBuffer, tempBuffer + bytesRead);
        } else if (bytesRead == 0) {
            // Got EOF
            return true;
        } else if (bytesRead == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
    if (state_ == State::Complete) {
        return;
    }

    // Read data from socket
    gotEof |= ReadFromSocket();

    // Process based on current state
    switch (state_) {
    case State::Pending:
        state_ = State::ReadingHeaders;
        [[fallthrough]];

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

    if (gotEof) {
        if ((state_ == State::ReadingBody && bodyBytesRead_ < contentLength_) ||
            (state_ == State::ReadingChunks && currentChunkSize_ != 0)) {
            state_ = State::Closed;
            // TODO: error handling strategy
            throw std::runtime_error("Connection closed before receiving complete response");
        } else {
            state_ = State::Complete;
        }
    }
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
    if (headers.find(TransferEncodingHeader) != std::string::npos) {
        isChunked_ = true;
        state_ = State::ReadingChunks;
        ProcessChunks();
        return;
    }

    // Look for Content-Length header
    auto contentLengthPos = headers.find(ContentLengthHeader);
    if (contentLengthPos != std::string::npos) {
        contentLengthPos += strlen(ContentLengthHeader);
        auto lengthEnd = headers.find(CRLF, contentLengthPos);
        contentLength_ = std::stoul(headers.substr(contentLengthPos, lengthEnd - contentLengthPos));
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
