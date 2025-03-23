#include "Robots.h"

#include <string_view>
#include <gtest/gtest.h>

using namespace mithril;
using namespace std::string_view_literals;

TEST(Robots, ParseRobotsLine) {
    // Standard line
    {
        auto line = internal::ParseRobotLine("User-agent: *"sv);
        ASSERT_TRUE(line.has_value());
        ASSERT_EQ(line->directive, "User-agent"sv);
        ASSERT_EQ(line->value, "*"sv);
    }

    // Line comment
    {
        auto line = internal::ParseRobotLine("# This is a comment"sv);
        ASSERT_FALSE(line.has_value());
    }

    // Whitespace in places
    {
        auto line = internal::ParseRobotLine("    User-agent :  *  # Everything"sv);
        ASSERT_TRUE(line.has_value());
        ASSERT_EQ(line->directive, "User-agent"sv);
        ASSERT_EQ(line->value, "*"sv);
    }

    // No whitespace in places
    {
        auto line = internal::ParseRobotLine("Disallow:/"sv);
        ASSERT_TRUE(line.has_value());
        ASSERT_EQ(line->directive, "Disallow"sv);
        ASSERT_EQ(line->value, "/"sv);
    }

    // Empty value is not parsed
    {
        auto line = internal::ParseRobotLine("Disallow:"sv);
        ASSERT_FALSE(line.has_value());
    }
}

TEST(Robots, ParseRobotsTxt) {
    // Catch-all user agent
    {
        auto txt = "User-agent: *\n"
                   "Crawl-Delay: 30\n"
                   "Disallow: /profile/message/\n"
                   "Disallow: /meta/*/download/  # Disallow download links\n"
                   "Allow: /profile/about-me/\n"sv;

        auto d = internal::ParseRobotsTxt(txt, "crawler"sv);
        ASSERT_EQ(d.disallows.size(), 2);
        EXPECT_EQ(d.disallows[0], "/profile/message/"sv);
        EXPECT_EQ(d.disallows[1], "/meta/*/download/"sv);
        ASSERT_EQ(d.allows.size(), 1);
        EXPECT_EQ(d.allows[0], "/profile/about-me/"sv);
        ASSERT_TRUE(d.crawlDelay.HasValue());
        EXPECT_EQ(*d.crawlDelay, 30);
    }

    // Multiple user agents, one match
    {
        auto txt = "User-agent: *\n"
                   "Disallow: /profile/message/\n"
                   "Disallow: /meta/*/download/  # Disallow download links\n"
                   "Allow: /profile/about-me/\n"
                   "\n"
                   "User-agent: Googlebot\n"
                   "Disallow: /i-hate-google\n"sv;

        auto d = internal::ParseRobotsTxt(txt, "crawler"sv);
        ASSERT_EQ(d.disallows.size(), 2);
        EXPECT_EQ(d.disallows[0], "/profile/message/"sv);
        EXPECT_EQ(d.disallows[1], "/meta/*/download/"sv);
        ASSERT_EQ(d.allows.size(), 1);
        EXPECT_EQ(d.allows[0], "/profile/about-me/"sv);
        EXPECT_FALSE(d.crawlDelay.HasValue());
    }

    // Multiple user agents, multiple matchs
    {
        auto txt = "User-agent: *\n"
                   "Disallow: /profile/message/\n"
                   "Disallow: /meta/*/download/  # Disallow download links\n"
                   "Allow: /profile/about-me/\n"
                   "\n"
                   "User-agent: crawler\n"
                   "Disallow: /i-hate-crawler\n"sv;

        auto d = internal::ParseRobotsTxt(txt, "crawler"sv);
        ASSERT_EQ(d.disallows.size(), 3);
        EXPECT_EQ(d.disallows[0], "/profile/message/"sv);
        EXPECT_EQ(d.disallows[1], "/meta/*/download/"sv);
        EXPECT_EQ(d.disallows[2], "/i-hate-crawler"sv);
        ASSERT_EQ(d.allows.size(), 1);
        EXPECT_EQ(d.allows[0], "/profile/about-me/"sv);
    }

    // Back-to-back user agents
    {
        auto txt = "User-agent: Googlebot\n"
                   "User-agent: crawler\n"
                   "Disallow: /profile/message/\n"
                   "Disallow: /meta/*/download/  # Disallow download links\n"
                   "Allow: /profile/about-me/\n"sv;

        auto d = internal::ParseRobotsTxt(txt, "crawler"sv);
        ASSERT_EQ(d.disallows.size(), 2);
        EXPECT_EQ(d.disallows[0], "/profile/message/"sv);
        EXPECT_EQ(d.disallows[1], "/meta/*/download/"sv);
        ASSERT_EQ(d.allows.size(), 1);
        EXPECT_EQ(d.allows[0], "/profile/about-me/"sv);
    }

    // No matching user agent
    {
        auto txt = "User-agent: Googlebot\n"
                   "Disallow: /profile/message/\n"
                   "Disallow: /meta/*/download/  # Disallow download links\n"
                   "Allow: /profile/about-me/\n"sv;

        auto d = internal::ParseRobotsTxt(txt, "crawler"sv);
        ASSERT_EQ(d.disallows.size(), 0);
        ASSERT_EQ(d.allows.size(), 0);
    }

    // Case insensitive user agent
    {
        auto txt = "User-agent: CRAWLER\n"
                   "Disallow: /profile/message/\n"
                   "Disallow: /meta/*/download/  # Disallow download links\n"
                   "Allow: /profile/about-me/\n"sv;

        auto d = internal::ParseRobotsTxt(txt, "Crawler"sv);
        ASSERT_NE(d.disallows.size(), 0);
        ASSERT_NE(d.allows.size(), 0);
    }
}
TEST(Robots, Disallow) {
    // Simple match or prefix match
    {
        auto trie = internal::RobotsTrie({"/search/", "/gist/"}, {});
        EXPECT_TRUE(trie.IsAllowed("/download/"sv));
        EXPECT_FALSE(trie.IsAllowed("/search/"sv));
        EXPECT_FALSE(trie.IsAllowed("/search/thing"sv));
        EXPECT_FALSE(trie.IsAllowed("/gist/abc/123"sv));
        EXPECT_TRUE(trie.IsAllowed("/searchbar"sv));
    }

    // Empty path
    {
        auto trie = internal::RobotsTrie({"/"}, {});
        EXPECT_FALSE(trie.IsAllowed("/"sv));
        EXPECT_FALSE(trie.IsAllowed("/anything"sv));
        EXPECT_FALSE(trie.IsAllowed("/path/to/something"sv));
    }

    // Wildcard patterns
    {
        auto trie = internal::RobotsTrie({"/private/*"}, {});
        EXPECT_FALSE(trie.IsAllowed("/private/"sv));
        EXPECT_FALSE(trie.IsAllowed("/private/docs"sv));
        EXPECT_FALSE(trie.IsAllowed("/private/user/profile"sv));
        EXPECT_TRUE(trie.IsAllowed("/public/docs"sv));
    }

    // Multiple patterns with common prefixes
    {
        auto trie = internal::RobotsTrie({"/api/v1/", "/api/v2/private/"}, {});
        EXPECT_FALSE(trie.IsAllowed("/api/v1/"sv));
        EXPECT_FALSE(trie.IsAllowed("/api/v1/users"sv));
        EXPECT_TRUE(trie.IsAllowed("/api/v2/public"sv));
        EXPECT_FALSE(trie.IsAllowed("/api/v2/private/"sv));
        EXPECT_FALSE(trie.IsAllowed("/api/v2/private/data"sv));
    }

    // Prefix without / at end
    {
        auto trie = internal::RobotsTrie({"/posts"}, {});
        EXPECT_FALSE(trie.IsAllowed("/posts/"sv));
        EXPECT_FALSE(trie.IsAllowed("/posts/123"sv));
        EXPECT_FALSE(trie.IsAllowed("/poststamp"sv));
        EXPECT_FALSE(trie.IsAllowed("/poststamp/123"sv));
    }
}

TEST(Robots, Allow) {
    // Simple allow rules
    {
        auto trie = internal::RobotsTrie({}, {"/public/", "/downloads/"});
        EXPECT_TRUE(trie.IsAllowed("/public/"sv));
        EXPECT_TRUE(trie.IsAllowed("/public/docs"sv));
        EXPECT_TRUE(trie.IsAllowed("/downloads/file.txt"sv));
        EXPECT_TRUE(trie.IsAllowed("/other/path"sv));  // No disallow rules
    }

    // Allow with wildcards
    {
        auto trie = internal::RobotsTrie({}, {"/api/*/public"});
        EXPECT_TRUE(trie.IsAllowed("/api/v1/public"sv));
        EXPECT_TRUE(trie.IsAllowed("/api/v2/public"sv));
        EXPECT_TRUE(trie.IsAllowed("/other/path"sv));
    }
}

TEST(Robots, Precedence) {
    // Allow takes precedence over Disallow for equal length
    {
        auto trie = internal::RobotsTrie({"/path/"}, {"/path/"});
        EXPECT_TRUE(trie.IsAllowed("/path/"sv));
        EXPECT_TRUE(trie.IsAllowed("/path/to/file"sv));
    }

    // More specific rules take precedence
    {
        auto trie = internal::RobotsTrie({"/private/", "/private/*/logs"}, {"/private/*/public"});
        EXPECT_FALSE(trie.IsAllowed("/private/"sv));
        EXPECT_FALSE(trie.IsAllowed("/private/user/logs"sv));
        EXPECT_TRUE(trie.IsAllowed("/private/user/public"sv));
    }

    // Complex precedence cases
    {
        auto trie = internal::RobotsTrie({"/", "/private/*", "/api/"}, {"/private/docs/*", "/api/public/"});
        EXPECT_FALSE(trie.IsAllowed("/random"sv));           // Caught by root disallow
        EXPECT_FALSE(trie.IsAllowed("/private/user"sv));     // Caught by /private/* disallow
        EXPECT_TRUE(trie.IsAllowed("/private/docs/api"sv));  // Allowed by specific rule
        EXPECT_FALSE(trie.IsAllowed("/api/private"sv));      // Caught by /api/ disallow
        EXPECT_TRUE(trie.IsAllowed("/api/public/docs"sv));   // Allowed by specific rule
    }

    // Edge cases with nested rules
    {
        auto trie = internal::RobotsTrie({"/a/", "/a/b/", "/a/b/c/"}, {"/a/b/"});
        EXPECT_FALSE(trie.IsAllowed("/a/"sv));
        EXPECT_TRUE(trie.IsAllowed("/a/b/"sv));     // Explicit allow
        EXPECT_FALSE(trie.IsAllowed("/a/b/c/"sv));  // More specific disallow
    }
}

TEST(Robots, Wildcards) {
    // Valid wildcard patterns (full path segments)
    {
        auto trie = internal::RobotsTrie({"/api/*/docs", "/users/*/settings/*"}, {"/api/*/public"});
        EXPECT_FALSE(trie.IsAllowed("/api/v1/docs"sv));
        EXPECT_FALSE(trie.IsAllowed("/api/v2/docs"sv));
        EXPECT_TRUE(trie.IsAllowed("/api/v1/public"sv));
        EXPECT_FALSE(trie.IsAllowed("/users/john/settings/privacy"sv));
        EXPECT_FALSE(trie.IsAllowed("/users/jane/settings/email"sv));
        EXPECT_TRUE(trie.IsAllowed("/api/v1/private"sv));  // No match
    }

    // Multiple wildcards in sequence
    {
        auto trie = internal::RobotsTrie({"/data/*/*/logs"}, {});
        EXPECT_FALSE(trie.IsAllowed("/data/2024/01/logs"sv));
        EXPECT_FALSE(trie.IsAllowed("/data/us/west/logs"sv));
        EXPECT_TRUE(trie.IsAllowed("/data/2024/01/other"sv));  // No match
    }

    // Wildcards at start and end
    {
        auto trie = internal::RobotsTrie({"/*/admin/*"}, {"/*/*/public"});
        EXPECT_FALSE(trie.IsAllowed("/us/admin/users"sv));
        EXPECT_FALSE(trie.IsAllowed("/eu/admin/settings"sv));
        EXPECT_TRUE(trie.IsAllowed("/us/region/public"sv));
        EXPECT_TRUE(trie.IsAllowed("/eu/zone/public"sv));
    }

    // Invalid wildcard patterns (should be discarded)
    {
        auto trie = internal::RobotsTrie(
            {
                "/partial_*_wildcard/",  // Invalid - wildcard within segment
                "/api/v*/docs",          // Invalid - wildcard within segment
                "/users/*/settings",     // Valid
                "/*_invalid/",           // Invalid - wildcard within segment
                "/test*/",               // Invalid - wildcard within segment
            },
            {});
        EXPECT_TRUE(trie.IsAllowed("/partial_abc_wildcard/"sv));  // Invalid rule discarded
        EXPECT_TRUE(trie.IsAllowed("/api/v1/docs"sv));            // Invalid rule discarded
        EXPECT_FALSE(trie.IsAllowed("/users/john/settings"sv));   // Valid rule works
        EXPECT_TRUE(trie.IsAllowed("/abc_invalid/"sv));           // Invalid rule discarded
        EXPECT_TRUE(trie.IsAllowed("/test123/"sv));               // Invalid rule discarded
    }

    // Mixed valid and invalid patterns
    {
        auto trie = internal::RobotsTrie({"/*/valid", "/in*valid", "/test/*"}, {"/valid/*", "/*_invalid"});
        EXPECT_FALSE(trie.IsAllowed("/something/valid"sv));
        EXPECT_TRUE(trie.IsAllowed("/invalid"sv));  // Invalid rule discarded
        EXPECT_FALSE(trie.IsAllowed("/test/anything"sv));
        EXPECT_TRUE(trie.IsAllowed("/valid/stuff"sv));
        EXPECT_TRUE(trie.IsAllowed("/something_invalid"sv));  // Invalid rule discarded
    }

    {
        auto trie = internal::RobotsTrie({"/Special:*"}, {"/Special:ExplicitlyAllowed"});
        EXPECT_TRUE(trie.IsAllowed("/path"));
        EXPECT_TRUE(trie.IsAllowed("/Special"));
        EXPECT_FALSE(trie.IsAllowed("/Special:"));
        EXPECT_FALSE(trie.IsAllowed("/Special:asdf"));
        EXPECT_FALSE(trie.IsAllowed("/Special:asdf/123"));
        EXPECT_FALSE(trie.IsAllowed("/Special:asdf/123/"));
        EXPECT_TRUE(trie.IsAllowed("/Special:ExplicitlyAllowed"));
    }
}

TEST(Robots, EdgeCases) {
    // Empty rules
    {
        auto trie = internal::RobotsTrie({}, {});
        EXPECT_TRUE(trie.IsAllowed("/any/path"sv));
        EXPECT_TRUE(trie.IsAllowed("/"sv));
    }

    // Special characters
    {
        auto trie = internal::RobotsTrie({"/test?param=1", "/path#section"}, {"/test?param=2"});
        EXPECT_FALSE(trie.IsAllowed("/test?param=1"sv));
        EXPECT_TRUE(trie.IsAllowed("/test?param=2"sv));
        EXPECT_FALSE(trie.IsAllowed("/path#section"sv));
    }
}

TEST(Robots, EndToEnd) {
    // Basic single user-agent rules
    {
        auto txt = "User-agent: *\n"
                   "Disallow: /private/\n"
                   "Allow: /private/public/\n"
                   "Crawl-Delay: 30\n"sv;

        auto rules = RobotRules::FromRobotsTxt(txt, "testbot"sv);
        EXPECT_FALSE(rules.Allowed("/private/profile"sv));
        EXPECT_TRUE(rules.Allowed("/private/public/docs"sv));
        EXPECT_TRUE(rules.Allowed("/public/stuff"sv));
        ASSERT_TRUE(rules.CrawlDelay().HasValue());
        EXPECT_EQ(*rules.CrawlDelay(), 30);
    }

    // Multiple user-agents with different rules
    {
        auto txt = "User-agent: *\n"
                   "Disallow: /downloads/\n"
                   "\n"
                   "User-agent: goodbot\n"
                   "Allow: /downloads/public/\n"
                   "Disallow: /downloads/private/\n"sv;

        auto defaultRules = RobotRules::FromRobotsTxt(txt, "randombot"sv);
        EXPECT_FALSE(defaultRules.Allowed("/downloads/anything"sv));
        EXPECT_FALSE(defaultRules.Allowed("/downloads/public/file.txt"sv));
        EXPECT_FALSE(defaultRules.CrawlDelay().HasValue());

        auto specificRules = RobotRules::FromRobotsTxt(txt, "goodbot"sv);
        EXPECT_TRUE(specificRules.Allowed("/downloads/public/file.txt"sv));
        EXPECT_FALSE(specificRules.Allowed("/downloads/private/secret.txt"sv));
        EXPECT_FALSE(specificRules.CrawlDelay().HasValue());
    }

    // Comments and whitespace handling
    {
        auto txt = "User-agent: *  # Default rules\n"
                   "Disallow: /secret/  # Private stuff\n"
                   "Allow: /secret/public/  # But allow public content\n"
                   "\n"
                   "# Special rules for testbot\n"
                   "User-agent: testbot\n"
                   "Disallow: /test/  # No test access\n"
                   "  Allow: /test/allowed/  # Except this\n"sv;

        auto rules = RobotRules::FromRobotsTxt(txt, "testbot"sv);
        EXPECT_FALSE(rules.Allowed("/test/forbidden"sv));
        EXPECT_TRUE(rules.Allowed("/test/allowed/stuff"sv));
        EXPECT_FALSE(rules.Allowed("/secret/things"sv));
        EXPECT_TRUE(rules.Allowed("/secret/public/things"sv));
    }

    // Wildcard patterns and multiple rules
    {
        auto txt = "User-agent: *\n"
                   "Disallow: /api/*/private/\n"
                   "Allow: /api/v1/private/docs/\n"
                   "Disallow: /users/*/settings/\n"
                   "Allow: /users/*/settings/public/\n"
                   "Disallow: /Special:*\n"sv;

        auto rules = RobotRules::FromRobotsTxt(txt, "crawler"sv);
        EXPECT_FALSE(rules.Allowed("/api/v1/private/config"sv));
        EXPECT_FALSE(rules.Allowed("/api/v2/private/secret"sv));
        EXPECT_TRUE(rules.Allowed("/api/v1/private/docs/guide"sv));
        EXPECT_FALSE(rules.Allowed("/users/john/settings/email"sv));
        EXPECT_TRUE(rules.Allowed("/users/john/settings/public/profile"sv));
        EXPECT_FALSE(rules.Allowed("/Special:Editors"sv));
    }

    // Edge cases and invalid patterns
    {
        auto txt = "User-agent: testbot\n"
                   "Disallow: /invalid_*_pattern/\n"  // Should be ignored
                   "Allow: /valid/*/pattern/\n"
                   "Disallow: /test*/\n"  // Should be ignored
                   "Allow: /*/valid\n"
                   "\n"
                   "User-agent: *\n"
                   "Disallow: /\n"sv;

        auto rules = RobotRules::FromRobotsTxt(txt, "testbot"sv);
        EXPECT_FALSE(rules.Allowed("/invalid_123_pattern/"sv));
        EXPECT_TRUE(rules.Allowed("/valid/123/pattern/"sv));
        EXPECT_FALSE(rules.Allowed("/test123/"sv));
        EXPECT_TRUE(rules.Allowed("/something/valid"sv));
    }

    // Invalid patterns without wildcard interference
    {
        auto txt = "User-agent: testbot\n"
                   "Disallow: /invalid_*_pattern/\n"  // Should be ignored
                   "Allow: /valid/*/pattern/\n"
                   "Disallow: /test*/\n"  // Should be ignored
                   "Allow: /*/valid\n"sv;

        auto rules = RobotRules::FromRobotsTxt(txt, "testbot"sv);
        // Invalid patterns should be ignored
        EXPECT_TRUE(rules.Allowed("/invalid_123_pattern/"sv));
        EXPECT_TRUE(rules.Allowed("/test123/"sv));
        // Valid patterns should work
        EXPECT_TRUE(rules.Allowed("/valid/123/pattern/"sv));
        EXPECT_TRUE(rules.Allowed("/something/valid"sv));
    }

    // Empty and malformed content
    {
        // Empty content
        auto emptyRules = RobotRules::FromRobotsTxt(""sv, "bot"sv);
        EXPECT_TRUE(emptyRules.Allowed("/anything"sv));

        // Empty disallow rule
        auto emptyDisallow = RobotRules::FromRobotsTxt("Disallow:\n"sv, "bot"sv);
        EXPECT_TRUE(emptyRules.Allowed("/anything"sv));

        // Only comments
        auto commentRules = RobotRules::FromRobotsTxt("# Just a comment\n"
                                                      "# Another comment"sv,
                                                      "bot"sv);
        EXPECT_TRUE(commentRules.Allowed("/anything"sv));

        // Malformed but parseable
        auto malformedTxt = "User-agent: *\n"
                            "Disallow: /bad\n"
                            "Random-line\n"
                            "Allow: /good\n"sv;
        auto malformedRules = RobotRules::FromRobotsTxt(malformedTxt, "bot"sv);
        EXPECT_FALSE(malformedRules.Allowed("/bad"sv));
        EXPECT_TRUE(malformedRules.Allowed("/good"sv));
    }
}
