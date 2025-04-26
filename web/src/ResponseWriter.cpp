#include "ResponseWriter.h"

#include "http/Response.h"

#include <cerrno>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>

namespace mithril {

using namespace std::string_view_literals;

namespace {

constexpr std::string_view CRLF = "\r\n"sv;

bool SendOnSocket(int sock, std::string_view data) {
    if (data.empty()) {
        return true;
    }

    off_t offset = 0;
    while (offset < data.size()) {
        ssize_t sent = send(sock, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
        if (sent <= 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;  // Retry on interrupt or would block
            }
            return false;
        }
        offset += sent;
    }
    return true;
}

}  // namespace

ResponseWriter::ResponseWriter(int sock) : sock_(sock), done_(false) {}

bool ResponseWriter::WriteResponse(http::StatusCode status, std::string_view contentType, std::string_view body) {
    if (done_) {
        return false;
    }

    done_ = true;

    std::string buf;
    buf.append("HTTP/1.1 "sv);
    buf.append(std::to_string(status));
    buf.push_back(' ');
    buf.append(http::StatusText(status));
    buf.append(CRLF);

    // Connection header
    buf.append("Connection: close"sv);
    buf.append(CRLF);

    if (!contentType.empty()) {
        // Content-Type header
        buf.append("Content-Type: "sv);
        buf.append(contentType);
        buf.append(CRLF);
    }

    // Content-Length header
    buf.append("Content-Length: "sv);
    buf.append(std::to_string(body.size()));
    buf.append(CRLF);

    // End of header
    buf.append(CRLF);

    // Send header
    if (!SendOnSocket(sock_, buf)) {
        return false;
    }

    // Send body
    if (!SendOnSocket(sock_, body)) {
        return false;
    }
    return true;
}

std::optional<ChunkWriter> ResponseWriter::BeginChunked(http::StatusCode status, std::string_view contentType) {
    if (done_) {
        return std::nullopt;
    }

    std::string buf;
    buf.append("HTTP/1.1 "sv);
    buf.append(std::to_string(status));
    buf.push_back(' ');
    buf.append(http::StatusText(status));
    buf.append(CRLF);

    // Connection header
    buf.append("Connection: close"sv);
    buf.append(CRLF);
    // Content-Type header
    buf.append("Content-Type: "sv);
    buf.append(contentType);
    buf.append(CRLF);
    // Transfer-Encoding header
    buf.append("Transfer-Encoding: chunked"sv);
    buf.append(CRLF);
    // Header end
    buf.append(CRLF);

    bool ok = SendOnSocket(sock_, buf);
    done_ = true;
    if (!ok) {
        return std::nullopt;
    }

    return ChunkWriter{sock_};
}

ChunkWriter::ChunkWriter(int sock) : sock_(sock), done_(false) {}

ChunkWriter::~ChunkWriter() {
    if (!done_) {
        Finish();
    }
}

ChunkWriter::ChunkWriter(ChunkWriter&& other) noexcept : sock_(other.sock_), done_(other.done_) {
    other.sock_ = -1;
    other.done_ = true;
}

ChunkWriter& ChunkWriter::operator=(ChunkWriter&& other) noexcept {
    if (!done_) {
        Finish();
    }
    sock_ = other.sock_;
    done_ = other.done_;
    other.sock_ = -1;
    other.done_ = true;
    return *this;
}


bool ChunkWriter::WriteChunk(std::string_view data) {
    if (done_) {
        return false;
    }

    if (data.empty()) {
        // Final chunk
        bool ok = SendOnSocket(sock_, "0\r\n\r\n"sv);
        done_ = true;
        return ok;
    }

    // Format chunk size
    char sizeBuf[32];
    int sz = snprintf(sizeBuf, sizeof(sizeBuf), "%zx\r\n", data.size());

    // Send the chunk header
    if (!SendOnSocket(sock_, std::string_view{sizeBuf, static_cast<size_t>(sz)})) {
        done_ = true;
        return false;
    }

    // Send the actual data
    if (!SendOnSocket(sock_, data)) {
        done_ = true;
        return false;
    }

    // Send the chunk terminator
    if (!SendOnSocket(sock_, "\r\n")) {
        done_ = true;
        return false;
    }
    return true;
}

bool ChunkWriter::Finish() {
    return WriteChunk(""sv);
}

}  // namespace mithril
