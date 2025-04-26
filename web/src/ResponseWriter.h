#ifndef WEB_RESPONSEWRITER_H
#define WEB_RESPONSEWRITER_H

#include "http/Response.h"

#include <optional>
#include <string_view>

namespace mithril {

class ChunkWriter;

class ResponseWriter {
public:
    ResponseWriter(int sock);

    bool WriteResponse(http::StatusCode status, std::string_view contentType, std::string_view body);
    std::optional<ChunkWriter> BeginChunked(http::StatusCode status, std::string_view contentType);

private:
    int sock_;
    bool done_;
};

class ChunkWriter {
public:
    ~ChunkWriter();

    ChunkWriter(const ChunkWriter&) = delete;
    ChunkWriter& operator=(const ChunkWriter&) = delete;

    ChunkWriter(ChunkWriter&&) noexcept;
    ChunkWriter& operator=(ChunkWriter&&) noexcept;

    bool WriteChunk(std::string_view data);
    bool Finish();

private:
    friend class ResponseWriter;
    ChunkWriter(int sock);

    int sock_;
    bool done_;
};

}  // namespace mithril

#endif
