#ifndef WEB_SNIPPETS_H
#define WEB_SNIPPETS_H

#include "data/Deserialize.h"
#include "data/Document.h"
#include "data/Gzip.h"
#include "data/Reader.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

class DocumentAccessor {
public:
    DocumentAccessor(const std::string& docs_path, size_t docs_per_chunk = 10000, size_t cache_size = 500)
        : docs_path_(docs_path), docs_per_chunk_(docs_per_chunk), max_cache_size_(cache_size) {
        if (docs_path.empty()) {
            return;
        }
        // Ensure path ends with slash
        if (!docs_path_.empty() && docs_path_.back() != '/') {
            docs_path_ += '/';
        }
        spdlog::info("DocumentAccessor initialized with path: {}", docs_path_);
    }

    std::optional<data::Document> getDocument(uint32_t doc_id) {
        if (docs_path_.empty()) {
            return std::nullopt;
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Check cache first
        auto it = doc_cache_.find(doc_id);
        if (it != doc_cache_.end()) {
            // Update timestamp and return cached document
            it->second.timestamp = std::chrono::steady_clock::now();
            return it->second.doc;
        }

        // Document not in cache, load from disk
        auto doc_opt = loadDocumentFromDisk(doc_id);
        if (!doc_opt) {
            return std::nullopt;
        }

        // Add to cache
        if (doc_cache_.size() >= max_cache_size_) {
            // Evict oldest entry
            auto oldest = std::min_element(doc_cache_.begin(), doc_cache_.end(), [](const auto& a, const auto& b) {
                return a.second.timestamp < b.second.timestamp;
            });
            doc_cache_.erase(oldest);
        }

        doc_cache_[doc_id] = {*doc_opt, std::chrono::steady_clock::now()};
        return doc_opt;
    }

    // Get concatenated text from document suitable for snippets
    std::string getDocumentText(const data::Document& doc) {
        // Prioritize content from most relevant fields
        if (!doc.words.empty()) {
            return combinedText(doc);
        }

        // Fallback options if main content is empty
        if (!doc.description.empty()) {
            std::string result;
            for (const auto& word : doc.description) {
                result += word + " ";
            }
            return result;
        }

        return "";
    }

private:
    std::string docs_path_;
    size_t docs_per_chunk_;
    size_t max_cache_size_;

    struct CacheEntry {
        data::Document doc;
        std::chrono::steady_clock::time_point timestamp;
    };

    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<uint32_t, CacheEntry> doc_cache_;

    std::optional<data::Document> loadDocumentFromDisk(uint32_t doc_id) const {
        // Calculate chunk number
        uint32_t chunk_id = doc_id / docs_per_chunk_;

        // Format chunk ID with leading zeros (10 digits)
        std::ostringstream chunk_oss;
        chunk_oss << "chunk_" << std::setw(10) << std::setfill('0') << chunk_id;

        // Format document ID with leading zeros (10 digits)
        std::ostringstream doc_oss;
        doc_oss << "doc_" << std::setw(10) << std::setfill('0') << doc_id;

        // Build full document path
        std::string doc_path = docs_path_ + chunk_oss.str() + "/" + doc_oss.str();

        // Check if file exists
        if (!std::filesystem::exists(doc_path)) {
            spdlog::warn("Document file not found: {}", doc_path);
            return std::nullopt;
        }

        // Load document (always gzipped)
        data::Document doc;
        try {
            data::FileReader file{doc_path.c_str()};
            data::GzipReader gzip{file};
            if (!data::DeserializeValue(doc, gzip)) {
                spdlog::error("Failed to deserialize document: {}", doc_path);
                return std::nullopt;
            }
            return doc;
        } catch (const std::exception& e) {
            spdlog::error("Error loading document {}: {}", doc_id, e.what());
            return std::nullopt;
        }
    }

    // Combine document content into a single string for snippet extraction
    std::string combinedText(const data::Document& doc) {
        // Simple concatenation with field separators
        std::string result;
        result.reserve(doc.title.size() * 5 + doc.description.size() + doc.words.size());

        if (!doc.title.empty()) {
            // Add title with more weight (repeat it)
            for (const auto& word : doc.title) {
                result += word + " ";
            }
            result += ". ";
        }

        if (!doc.description.empty()) {
            for (const auto& word : doc.description) {
                result += word + " ";
            }
            result += " ";
        }

        if (!doc.words.empty()) {
            // Add main content
            for (const auto& word : doc.words) {
                result += word + " ";
            }
        }

        return result;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SnippetGenerator {
public:
    SnippetGenerator(DocumentAccessor& doc_accessor) : doc_accessor_(doc_accessor) {}

    std::string generateSnippet(uint32_t doc_id,
                                const std::vector<std::string>& query_terms,
                                const std::unordered_map<std::string, std::vector<uint16_t>>& positions) {
        // Get document
        auto doc_opt = doc_accessor_.getDocument(doc_id);
        if (!doc_opt) {
            return "No preview available";
        }

        // Get document text
        std::string text = doc_accessor_.getDocumentText(*doc_opt);
        if (text.empty()) {
            return "No content available";
        }

        // Try position-based snippet generation first
        if (!positions.empty()) {
            std::string snippet = extractSnippetFromPositions(text, positions, query_terms);
            if (!snippet.empty()) {
                // spdlog::info("Extracted snippet from positions");
                return snippet;
            }
        }

        // Fall back to substring search
        std::string substring_snippet = extractSnippetFromSubstring(text, query_terms);
        if (!substring_snippet.empty()) {
            // spdlog::info("Extracted snippet from substring search");
            return substring_snippet;
        }

        // spdlog::info("No snippet found, using fallback");
        // Last resort: take beginning of document
        return getFallbackSnippet(text);
    }

private:
    DocumentAccessor& doc_accessor_;

    // Convert approximate token position to character position
    // This is a simple heuristic - tokens are roughly separated by spaces
    size_t findCharPositionFromTokenIndex(const std::string& text, uint16_t token_index) {
        size_t pos = 0;
        size_t count = 0;

        while (pos < text.length() && count < token_index) {
            // Skip current token
            while (pos < text.length() && !std::isspace(text[pos])) {
                pos++;
            }

            // Skip spaces
            while (pos < text.length() && std::isspace(text[pos])) {
                pos++;
            }

            count++;
        }

        return pos;
    }

    // Find good snippet boundaries
    std::pair<size_t, size_t> getSnippetBoundaries(const std::string& text, size_t pos, size_t context_length = 75) {
        size_t start = pos > context_length ? pos - context_length : 0;
        size_t end = std::min(pos + context_length, text.length());

        // Try to extend to sentence boundaries
        const auto extend_to_sentence_start = [&text](size_t pos) {
            // Look back up to 100 chars for sentence boundary
            size_t limit = pos > 100 ? pos - 100 : 0;
            while (pos > limit) {
                if (text[pos] == '.' || text[pos] == '!' || text[pos] == '?') {
                    return pos + 1;
                }
                pos--;
            }
            return pos;
        };

        const auto extend_to_sentence_end = [&text](size_t pos) {
            // Look ahead up to 100 chars for sentence boundary
            size_t limit = std::min(pos + 100, text.length());
            while (pos < limit) {
                if (text[pos] == '.' || text[pos] == '!' || text[pos] == '?') {
                    return pos + 1;
                }
                pos++;
            }
            return pos;
        };

        // Only adjust if we're not already at beginning/end
        if (start > 0) {
            start = extend_to_sentence_start(start);
        }

        if (end < text.length()) {
            end = extend_to_sentence_end(end);
        }

        return {start, end};
    }

    std::string extractSnippetFromPositions(const std::string& text,
                                            const std::unordered_map<std::string, std::vector<uint16_t>>& positions,
                                            const std::vector<std::string>& query_terms) {
        // Find best position to use (prioritize positions that match multiple terms)
        std::vector<std::pair<size_t, std::string>> char_positions;

        for (const auto& [term, pos_vec] : positions) {
            for (uint16_t token_pos : pos_vec) {
                size_t char_pos = findCharPositionFromTokenIndex(text, token_pos);
                if (char_pos < text.length()) {
                    char_positions.emplace_back(char_pos, term);
                }
            }
        }

        if (char_positions.empty()) {
            return "";
        }

        // Sort by character position
        std::sort(char_positions.begin(), char_positions.end());

        // Choose the middle position for the snippet
        size_t best_pos_index = char_positions.size() / 2;
        auto [pos, term] = char_positions[best_pos_index];

        // Get snippet boundaries
        auto [start, end] = getSnippetBoundaries(text, pos);

        // Extract snippet
        std::string snippet = text.substr(start, end - start);

        // Add ellipsis if needed
        if (start > 0) {
            snippet = "..." + snippet;
        }

        if (end < text.length()) {
            snippet += "...";
        }
        return snippet;
    }

    std::string extractSnippetFromSubstring(const std::string& text, const std::vector<std::string>& query_terms) {
        for (const auto& term : query_terms) {
            if (term.length() < 3)
                continue;  // Skip very short terms

            // Case-insensitive search
            std::string lower_text = text;
            std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), [](unsigned char c) {
                return std::tolower(c);
            });

            std::string lower_term = term;
            std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(), [](unsigned char c) {
                return std::tolower(c);
            });

            size_t pos = lower_text.find(lower_term);
            if (pos != std::string::npos) {
                auto [start, end] = getSnippetBoundaries(text, pos, 150);
                std::string snippet = text.substr(start, end - start);

                // Add ellipsis if needed
                if (start > 0) {
                    snippet = "..." + snippet;
                }

                if (end < text.length()) {
                    snippet += "...";
                }

                return snippet;
            }
        }

        return "";  // No matches found
    }

    std::string getFallbackSnippet(const std::string& text) {
        // Just take the beginning of the text
        const size_t max_length = 75;
        if (text.length() <= max_length) {
            return text;
        }

        // Try to break at a sentence boundary
        size_t end = max_length;
        while (end < std::min(text.length(), max_length + 50)) {
            if (text[end] == '.' || text[end] == '!' || text[end] == '?') {
                end++;
                break;
            }
            end++;
        }

        return text.substr(0, end) + "...";
    }
};

}  // namespace mithril

#endif  // WEB_SNIPPETS_H
