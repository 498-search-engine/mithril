#ifndef COMMON_STRINGTRIE_H
#define COMMON_STRINGTRIE_H

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class StringTrie {
public:
    /**
     * @brief Inserts a sequence of strings into the trie.
     */
    void Insert(const std::vector<std::string>& seq);

    /**
     * @brief Inserts a sequence of strings into the trie.
     */
    void Insert(const std::vector<std::string_view>& seq);

    /**
     * @brief Checks whether a sequence of strings is in the trie.
     */
    bool Contains(const std::vector<std::string>& seq) const;

    /**
     * @brief Checks whether a sequence of strings is in the trie.
     */
    bool Contains(const std::vector<std::string_view>& seq) const;

    /**
     * @brief Checks whether a prefix sequence of strings is in the trie.
     */
    bool ContainsPrefix(const std::vector<std::string>& seq) const;

    /**
     * @brief Checks whether a prefix sequence of strings is in the trie.
     */
    bool ContainsPrefix(const std::vector<std::string_view>& seq) const;

private:
    struct Node {
        std::map<std::string, Node, std::less<>> nodes;
        bool terminal{false};
    };

    template<typename T>
    bool ContainsImpl(const std::vector<T>& seq) const {
        const Node* currentNode = &root_;

        for (const auto& segment : seq) {
            auto it = currentNode->nodes.find(segment);
            if (it == currentNode->nodes.end()) {
                return false;
            }
            currentNode = &(it->second);
        }

        return currentNode->terminal;
    }

    template<typename T>
    bool ContainsPrefixImpl(const std::vector<T>& seq) const {
        if (seq.empty()) {
            return root_.terminal;
        } else if (root_.terminal) {
            return true;
        }

        const Node* currentNode = &root_;

        // Check each prefix of the sequence, from longest to shortest
        for (const auto& segment : seq) {
            auto it = currentNode->nodes.find(segment);
            if (it == currentNode->nodes.end()) {
                return false;
            }
            currentNode = &it->second;

            if (currentNode->terminal) {
                // Prefix match
                return true;
            }
        }

        // No prefix of the sequence was a complete path
        return false;
    }

    Node root_;
};

#endif
