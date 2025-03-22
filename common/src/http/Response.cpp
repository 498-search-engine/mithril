#include "http/Response.h"

#include "Util.h"
#include "data/Gzip.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mithril::http {

using namespace std::string_view_literals;

namespace {

void PopulateHeaderFields(ResponseHeader& h) {
    using namespace std::string_view_literals;
    for (auto& header : h.headers) {
        if (InsensitiveStrEquals(header.name, "Content-Encoding"sv)) {
            h.ContentEncoding = &header;
        } else if (InsensitiveStrEquals(header.name, "Content-Language"sv)) {
            h.ContentLanguage = &header;
        } else if (InsensitiveStrEquals(header.name, "Content-Length"sv)) {
            h.ContentLength = &header;
        } else if (InsensitiveStrEquals(header.name, "Content-Type"sv)) {
            h.ContentType = &header;
        } else if (InsensitiveStrEquals(header.name, "Location"sv)) {
            h.Location = &header;
        } else if (InsensitiveStrEquals(header.name, "Transfer-Encoding"sv)) {
            h.TransferEncoding = &header;
        }
    }
}

bool IsDigitSafe(int c) {
    return c >= 0 && c <= 127 && std::isdigit(c);
}

}  // namespace

Response::Response(std::vector<char> header, std::vector<char> body, ResponseHeader parsedHeader)
    : headerData(std::move(header)), body(std::move(body)), header(std::move(parsedHeader)), decoded_(false) {}

void Response::DecodeBody() {
    if (decoded_) {
        return;
    }

    if (header.ContentEncoding == nullptr) {
        // No Content-Encoding
        decoded_ = true;
        return;
    }

    if (header.ContentEncoding->value == "gzip"sv) {
        auto unzipped = data::Gunzip(body);
        body = std::move(unzipped);
    } else if (header.ContentEncoding->value == "none"sv || header.ContentEncoding->value == "identity"sv) {
        // Nothing to do
    } else {
        spdlog::error("got unsupported Content-Encoding {}", header.ContentEncoding->value);
        throw std::runtime_error("unsupported Content-Encoding");
    }

    decoded_ = true;
}

std::optional<ResponseHeader> ParseResponseHeader(std::string_view header) {
    ResponseHeader parsed;

    // Need at least "HTTP/1.x XXX" (where X is any digit)
    if (header.size() < 12) {
        return std::nullopt;
    }

    // Validate HTTP version
    if (header[0] != 'H' || header[1] != 'T' || header[2] != 'T' || header[3] != 'P' || header[4] != '/' ||
        header[5] != '1' || header[6] != '.' || !std::isdigit(header[7]) || header[8] != ' ') {
        return std::nullopt;
    }

    // Parse status code
    if (!IsDigitSafe(header[9]) || !IsDigitSafe(header[10]) || !IsDigitSafe(header[11])) {
        return std::nullopt;
    }

    uint16_t status = ((header[9] - '0') * 100) + ((header[10] - '0') * 10) + (header[11] - '0');
    parsed.status = static_cast<StatusCode>(status);

    // Parse headers
    size_t pos = header.find("\r\n");
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    size_t headerStart = pos + 2;
    while (headerStart < header.size()) {
        // Check for end of headers
        if (headerStart + 2 <= header.size() && header[headerStart] == '\r' && header[headerStart + 1] == '\n') {
            break;
        }

        // Find end of current header
        size_t headerEnd = header.find("\r\n"sv, headerStart);
        if (headerEnd == std::string::npos) {
            return std::nullopt;
        }

        auto headerView = header.substr(headerStart, headerEnd - headerStart);

        // Find colon separator
        size_t colonPos = headerView.find(':');
        if (colonPos == std::string::npos) {
            return std::nullopt;
        }

        // Consume spaces and tabs to start of value
        size_t valPos = colonPos + 1;
        while (valPos < headerView.size() && (headerView[valPos] == ' ' || headerView[valPos] == '\t')) {
            ++valPos;
        }

        // Create header entry
        Header header;
        header.name = headerView.substr(0, colonPos);
        header.value = headerView.substr(valPos, headerView.size() - valPos);

        parsed.headers.push_back(header);
        headerStart = headerEnd + 2;
    }

    PopulateHeaderFields(parsed);
    return parsed;
}

bool ContentTypeMatches(std::string_view val, std::string_view mimeType) {
    auto semicolonPos = val.find(';');
    std::string_view headerMimeType = val.substr(0, semicolonPos);

    // Trim trailing whitespace
    while (!headerMimeType.empty() && std::isspace(static_cast<unsigned char>(headerMimeType.back()))) {
        headerMimeType.remove_suffix(1);
    }

    return InsensitiveStrEquals(headerMimeType, mimeType);
}

bool ContentLanguageMatches(std::string_view val, std::string_view lang) {
    if (lang.empty()) {
        return true;
    }

    auto semiPos = val.find(';');
    if (semiPos != std::string_view::npos) {
        val = val.substr(0, semiPos);
    }

    if (lang.back() == '*') {
        lang.remove_suffix(1);
        return InsensitiveStartsWith(val, lang);
    } else {
        return InsensitiveStrEquals(val, lang);
    }
}

}  // namespace mithril::http
