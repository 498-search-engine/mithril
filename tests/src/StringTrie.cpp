#include "StringTrie.h"

#include <string>
#include <string_view>
#include <vector>
#include <gtest/gtest.h>

using namespace std::string_view_literals;

class StringTrieTest : public ::testing::Test {
protected:
    StringTrie trie;

    // Sample data for testing
    std::vector<std::string> path1 = {"root", "folder", "subfolder", "file"};
    std::vector<std::string> path2 = {"root", "folder", "file"};
    std::vector<std::string> path3 = {"another", "path"};
    std::vector<std::string> emptyPath = {};

    void SetUp() override {
        // Initialize trie with some data for tests that need pre-populated trie
        trie.Insert(path1);
        trie.Insert(path2);
    }
};

// Test basic insertion and retrieval functionality
TEST_F(StringTrieTest, BasicFunctionality) {
    EXPECT_TRUE(trie.Contains(path1));
    EXPECT_TRUE(trie.Contains(path2));
    EXPECT_FALSE(trie.Contains(path3));

    trie.Insert(path3);
    EXPECT_TRUE(trie.Contains(path3));
}

// Test with empty path vector
TEST_F(StringTrieTest, EmptyPath) {
    EXPECT_FALSE(trie.Contains(emptyPath));

    trie.Insert(emptyPath);
    EXPECT_TRUE(trie.Contains(emptyPath));
}

// Test paths that share common prefixes
TEST_F(StringTrieTest, SharedPrefixes) {
    std::vector<std::string> partialPath = {"root", "folder"};
    EXPECT_FALSE(trie.Contains(partialPath));

    trie.Insert(partialPath);
    EXPECT_TRUE(trie.Contains(partialPath));

    // Verify original paths still exist
    EXPECT_TRUE(trie.Contains(path1));
    EXPECT_TRUE(trie.Contains(path2));
}

// Test with duplicate insertions
TEST_F(StringTrieTest, DuplicateInsertions) {
    // Re-insert an existing path
    trie.Insert(path1);

    // Should still exist normally
    EXPECT_TRUE(trie.Contains(path1));
}

// Test case sensitivity
TEST_F(StringTrieTest, CaseSensitivity) {
    std::vector<std::string> upperCasePath = {"ROOT", "FOLDER", "SUBFOLDER", "FILE"};
    EXPECT_FALSE(trie.Contains(upperCasePath));

    trie.Insert(upperCasePath);
    EXPECT_TRUE(trie.Contains(upperCasePath));
    EXPECT_TRUE(trie.Contains(path1));  // Original path should remain
}

// Test with longer paths
TEST_F(StringTrieTest, LongerPaths) {
    std::vector<std::string> longPath = {"level1", "level2", "level3", "level4", "level5", "level6", "level7"};
    EXPECT_FALSE(trie.Contains(longPath));

    trie.Insert(longPath);
    EXPECT_TRUE(trie.Contains(longPath));
}

// Test with path that extends beyond existing path
TEST_F(StringTrieTest, ExtendedPath) {
    std::vector<std::string> extendedPath = {"root", "folder", "subfolder", "file", "extension"};
    EXPECT_FALSE(trie.Contains(extendedPath));

    trie.Insert(extendedPath);
    EXPECT_TRUE(trie.Contains(extendedPath));
    EXPECT_TRUE(trie.Contains(path1));  // Original shorter path should still exist
}

// Test with path that diverges from existing path
TEST_F(StringTrieTest, DivergentPath) {
    std::vector<std::string> divergentPath = {"root", "folder", "document"};
    EXPECT_FALSE(trie.Contains(divergentPath));

    trie.Insert(divergentPath);
    EXPECT_TRUE(trie.Contains(divergentPath));
    EXPECT_TRUE(trie.Contains(path1));  // Original path should remain
    EXPECT_TRUE(trie.Contains(path2));
}

// Test with paths containing special characters
TEST_F(StringTrieTest, SpecialCharacters) {
    std::vector<std::string> specialPath = {"root!@#", "folder$%^", "file&*()"};
    EXPECT_FALSE(trie.Contains(specialPath));

    trie.Insert(specialPath);
    EXPECT_TRUE(trie.Contains(specialPath));
}

// Test with empty string components in the path
TEST_F(StringTrieTest, EmptyStringComponents) {
    std::vector<std::string> pathWithEmpty = {"root", "", "file"};
    EXPECT_FALSE(trie.Contains(pathWithEmpty));

    trie.Insert(pathWithEmpty);
    EXPECT_TRUE(trie.Contains(pathWithEmpty));
}

// Performance test with many insertions
TEST_F(StringTrieTest, ManyInsertions) {
    StringTrie largeTrie;

    // Insert many paths
    for (int i = 0; i < 1000; i++) {
        std::vector<std::string> path = {"level" + std::to_string(i), "file" + std::to_string(i)};
        largeTrie.Insert(path);
    }

    // Verify some random paths
    EXPECT_TRUE(largeTrie.Contains({"level42"sv, "file42"sv}));
    EXPECT_TRUE(largeTrie.Contains({"level999"sv, "file999"sv}));
    EXPECT_FALSE(largeTrie.Contains({"level1000"sv, "file1000"sv}));  // Should not exist
}

// Test with Unicode/multibyte characters
TEST_F(StringTrieTest, UnicodeCharacters) {
    std::vector<std::string> unicodePath = {"📁", "文件夹", "파일"};
    EXPECT_FALSE(trie.Contains(unicodePath));

    trie.Insert(unicodePath);
    EXPECT_TRUE(trie.Contains(unicodePath));
}

// Test subpath relationships
TEST_F(StringTrieTest, SubpathRelationships) {
    // Insert a sequence
    std::vector<std::string> fullPath = {"a", "b", "c", "d"};
    trie.Insert(fullPath);

    // These should all be false since we only inserted the full path
    EXPECT_FALSE(trie.Contains({"a"sv}));
    EXPECT_FALSE(trie.Contains({"a"sv, "b"sv}));
    EXPECT_FALSE(trie.Contains({"a"sv, "b"sv, "c"sv}));

    // Now insert the subpaths
    trie.Insert({"a"sv});
    trie.Insert({"a"sv, "b"sv});
    trie.Insert({"a"sv, "b"sv, "c"sv});

    // Now they should all be true
    EXPECT_TRUE(trie.Contains({"a"sv}));
    EXPECT_TRUE(trie.Contains({"a"sv, "b"sv}));
    EXPECT_TRUE(trie.Contains({"a"sv, "b"sv, "c"sv}));
    EXPECT_TRUE(trie.Contains(fullPath));
}

// Test extremely large string
TEST_F(StringTrieTest, LargeString) {
    // Create a large string
    std::string largeStr(10000, 'x');
    std::vector<std::string> largePath = {largeStr};

    EXPECT_FALSE(trie.Contains(largePath));

    trie.Insert(largePath);
    EXPECT_TRUE(trie.Contains(largePath));
}

// Test the ContainsPrefix functionality for reverse hostnames
TEST_F(StringTrieTest, ContainsPrefix) {
    // Setup: Create a new trie with specific paths for testing prefixes
    StringTrie prefixTrie;
    prefixTrie.Insert({"com"sv, "github"sv, "sub"sv});           // com.github.sub
    prefixTrie.Insert({"com"sv, "example"sv, "docs"sv});         // com.example.docs
    prefixTrie.Insert({"org"sv, "wikipedia"sv, "en"sv});         // org.wikipedia.en
    prefixTrie.Insert({"io"sv, "github"sv, "repo"sv, "src"sv});  // io.github.repo.src
    prefixTrie.Insert({"com"sv, "badsite"sv});                   // io.github.repo.src

    // Test exact matches - these should return true
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"com"sv, "github"sv, "sub"sv}));
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"org"sv, "wikipedia"sv, "en"sv}));
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"com"sv, "badsite"sv}));
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"com"sv, "badsite"sv, "www"sv}));

    // Test where query extends beyond a complete path - should return true
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"com"sv, "github"sv, "sub"sv, "extra"sv}));
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"com"sv, "github"sv, "sub"sv, "extra"sv, "path"sv}));
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"org"sv, "wikipedia"sv, "en"sv, "page"sv}));

    // Test incomplete paths - should return false
    EXPECT_FALSE(prefixTrie.ContainsPrefix({"com"sv}));
    EXPECT_FALSE(prefixTrie.ContainsPrefix({"com"sv, "github"sv}));
    EXPECT_FALSE(prefixTrie.ContainsPrefix({"org"sv, "wikipedia"sv}));

    // Test completely non-existent paths - should return false
    EXPECT_FALSE(prefixTrie.ContainsPrefix({"net"sv, "example"sv}));
    EXPECT_FALSE(prefixTrie.ContainsPrefix({"com"sv, "gitlab"sv}));

    // Test the empty path
    std::vector<std::string> empty;
    EXPECT_FALSE(prefixTrie.ContainsPrefix(empty));

    // Test case sensitivity
    EXPECT_FALSE(prefixTrie.ContainsPrefix({"Com"sv, "github"sv, "sub"sv}));

    // Now insert empty path and test again
    prefixTrie.Insert(empty);
    EXPECT_TRUE(prefixTrie.ContainsPrefix(empty));
    EXPECT_TRUE(prefixTrie.ContainsPrefix({"anything"sv}));  // Empty is prefix of any path

    EXPECT_TRUE(trie.ContainsPrefix({"root"sv, "folder"sv, "subfolder"sv, "file"sv, "extension"sv}));
    EXPECT_TRUE(trie.ContainsPrefix({"root"sv, "folder"sv, "file"sv, "something"sv}));
    EXPECT_FALSE(trie.ContainsPrefix({"root"sv}));
    EXPECT_FALSE(trie.ContainsPrefix({"root"sv, "folder"sv}));
}
