#ifndef COMMON_HTTP_PARSEDURL_H
#define COMMON_HTTP_PARSEDURL_H

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <cassert>
#include <iostream>

namespace mithril::http {

struct URL {
    std::string url;
    std::string service;  
    std::string host;    
    std::string port;
    std::string path;  
    bool valid = false;
    
    explicit operator bool() const { return valid; }
};

// helpers for host validation
namespace {
    bool IsAlnum(char c) {
        return std::isalnum(c) != 0;
    }

    bool IsValidDomainLabel(std::string_view label) {
        if(label.empty() || label.size() > 63) return false;
        if(label.front() == '-' || label.back() == '-') return false;
        
        for(char c : label) {
            if(!IsAlnum(c) && c != '-') return false;
        }
        return true;
    }

    bool IsValidDomain(std::string_view host) {
        if(host.empty() || host.size() > 253) return false;
        if(host.front() == '.' || host.back() == '.') return false;

        size_t start = 0;
        while(start < host.size()) {
            size_t dot = host.find('.', start);
            std::string_view label = host.substr(start, dot - start);
            
            if(!IsValidDomainLabel(label)) return false;
            
            start = (dot != std::string_view::npos) ? dot + 1 : host.size();
        }
        return true;
    }

    bool IsValidIPv6(std::string_view host) {
        if(host.size() < 3) return false;
        if(host.front() != '[' || host.back() != ']') return false;

        std::string_view inner = host.substr(1, host.size() - 2);
        bool has_colon = false;
        
        for(char c : inner) {
            if(!std::isxdigit(c) && c != ':') return false;
            if(c == ':') has_colon = true;
        }
        return has_colon;  // Minimal IPv6 validation
    }
}

inline URL ParseURL(std::string_view url_view) {
    URL u;
    u.url = url_view;
    std::string_view uv = u.url;
    const size_t size = uv.size();
    
    try {
        // --- Scheme validation ---
        size_t scheme_end = uv.find(':');
        if(scheme_end == std::string_view::npos || scheme_end == 0) {
            throw std::invalid_argument("Missing or invalid scheme");
        }
        
        u.service = uv.substr(0, scheme_end);
        std::transform(u.service.begin(), u.service.end(), u.service.begin(),
            [](unsigned char c) { return std::tolower(c); });
            
        if(u.service != "http" && u.service != "https") {
            throw std::invalid_argument("Unsupported scheme: " + u.service);
        }

        // --- Authority validation ---
        size_t i = scheme_end + 1;
        size_t authority_start = i;
        
        // Require // for net-paths (RFC 3986 section 3)
        if(i+1 < size && uv[i] == '/' && uv[i+1] == '/') {
            i += 2;
            authority_start = i;
        } else if(u.service == "http" || u.service == "https") {
            throw std::invalid_argument("Missing authority component");
        }

        // --- Host validation ---
        size_t host_end = authority_start;
        while(host_end < size && uv[host_end] != ':' && uv[host_end] != '/' &&
              uv[host_end] != '?' && uv[host_end] != '#') {
            host_end++;
        }
        
        u.host = uv.substr(authority_start, host_end - authority_start);
        if(u.host.empty()) {
            throw std::invalid_argument("Empty host");
        }

        const bool is_valid_host = IsValidDomain(u.host) || IsValidIPv6(u.host);
        if(!is_valid_host) {
            throw std::invalid_argument("Invalid host: " + u.host);
        }

        // --- Port validation ---
        i = host_end;
        if(i < size && uv[i] == ':') {
            i++;
            size_t port_start = i;
            while(i < size && uv[i] != '/' && uv[i] != '?' && uv[i] != '#') i++;
            
            u.port = uv.substr(port_start, i - port_start);
            if(u.port.empty()) {
                throw std::invalid_argument("Empty port");
            }
            
            if(!std::all_of(u.port.begin(), u.port.end(), ::isdigit)) {
                throw std::invalid_argument("Non-numeric port: " + u.port);
            }
            
            const int port_num = std::stoi(u.port);
            if(port_num < 1 || port_num > 65535) {
                throw std::invalid_argument("Port out of range: " + u.port);
            }
        }

        // --- Path validation ---
        size_t path_start = i;
        size_t path_end = std::min({
            uv.find('?', path_start),
            uv.find('#', path_start),
            size
        });
        
        u.path = uv.substr(path_start, path_end - path_start);
        if(u.path.empty() || u.path[0] != '/') {
            u.path.insert(0, 1, '/');
        }

        u.valid = true;
    }
    catch(const std::exception& e) {
        #ifndef NDEBUG
        std::cerr << "URL parse error: " << e.what() << " in " << uv << "\n";
        #endif
    }
    
    return u;
}

inline std::string NormalizeURL(const URL& url) {
    std::string normalized;
    normalized.reserve(url.url.size());

    // Lowercase service and host
    std::string service = url.service;
    std::transform(service.begin(), service.end(), service.begin(),
        [](unsigned char c) { return std::tolower(c); });
    
    std::string host = url.host;
    std::transform(host.begin(), host.end(), host.begin(),
        [](unsigned char c) { return std::tolower(c); });

    normalized += service + "://" + host;

    // Add non-default port
    if (!url.port.empty() &&
        !((service == "http" && url.port == "80") ||
          (service == "https" && url.port == "443"))) {
        normalized += ":" + url.port;
    }

    // Normalize path
    std::string clean_path;
    clean_path.reserve(url.path.size());
    bool prev_slash = false;
    
    // Ensure leading slash and collapse duplicates
    if (url.path.empty() || url.path[0] != '/') {
        clean_path += '/';
        prev_slash = true;
    }
    
    for (char c : url.path) {
        if (c == '/') {
            if (!prev_slash) {
                clean_path += '/';
                prev_slash = true;
            }
        } else {
            clean_path += c;
            prev_slash = false;
        }
    }

    // Handle trailing slash for empty paths
    if (clean_path.empty() || (clean_path.size() == 1 && clean_path[0] == '/')) {
        normalized += "/";
    } else {
        normalized += clean_path;
    }

    return normalized;
}

// Runtime test cases (replace static_assert)
inline void TestURLParsing() {
    auto test1 = ParseURL("https://GitHub.COM/dnsge?achievement=arctic#section");
    assert(test1.service == "https");
    assert(test1.host == "GitHub.COM");
    assert(test1.path == "/dnsge");
    assert(NormalizeURL(test1) == "https://github.com/dnsge");

    auto test2 = ParseURL("http://example.com:8080//a//b/../c");
    assert(test2.port == "8080");
    assert(test2.path == "//a//b/../c");
    assert(NormalizeURL(test2) == "http://example.com:8080/a/b/../c");
}

} // namespace mithril::http

#endif