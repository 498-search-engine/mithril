#include "StringTrie.h"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

void StringTrie::Insert(const std::vector<std::string>& seq) {
    Node* currentNode = &root_;

    for (const auto& segment : seq) {
        currentNode = &(currentNode->nodes[segment]);
    }

    currentNode->terminal = true;
}

void StringTrie::Insert(const std::vector<std::string_view>& seq) {
    Node* currentNode = &root_;

    for (auto segment : seq) {
        auto it = currentNode->nodes.find(segment);
        if (it == currentNode->nodes.end()) {
            auto inserted = currentNode->nodes.insert({std::string{segment}, Node{}});
            assert(inserted.second);
            currentNode = &inserted.first->second;
        } else {
            currentNode = &it->second;
        }
    }

    currentNode->terminal = true;
}

bool StringTrie::Contains(const std::vector<std::string>& seq) const {
    return ContainsImpl(seq);
}

bool StringTrie::Contains(const std::vector<std::string_view>& seq) const {
    return ContainsImpl(seq);
}

bool StringTrie::ContainsPrefix(const std::vector<std::string>& seq) const {
    return ContainsPrefixImpl(seq);
}

bool StringTrie::ContainsPrefix(const std::vector<std::string_view>& seq) const {
    return ContainsPrefixImpl(seq);
}
