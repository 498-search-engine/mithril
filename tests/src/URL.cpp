#include "http/URL.h"

#include <string_view>
#include <gtest/gtest.h>

using namespace mithril::http;
using namespace std::string_view_literals;

TEST(URL, ParseValid) {
    // Basic HTTP URL
    {
        auto result = ParseURL("http://example.com"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "http");
        EXPECT_EQ(result->host, "example.com");
        EXPECT_EQ(result->port, "");
        EXPECT_EQ(result->path, "");
    }

    // HTTPS with port
    {
        auto result = ParseURL("https://localhost:8080"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "https");
        EXPECT_EQ(result->host, "localhost");
        EXPECT_EQ(result->port, "8080");
        EXPECT_EQ(result->path, "");
    }

    // Complex path
    {
        auto result = ParseURL("https://api.example.com/v1/users/123"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "https");
        EXPECT_EQ(result->host, "api.example.com");
        EXPECT_EQ(result->port, "");
        EXPECT_EQ(result->path, "/v1/users/123");
    }

    // Path with query parameters
    {
        auto result = ParseURL("http://example.com/search?q=test&page=1"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "http");
        EXPECT_EQ(result->host, "example.com");
        EXPECT_EQ(result->port, "");
        EXPECT_EQ(result->path, "/search?q=test&page=1");
    }

    // IPv4 address as host
    {
        auto result = ParseURL("http://127.0.0.1:8080/status"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "http");
        EXPECT_EQ(result->host, "127.0.0.1");
        EXPECT_EQ(result->port, "8080");
        EXPECT_EQ(result->path, "/status");
    }
}

TEST(URL, ParseInvalid) {
    // Empty URL
    EXPECT_FALSE(ParseURL(""sv).has_value());

    // Missing scheme
    EXPECT_FALSE(ParseURL("example.com"sv).has_value());

    // Invalid scheme (contains invalid characters)
    EXPECT_FALSE(ParseURL("ht@tp://example.com"sv).has_value());

    // Missing host
    EXPECT_FALSE(ParseURL("http://"sv).has_value());

    // Invalid port (non-numeric)
    EXPECT_FALSE(ParseURL("http://example.com:abc"sv).has_value());

    // Invalid port (out of range)
    EXPECT_FALSE(ParseURL("http://example.com:65536"sv).has_value());
    EXPECT_FALSE(ParseURL("http://tel:8883719655"sv).has_value());

    // Invalid characters in host
    EXPECT_FALSE(ParseURL("http://exam<>ple.com"sv).has_value());

    // While trailing dots ARE VALID ACCORDING TO THE RFC,
    // we choose not to handle them.
    // https://daniel.haxx.se/blog/2022/05/12/a-tale-of-a-trailing-dot/
    EXPECT_FALSE(ParseURL("http://example.com."sv).has_value());

    // Unsupported scheme
    EXPECT_FALSE(ParseURL("ftp://ftp.example.com:21/pub/file.txt"sv).has_value());
}

TEST(URL, ParseEdgeCases) {
    // Empty path
    {
        auto result = ParseURL("http://example.com"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "");
    }

    // No / with query
    {
        auto result = ParseURL("http://example.com?thing=123"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->host, "example.com");
        EXPECT_EQ(result->path, "?thing=123");
    }

    // Root path
    {
        auto result = ParseURL("http://example.com/"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/");
    }

    // URL with fragments
    {
        auto result = ParseURL("http://example.com/page#section1"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/page#section1");
    }

    // URL with special characters in path
    {
        auto result = ParseURL("http://example.com/path%20with%20spaces"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/path%20with%20spaces");
    }

    // Maximum port number
    {
        auto result = ParseURL("http://example.com:65535"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->port, "65535");
    }

    // Complex subdomain
    {
        auto result = ParseURL("https://sub1.sub2.sub3.example.com"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->host, "sub1.sub2.sub3.example.com");
    }

    // Normalized scheme
    {
        auto result = ParseURL("HTTPS://example.com"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "https");
    }
}

TEST(URL, CanonicalizeHost) {
    // Basic canonicalization
    {
        auto url = ParseURL("Https://GitHub.COM/dnsge?achievement=arctic#section"sv);
        ASSERT_TRUE(url.has_value());

        auto canonical = CanonicalizeHost(*url);
        EXPECT_EQ(canonical.url, "https://github.com");
        EXPECT_EQ(canonical.scheme, "https");
        EXPECT_EQ(canonical.host, "github.com");
        EXPECT_EQ(canonical.port, "");
    }

    // Non-standard port
    {
        auto url = ParseURL("https://github.com:80/dnsge?achievement=arctic#section"sv);
        ASSERT_TRUE(url.has_value());

        auto canonical = CanonicalizeHost(*url);
        EXPECT_EQ(canonical.url, "https://github.com:80");
        EXPECT_EQ(canonical.scheme, "https");
        EXPECT_EQ(canonical.host, "github.com");
        EXPECT_EQ(canonical.port, "80");
    }
}
