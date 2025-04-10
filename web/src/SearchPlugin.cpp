#include "SearchPlugin.h"

#include <chrono>
#include <fstream>
#include <future>
#include <memory>
#include <spdlog/spdlog.h>

SearchPlugin::SearchPlugin(const std::string& server_config_path) : config_path_(server_config_path) {

    spdlog::info("Initializing search plugin with config: {}", server_config_path);
    coordinator_initialized_ = TryInitializeCoordinator();

    if (!coordinator_initialized_) {
        spdlog::warn("Running in DEMO mode with mock results - no worker servers available");
    }
}

SearchPlugin::~SearchPlugin() {
    // Make sure we clean up any resources
    query_coordinator_.reset();
}

bool SearchPlugin::TryInitializeCoordinator() {
    try {
        // Check if config file exists
        std::ifstream config_file(config_path_);
        if (!config_file.good()) {
            spdlog::error("Server config file not found: {}", config_path_);
            return false;
        }

        // Attempt to initialize the coordinator
        query_coordinator_ = std::make_unique<mithril::QueryCoordinator>(config_path_);

        // Test connectivity to at least one server, could be implemented if QueryCoordinator has a test method

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize QueryCoordinator: {}", e.what());
        return false;
    }
}

bool SearchPlugin::MagicPath(const std::string path) {
    return path.rfind("/api/search", 0) == 0;
}

std::string SearchPlugin::DecodeUrlString(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            int value = 0;
            sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &value);
            decoded += static_cast<char>(value);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }

    return decoded;
}

std::string SearchPlugin::ProcessRequest(std::string request) {
    std::string query_text;
    int max_results = 50;

    // Parse the query parameter
    size_t query_pos = request.find("q=");
    if (query_pos != std::string::npos) {
        size_t query_end = request.find('&', query_pos);
        if (query_end == std::string::npos)
            query_end = request.find(' ', query_pos);
        if (query_end == std::string::npos)
            query_end = request.length();

        std::string encoded_query = request.substr(query_pos + 2, query_end - query_pos - 2);
        query_text = DecodeUrlString(encoded_query);
    }

    // Parse max_results parameter if present
    size_t max_pos = request.find("max=");
    if (max_pos != std::string::npos) {
        size_t max_end = request.find('&', max_pos);
        if (max_end == std::string::npos)
            max_end = request.find(' ', max_pos);
        if (max_end == std::string::npos)
            max_end = request.length();

        try {
            max_results = std::stoi(request.substr(max_pos + 4, max_end - max_pos - 4));
            // Cap max_results to a reasonable value
            max_results = std::min(max_results, 100);
        } catch (...) {
            // If parsing fails, keep default
            max_results = 50;
        }
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        CleanExpiredCache();

        auto it = query_cache_.find(query_text);
        if (it != query_cache_.end()) {
            // Return cached result but update timestamp
            it->second.timestamp = std::chrono::steady_clock::now();
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + it->second.result;
        }
    }

    // Execute query with timeout
    auto start_time = std::chrono::steady_clock::now();
    auto result_future = std::async(
        std::launch::async, [this, &query_text, max_results]() { return ExecuteQuery(query_text, max_results); });

    // Wait for result with timeout
    nlohmann::json result_json;
    auto status = result_future.wait_for(QUERY_TIMEOUT);
    if (status == std::future_status::ready) {
        result_json = result_future.get();
    } else {
        spdlog::warn("Query timed out after {} seconds: '{}'", QUERY_TIMEOUT.count(), query_text);
        result_json["error"] = "Query timed out";
        result_json["results"] = nlohmann::json::array();
        result_json["total"] = 0;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto query_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    result_json["time_ms"] = query_time_ms;

    std::string response_json = result_json.dump();

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        // LRU eviction
        if (query_cache_.size() >= MAX_CACHE_SIZE) {
            auto oldest = std::min_element(query_cache_.begin(), query_cache_.end(), [](const auto& a, const auto& b) {
                return a.second.timestamp < b.second.timestamp;
            });
            if (oldest != query_cache_.end()) {
                query_cache_.erase(oldest);
            }
        }

        query_cache_[query_text] = {response_json, std::chrono::steady_clock::now()};
    }

    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + response_json;
}

const std::vector<nlohmann::json> SearchPlugin::MOCK_RESULTS = {
    {{"id", 1},
     {"title", "Introduction to Search Engines"},
     {"url", "https://example.com/search-intro"},
     {"snippet", "A comprehensive guide to how search engines work, including indexing and query processing."}       },

    {{"id", 2},
     {"title", "C++ Performance Optimization"},
     {"url", "https://example.com/cpp-optimization"},
     {"snippet", "Learn advanced techniques for optimizing C++ code, including memory layout and SIMD instructions."}},

    {{"id", 3},
     {"title", "Distributed Systems Architecture"},
     {"url", "https://example.com/distributed-systems"},
     {"snippet", "Design patterns for building scalable and reliable distributed systems in the cloud."}             }
};

nlohmann::json SearchPlugin::ExecuteQuery(const std::string& query_text, int max_results) {
    nlohmann::json result;

    if (query_text.empty()) {
        result["results"] = nlohmann::json::array();
        result["total"] = 0;
        return result;
    }

    try {
        if (!coordinator_initialized_) {
            // Return mock results in dev mode
            spdlog::info("Using mock results for query: '{}'", query_text);

            nlohmann::json results = nlohmann::json::array();
            size_t num_results = std::min(MOCK_RESULTS.size(), static_cast<size_t>(max_results));

            for (size_t i = 0; i < num_results; i++) {
                results.push_back(MOCK_RESULTS[i]);
            }

            result["results"] = results;
            result["total"] = MOCK_RESULTS.size();
            result["demo_mode"] = true;
            return result;
        }

        // Attempt to execute real query
        auto query_results = query_coordinator_->send_query_to_workers(query_text);

        // Format results
        nlohmann::json results = nlohmann::json::array();
        size_t result_count = std::min(static_cast<size_t>(max_results), query_results.size());

        for (size_t i = 0; i < result_count; i++) {
            auto doc_id = query_results[i];

            // In production, you would fetch document details from a shared store
            nlohmann::json result_item;
            result_item["id"] = doc_id;
            result_item["title"] = "Document " + std::to_string(doc_id);
            result_item["url"] = "https://example.com/" + std::to_string(doc_id);
            result_item["snippet"] = "Content for document " + std::to_string(doc_id);

            results.push_back(result_item);
        }

        result["results"] = results;
        result["total"] = query_results.size();

    } catch (const std::exception& e) {
        spdlog::error("Error executing query: {}", e.what());

        // Fallback to mock results on error
        nlohmann::json results = nlohmann::json::array();
        size_t num_results = std::min(MOCK_RESULTS.size(), static_cast<size_t>(max_results));

        for (size_t i = 0; i < num_results; i++) {
            results.push_back(MOCK_RESULTS[i]);
        }

        result["results"] = results;
        result["total"] = MOCK_RESULTS.size();
        result["error"] = e.what();
        result["fallback"] = true;
    }

    return result;
}

void SearchPlugin::CleanExpiredCache() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = query_cache_.begin(); it != query_cache_.end();) {
        if (now - it->second.timestamp > CACHE_TTL) {
            it = query_cache_.erase(it);
        } else {
            ++it;
        }
    }
}