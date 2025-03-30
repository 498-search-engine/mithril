#include "http/URL.h"

#include <string>
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
        EXPECT_EQ(result->queryFragment, "");
    }

    // HTTPS with port
    {
        auto result = ParseURL("https://localhost:8080"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "https");
        EXPECT_EQ(result->host, "localhost");
        EXPECT_EQ(result->port, "8080");
        EXPECT_EQ(result->path, "");
        EXPECT_EQ(result->queryFragment, "");
    }

    // Complex path
    {
        auto result = ParseURL("https://api.example.com/v1/users/123"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "https");
        EXPECT_EQ(result->host, "api.example.com");
        EXPECT_EQ(result->port, "");
        EXPECT_EQ(result->path, "/v1/users/123");
        EXPECT_EQ(result->queryFragment, "");
    }

    // Path with query parameters
    {
        auto result = ParseURL("http://example.com/search?q=test&page=1"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "http");
        EXPECT_EQ(result->host, "example.com");
        EXPECT_EQ(result->port, "");
        EXPECT_EQ(result->path, "/search?q=test&page=1");
        EXPECT_EQ(result->queryFragment, "?q=test&page=1");
        EXPECT_EQ(result->BasePath(), "/search");
    }

    // IPv4 address as host
    {
        auto result = ParseURL("http://127.0.0.1:8080/status"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->scheme, "http");
        EXPECT_EQ(result->host, "127.0.0.1");
        EXPECT_EQ(result->port, "8080");
        EXPECT_EQ(result->path, "/status");
        EXPECT_EQ(result->queryFragment, "");
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
        EXPECT_EQ(result->queryFragment, "");
    }

    // No / with query
    {
        auto result = ParseURL("http://example.com?thing=123"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->host, "example.com");
        EXPECT_EQ(result->path, "?thing=123");
        EXPECT_EQ(result->queryFragment, "?thing=123");
    }

    // Root path
    {
        auto result = ParseURL("http://example.com/"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/");
        EXPECT_EQ(result->queryFragment, "");
    }

    // URL with fragments
    {
        auto result = ParseURL("http://example.com/page#section1"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/page#section1");
        EXPECT_EQ(result->queryFragment, "#section1");
        EXPECT_EQ(result->BasePath(), "/page");
    }

    // URL with fragment before ?
    {
        auto result = ParseURL("http://example.com/page#section1?asdf=123"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/page#section1?asdf=123");
        EXPECT_EQ(result->queryFragment, "#section1?asdf=123");
        EXPECT_EQ(result->BasePath(), "/page");
    }

    // URL with special characters in path
    {
        auto result = ParseURL("http://example.com/path%20with%20spaces"sv);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->path, "/path%20with%20spaces");
        EXPECT_EQ(result->queryFragment, "");
        EXPECT_EQ(result->BasePath(), "/path%20with%20spaces");
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

// Basic path encoding tests
TEST(URLEncodingTest, BasicPathEncoding) {
    // Simple paths
    EXPECT_EQ("/simple/path", EncodePath("/simple/path"));

    // Spaces in path
    EXPECT_EQ("/test%20path/file", EncodePath("/test path/file"));

    // Mixed path with special characters
    EXPECT_EQ("/user/john_doe/profile.html", EncodePath("/user/john_doe/profile.html"));

    // Path with reserved characters
    EXPECT_EQ("/path?with%3Aspecial=chars", EncodePath("/path?with:special=chars"));
}

// Query parameter tests
TEST(URLEncodingTest, QueryParameterEncoding) {
    // Path with query
    EXPECT_EQ("/search?query=test%20value&page=2", EncodePath("/search?query=test value&page=2"));

    // Multiple parameters
    EXPECT_EQ("/api/v1?name=John%20Doe&role=admin&active=true",
              EncodePath("/api/v1?name=John Doe&role=admin&active=true"));

    // Special characters in query
    EXPECT_EQ("/products?filter=%3C%3E&sort=price", EncodePath("/products?filter=<>&sort=price"));

    // Question mark in query value
    EXPECT_EQ("/search?q=what%3F&when=now", EncodePath("/search?q=what?&when=now"));

    // Fragment
    EXPECT_EQ("/search#hello%20world&123", EncodePath("/search#hello world&123"));
}

// Special character tests
TEST(URLEncodingTest, SpecialCharacters) {
    // Control characters
    EXPECT_EQ("control%09char%0A", EncodePath("control\tchar\n"));

    // International characters
    EXPECT_EQ("caf%C3%A9/%E2%82%AC", EncodePath("café/€"));

    // Reserved characters per RFC 3986
    EXPECT_EQ("%21#%24%25&%27%28%29%2A%2B%2C%3B=", EncodePath("!#$%&'()*+,;="));
}

// Edge cases
TEST(URLEncodingTest, EdgeCases) {
    // Empty string
    EXPECT_EQ("", EncodePath(""));

    // Single character tests
    EXPECT_EQ("%20", EncodePath(" "));
    EXPECT_EQ("/", EncodePath("/"));
    EXPECT_EQ("/?", EncodePath("/?"));
    EXPECT_EQ("/?%3F", EncodePath("/??"));

    // Percent sign
    EXPECT_EQ("%25", EncodePath("%"));
    EXPECT_EQ("%25%25", EncodePath("%%"));

    // Trailing slash
    EXPECT_EQ("/path/", EncodePath("/path/"));
}

// Basic decoding tests
TEST(URLDecodingTest, BasicDecoding) {
    // Simple encoded strings
    EXPECT_EQ("Hello World", DecodeURL("Hello%20World"));
    EXPECT_EQ("/test path/file", DecodeURL("/test%20path/file"));

    // International characters
    EXPECT_EQ("café/€", DecodeURL("caf%C3%A9/%E2%82%AC"));
}

// Special decoding cases
TEST(URLDecodingTest, SpecialCases) {
    // Percent sign
    EXPECT_EQ("%", DecodeURL("%25"));

    // Incomplete encoding
    EXPECT_EQ("%5", DecodeURL("%5"));
    EXPECT_EQ("%", DecodeURL("%"));

    // Invalid hex in encoding
    EXPECT_EQ("%XY", DecodeURL("%XY"));

    // Reserved character
    EXPECT_EQ("%2B", DecodeURL("%2B"));

    // Don't decode reserved characters
    EXPECT_EQ("example.com/folder%2Ffile.txt", DecodeURL("example.com/folder%2Ffile.txt"));
}

// Encode-decode consistency
TEST(URLRoundTripTest, EncodeDecodeConsistency) {
    // Simple string
    std::string original = "Hello World";
    EXPECT_EQ(original, DecodeURL(EncodePath(original)));

    // Complex path with reserved chars
    EXPECT_EQ(
        "/api/v1/users?name=John Doe&role=admin&test=<>&special=%3A%2F%3F%23%5B%5D%40%21%24&%27%28%29%2A%2B%2C%3B",
        DecodeURL(EncodePath("/api/v1/users?name=John Doe&role=admin&test=<>&special=:/?#[]@!$&'()*+,;")));

    // International characters
    original = "/café/München/北京/";
    EXPECT_EQ(original, DecodeURL(EncodePath(original)));
}

// Specific test cases for path encoding rules
TEST(URLEncodingTest, PathEncodingRules) {
    // Forward slashes preserved in path
    EXPECT_EQ("/a/b/c", EncodePath("/a/b/c"));

    // Forward slashes encoded in query
    EXPECT_EQ("/path?query=a%2Fb%2Fc", EncodePath("/path?query=a/b/c"));

    // Question marks
    EXPECT_EQ("/path?with%3Fquestions", EncodePath("/path?with?questions"));
    EXPECT_EQ("/legitimate?param%3Fwith%3Fquestions", EncodePath("/legitimate?param?with?questions"));
}

class CanonicalizeURLTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Any setup code if needed
    }

    void TearDown() override {
        // Any teardown code if needed
    }

    // Helper method to test canonicalization
    static void TestCanonicalizationImpl(const std::string& input,
                                         const std::string& expectedStr,
                                         const char* file = __FILE__,
                                         int line = __LINE__) {
        auto urlOpt = ParseURL(input);
        if (!urlOpt.has_value()) {
            ADD_FAILURE_AT(file, line) << "Failed to parse URL: " << input;
            return;
        }

        auto expected = ParseURL(expectedStr);
        if (!expected.has_value()) {
            ADD_FAILURE_AT(file, line) << "Failed to parse expected URL: " << expectedStr;
            return;
        }

        auto result = CanonicalizeURL(*urlOpt);
        if (result.url != expected->url) {
            ADD_FAILURE_AT(file, line) << "Input: " << input << "\nExpected url: " << expected->url
                                       << "\nActual url: " << result.url;
        }
        if (result.scheme != expected->scheme) {
            ADD_FAILURE_AT(file, line) << "Input: " << input << "\nExpected scheme: " << expected->scheme
                                       << "\nActual scheme: " << result.scheme;
        }
        if (result.host != expected->host) {
            ADD_FAILURE_AT(file, line) << "Input: " << input << "\nExpected host: " << expected->host
                                       << "\nActual host: " << result.host;
        }
        if (result.port != expected->port) {
            ADD_FAILURE_AT(file, line) << "Input: " << input << "\nExpected port: " << expected->port
                                       << "\nActual port: " << result.port;
        }
        if (result.path != expected->path) {
            ADD_FAILURE_AT(file, line) << "Input: " << input << "\nExpected path: " << expected->path
                                       << "\nActual path: " << result.path;
        }
        if (result.queryFragment != expected->queryFragment) {
            ADD_FAILURE_AT(file, line) << "Input: " << input << "\nExpected queryFragment: " << expected->queryFragment
                                       << "\nActual queryFragment: " << result.queryFragment;
        }
    }
};

#define TestCanonicalization(input, expected) TestCanonicalizationImpl(input, expected, __FILE__, __LINE__)

// Test that scheme and host are converted to lowercase
TEST_F(CanonicalizeURLTest, LowercaseSchemeAndHost) {
    TestCanonicalization("HTTP://ExAmPlE.CoM/path", "http://example.com/path");
    TestCanonicalization("HTTPS://TEST.example.ORG/", "https://test.example.org/");
}

// Test that standard ports are omitted
TEST_F(CanonicalizeURLTest, StandardPortsOmitted) {
    TestCanonicalization("http://example.com:80/path", "http://example.com/path");
    TestCanonicalization("https://example.com:443/path", "https://example.com/path");
}

// Test that non-standard ports are included
TEST_F(CanonicalizeURLTest, NonStandardPortsIncluded) {
    TestCanonicalization("http://example.com:8080/path", "http://example.com:8080/path");
    TestCanonicalization("https://example.com:8443/path", "https://example.com:8443/path");
}

// Test that consecutive slashes in path are collapsed
TEST_F(CanonicalizeURLTest, CollapseConsecutiveSlashes) {
    TestCanonicalization("http://example.com//path", "http://example.com/path");
    TestCanonicalization("http://example.com/path//to///resource", "http://example.com/path/to/resource");
    TestCanonicalization("http://example.com////", "http://example.com/");
}

// Test that fragments are removed
TEST_F(CanonicalizeURLTest, RemoveFragments) {
    TestCanonicalization("http://example.com/path#fragment", "http://example.com/path");
    TestCanonicalization("http://example.com/path?query=value#fragment", "http://example.com/path?query=value");
    TestCanonicalization("http://example.com/#fragment", "http://example.com/");
}

// Test that path always starts with /
TEST_F(CanonicalizeURLTest, PathStartsWithSlash) {
    TestCanonicalization("http://example.com", "http://example.com/");
    TestCanonicalization("http://example.com?query=value", "http://example.com/?query=value");
    TestCanonicalization("http://example.com#fragment", "http://example.com/");
}

// Test combinations of requirements
TEST_F(CanonicalizeURLTest, CombinedRequirements) {
    TestCanonicalization("HTTP://ExAmPlE.CoM:80//path/to///resource?query=value#fragment",
                         "http://example.com/path/to/resource?query=value");

    TestCanonicalization("HTTPS://TEST.example.ORG:443///path///?a=b#fragment", "https://test.example.org/path/?a=b");

    TestCanonicalization("HTTP://localhost:8080//", "http://localhost:8080/");
}

// Test with query parameters
TEST_F(CanonicalizeURLTest, QueryParameters) {
    TestCanonicalization("http://example.com/path?param=value", "http://example.com/path?param=value");

    TestCanonicalization("http://example.com/path?param1=value1&param2=value2#fragment",
                         "http://example.com/path?param1=value1&param2=value2");

    // Removal and reordering of query parameters
    TestCanonicalization("http://example.com/path?param2=value2&param1=value1",
                         "http://example.com/path?param1=value1&param2=value2");
    TestCanonicalization("http://example.com/path?xyz=123&abc", "http://example.com/path?abc&xyz=123");
    TestCanonicalization("http://example.com/path?xyz=123&abc=", "http://example.com/path?abc&xyz=123");

    TestCanonicalization("http://example.com/path?utm_source=google&thing=123&utm_term=thing+page&abc=hello+world",
                         "http://example.com/path?abc=hello+world&thing=123");

    TestCanonicalization("http://example.com/path?utm_source=google&utm_term=thing+page", "http://example.com/path");
}

// Test with empty paths
TEST_F(CanonicalizeURLTest, EmptyPath) {
    TestCanonicalization("http://example.com", "http://example.com/");
    TestCanonicalization("http://example.com/", "http://example.com/");
    TestCanonicalization("http://example.com?query=value", "http://example.com/?query=value");
}

// Test with relative directory traversal
TEST_F(CanonicalizeURLTest, DirectoryTraversal) {
    TestCanonicalization("http://example.com/a/b/../c/d/", "http://example.com/a/c/d/");
    TestCanonicalization("http://example.com/a/b/../c/d", "http://example.com/a/c/d");
    TestCanonicalization("http://example.com/../../a/./b/./c", "http://example.com/a/b/c");
    TestCanonicalization("http://example.com/a/b/c/../../d/e/../f", "http://example.com/a/d/f");
    TestCanonicalization("http://example.com/a/b/c/../../d/e/../f?123=456", "http://example.com/a/d/f?123=456");
}

// Test edge cases
TEST_F(CanonicalizeURLTest, EdgeCases) {
    // URL with unusual but valid characters
    TestCanonicalization("http://example.com/~user/file.txt", "http://example.com/~user/file.txt");

    // URL with encoded characters
    TestCanonicalization("http://example.com/path%20with%20spaces", "http://example.com/path%20with%20spaces");
}
