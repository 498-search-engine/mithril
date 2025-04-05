#include "DocumentMapReader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace mithril {

DocumentMapReader::DocumentMapReader(const std::string& index_dir) {
    loadDocumentMap(index_dir + "/document_map.data");
}

void DocumentMapReader::loadDocumentMap(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open document map: " + path);
    }

    // Read document count
    in.read(reinterpret_cast<char*>(&doc_count_), sizeof(uint32_t));
    doc_infos_.reserve(doc_count_);

    // Temp buffers
    std::vector<char> url_buffer;
    std::vector<char> title_buffer;

    // First pass: calculate total string sizes
    size_t total_url_size = 0;
    size_t total_title_size = 0;
    std::streampos start_pos = in.tellg();

    for (size_t i = 0; i < doc_count_; ++i) {
        data::docid_t doc_id;
        uint32_t url_len, title_len;

        in.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
        in.read(reinterpret_cast<char*>(&url_len), sizeof(url_len));
        in.seekg(url_len, std::ios::cur);
        in.read(reinterpret_cast<char*>(&title_len), sizeof(title_len));
        in.seekg(title_len, std::ios::cur);

        // Skip BM25F stats in first pass
        in.seekg(sizeof(uint32_t) * 4 + sizeof(float), std::ios::cur);

        total_url_size += url_len;
        total_title_size += title_len;
    }

    // Pre-allocate string pools
    urls_.reserve(total_url_size);
    titles_.reserve(total_title_size);
    in.seekg(start_pos);

    // Second pass: actual loading
    for (size_t i = 0; i < doc_count_; ++i) {
        DocInfo info;
        uint32_t url_str_len, title_str_len;

        // Read ID and URL string
        in.read(reinterpret_cast<char*>(&info.id), sizeof(info.id));
        in.read(reinterpret_cast<char*>(&url_str_len), sizeof(url_str_len));
        url_buffer.resize(url_str_len);
        in.read(url_buffer.data(), url_str_len);
        info.url_offset = urls_.size();
        info.url_length = url_str_len;  // Store actual string length
        urls_.append(url_buffer.data(), url_str_len);

        // Read title string
        in.read(reinterpret_cast<char*>(&title_str_len), sizeof(title_str_len));
        title_buffer.resize(title_str_len);
        in.read(title_buffer.data(), title_str_len);
        info.title_offset = titles_.size();
        info.title_length = title_str_len;  // Store actual string length
        titles_.append(title_buffer.data(), title_str_len);

        // Read BM25F stats (but don't overwrite string lengths)
        uint32_t body_tokens, title_tokens, url_tokens, desc_tokens;
        in.read(reinterpret_cast<char*>(&body_tokens), sizeof(body_tokens));
        in.read(reinterpret_cast<char*>(&title_tokens), sizeof(title_tokens));
        in.read(reinterpret_cast<char*>(&url_tokens), sizeof(url_tokens));
        in.read(reinterpret_cast<char*>(&desc_tokens), sizeof(desc_tokens));
        in.read(reinterpret_cast<char*>(&info.pagerank_score), sizeof(info.pagerank_score));

        // Store token counts separately
        info.body_length = body_tokens;
        // Note: We preserve title_str_len for the actual string length
        info.desc_length = desc_tokens;

        doc_infos_.push_back(info);
        std::string_view url_view(urls_.data() + info.url_offset, info.url_length);
        url_to_id_[url_view] = info.id;
    }
    for (size_t i = 0; i < doc_infos_.size(); ++i) {
        id_to_index_[doc_infos_[i].id] = i;
    }
}

std::optional<data::Document> DocumentMapReader::getDocument(data::docid_t id) const {
    auto it = id_to_index_.find(id);
    if (it == id_to_index_.end()) {
        return std::nullopt;
    }

    const DocInfo& info = doc_infos_[it->second];

    data::Document doc;
    doc.id = info.id;
    doc.url = std::string(urls_.data() + info.url_offset, info.url_length);

    // Parse title into words (space-separated)
    std::string title_str(titles_.data() + info.title_offset, info.title_length);
    std::istringstream title_stream(title_str);
    std::string word;
    while (title_stream >> word) {
        doc.title.push_back(word);
    }

    return doc;
}

std::optional<data::docid_t> DocumentMapReader::lookupDocID(const std::string& url) const {
    auto it = url_to_id_.find(url);
    if (it != url_to_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool DocumentMapReader::hasNext() const {
    return current_position_ < doc_infos_.size();
}

data::Document DocumentMapReader::next() {
    if (!hasNext()) {
        throw std::runtime_error("No more documents");
    }

    const DocInfo& info = doc_infos_[current_position_++];

    data::Document doc;
    doc.id = info.id;
    doc.url = std::string(urls_.data() + info.url_offset, info.url_length);

    // Parse title into words
    std::string title_str(titles_.data() + info.title_offset, info.title_length);
    std::istringstream title_stream(title_str);
    std::string word;
    while (title_stream >> word) {
        doc.title.push_back(word);
    }

    return doc;
}

void DocumentMapReader::reset() {
    current_position_ = 0;
}

}  // namespace mithril
