#include "http/Response.h"

#include "Util.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mithril::http {

namespace {

void PopulateHeaderFields(ResponseHeader& h) {
    using namespace std::string_view_literals;
    for (auto& header : h.headers) {
        if (InsensitiveStrEquals(header.name, "Content-Language"sv)) {
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

}  // namespace

Response::Response(std::vector<char> header, std::vector<char> body, ResponseHeader parsedHeader)
    : headerData(std::move(header)), body(std::move(body)), header(std::move(parsedHeader)) {}

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
    if (!std::isdigit(header[9]) || !std::isdigit(header[10]) || !std::isdigit(header[11])) {
        return std::nullopt;
    }

    uint16_t status = (header[9] - '0') * 100 + (header[10] - '0') * 10 + (header[11] - '0');
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
        size_t headerEnd = header.find("\r\n", headerStart);
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

}  // namespace mithril::http
