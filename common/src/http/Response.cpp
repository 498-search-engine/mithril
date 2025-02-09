#include "http/Response.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace mithril::http {

namespace {

bool InsensitiveCharEquals(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

bool InsensitiveStrEquals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), InsensitiveCharEquals);
}

void PopulateHeaderFields(ResponseHeader& h) {
    for (auto& header : h.headers) {
        if (InsensitiveStrEquals(header.name, "Content-Type")) {
            h.ContentType = &header;
        } else if (InsensitiveStrEquals(header.name, "Location")) {
            h.Location = &header;
        }
    }
}

}  // namespace

Response::Response(std::vector<char> header, std::vector<char> body)
    : header(std::move(header)), body(std::move(body)) {}

std::optional<ResponseHeader> ParseResponseHeader(const Response& res) {
    ResponseHeader parsed;

    // Need at least "HTTP/1.x XXX" (where X is any digit)
    if (res.header.size() < 12) {
        return std::nullopt;
    }

    // Validate HTTP version
    if (res.header[0] != 'H' || res.header[1] != 'T' || res.header[2] != 'T' || res.header[3] != 'P' ||
        res.header[4] != '/' || res.header[5] != '1' || res.header[6] != '.' || !std::isdigit(res.header[7]) ||
        res.header[8] != ' ') {
        return std::nullopt;
    }

    // Parse status code
    if (!std::isdigit(res.header[9]) || !std::isdigit(res.header[10]) || !std::isdigit(res.header[11])) {
        return std::nullopt;
    }

    uint16_t status = (res.header[9] - '0') * 100 + (res.header[10] - '0') * 10 + (res.header[11] - '0');

    parsed.status = static_cast<StatusCode>(status);


    // Parse headers
    auto rawHeaderView = std::string_view{res.header.data(), res.header.size()};
    size_t pos = rawHeaderView.find("\r\n");
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    size_t headerStart = pos + 2;
    while (headerStart < res.header.size()) {
        // Check for end of headers
        if (headerStart + 2 <= res.header.size() && res.header[headerStart] == '\r' &&
            res.header[headerStart + 1] == '\n') {
            break;
        }

        // Find end of current header
        size_t headerEnd = rawHeaderView.find("\r\n", headerStart);
        if (headerEnd == std::string::npos) {
            return std::nullopt;
        }

        auto headerView = rawHeaderView.substr(headerStart, headerEnd - headerStart);

        // Find colon separator
        size_t colonPos = headerView.find(": ");
        if (colonPos == std::string::npos) {
            return std::nullopt;
        }

        // Create header entry
        Header header;
        header.name = headerView.substr(0, colonPos);
        header.value = headerView.substr(colonPos + 2, headerView.size() - colonPos - 2);

        parsed.headers.push_back(header);
        headerStart = headerEnd + 2;
    }

    PopulateHeaderFields(parsed);
    return parsed;
}

}  // namespace mithril::http
