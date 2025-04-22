#include "SearchPlugin.h"

#include "QueryManager.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;



const std::vector<std::pair<std::string, std::string>> SearchPlugin::MOCK_RESULTS = {
    {       "https://example.com/search-intro",   "Introduction to Search Engines"},
    {   "https://example.com/cpp-optimization",     "C++ Performance Optimization"},
    {"https://example.com/distributed-systems", "Distributed Systems Architecture"}
};

SearchPlugin::SearchPlugin(const std::string& server_config_path, const std::string& index_path)
    : config_path_(server_config_path), index_path_(index_path) {

    spdlog::info("Initializing search plugin with config: {}", server_config_path);

    // Initialize local query engine
    if (!index_path.empty()) {
        try {
            std::vector<std::string> index_dirs = {index_path};
            query_manager_ = std::make_unique<QueryManager>(index_dirs);
            engine_initialized_ = true;
            spdlog::info("Local QueryEngine initialized with index: {}", index_path);
        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize local QueryEngine: {}", e.what());
        }
    }

    // Initialize coordinator
    coordinator_initialized_ = TryInitializeCoordinator();

    if (!coordinator_initialized_ && !engine_initialized_) {
        spdlog::warn("Running in DEMO mode with mock results - no query engine available");
    }

    spdlog::info("Coordinator initalized: {}", coordinator_initialized_);
    spdlog::info("Engine initalized: {}", engine_initialized_);
}

SearchPlugin::~SearchPlugin() {
    query_coordinator_.reset();
    query_manager_.reset();
}

bool SearchPlugin::TryInitializeCoordinator() {
    try {
        std::ifstream config_file(config_path_);
        if (!config_file.good()) {
            spdlog::error("Server config file not found: {}", config_path_);
            return false;
        }

        spdlog::info("Initializing QueryCoordinator is initalized: {}", config_path_);
        query_coordinator_ = std::make_unique<mithril::QueryCoordinator>(config_path_);
        query_coordinator_->print_server_configs();
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
            auto hex_to_char = [](char c) -> int {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                return -1;
            };

            int high = hex_to_char(encoded[i + 1]);
            int low = hex_to_char(encoded[i + 2]);

            if (high != -1 && low != -1) {
                decoded += static_cast<char>((high << 4) | low);
                i += 2;
            } else {
                decoded += encoded[i];
            }
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
            max_results = std::min(max_results, 100);
        } catch (...) {
            max_results = 50;
        }
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        CleanExpiredCache();

        auto it = query_cache_.find(query_text);
        if (it != query_cache_.end()) {
            it->second.timestamp = std::chrono::steady_clock::now();
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + it->second.result;
        }
    }

    // Execute query with timeout
    auto start_time = std::chrono::steady_clock::now();

    std::string json_result;
    auto result_future = std::async(
        std::launch::async, [this, &query_text, max_results]() { return ExecuteQuery(query_text, max_results); });

    // Wait for result with timeout
    auto status = result_future.wait_for(QUERY_TIMEOUT);

    if (status == std::future_status::ready) {
        json_result = result_future.get();
    } else {
        spdlog::warn("Query timed out after {} seconds: '{}'", QUERY_TIMEOUT.count(), query_text);
        json_result = "{\"results\":[],\"total\":0,\"time_ms\":0,\"error\":\"Query timed out\"}";
    }

    // Add query time to JSON result
    auto end_time = std::chrono::steady_clock::now();
    auto query_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Replace time_ms placeholder
    size_t time_pos = json_result.find("\"time_ms\":");
    if (time_pos != std::string::npos) {
        size_t value_start = time_pos + 10;
        size_t value_end = json_result.find_first_of(",}", value_start);
        if (value_end != std::string::npos) {
            json_result.replace(value_start, value_end - value_start, std::to_string(query_time_ms));
        }
    }

    // Cache the result
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        // LRU eviction if needed
        if (query_cache_.size() >= MAX_CACHE_SIZE) {
            auto oldest = std::min_element(query_cache_.begin(), query_cache_.end(), [](const auto& a, const auto& b) {
                return a.second.timestamp < b.second.timestamp;
            });

            if (oldest != query_cache_.end()) {
                query_cache_.erase(oldest);
            }
        }

        query_cache_[query_text] = {json_result, std::chrono::steady_clock::now()};
    }

    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + json_result;
}

std::string SearchPlugin::ExecuteQuery(const std::string& query_text, int max_results) {
    std::string json;
    json.reserve(1024);  // Pre-allocate a reasonable buffer

    if (query_text.empty()) {
        json = "{\"results\":[],\"total\":0,\"time_ms\":0}";
        return json;
    }

    try {
        std::string temp = "";
        // Local query engine
        if (engine_initialized_) {
            spdlog::info("Executing local query: '{}'", query_text);

            auto results = query_manager_->AnswerQuery(query_text);
            size_t num_results = std::min(results.size(), static_cast<size_t>(max_results));
            
            json = GenerateJsonResults(results, num_results, false, temp);
            return json;
        }

        // Distributed query
        if (coordinator_initialized_) {
            spdlog::info("Executing distributed query: '{}'", query_text);

            auto doc_ids = query_coordinator_->send_query_to_workers(query_text);

            size_t num_results = std::min(doc_ids.size(), static_cast<size_t>(max_results));

            spdlog::info("⭐ Received {} results from coordinator", doc_ids.size());
            json = GenerateJsonResults(doc_ids, num_results, false, temp);
            return json;
        }

        // Demo mode with mock results
        spdlog::info("Using mock results for query: '{}'", query_text);

        std::vector<uint32_t> mock_ids;
        for (size_t i = 0; i < MOCK_RESULTS.size(); i++) {
            mock_ids.push_back(i + 1);
        }

        json = GenerateJsonResults(mock_ids, std::min(mock_ids.size(), static_cast<size_t>(max_results)), true);
        return json;

    } catch (const std::exception& e) {
        spdlog::error("Error executing query: {}", e.what());

        // Fallback to mock results
        std::vector<uint32_t> mock_ids;
        for (size_t i = 0; i < MOCK_RESULTS.size(); i++) {
            mock_ids.push_back(i + 1);
        }

        json =
            GenerateJsonResults(mock_ids, std::min(mock_ids.size(), static_cast<size_t>(max_results)), true, e.what());
        return json;
    }
}

std::string SearchPlugin::GenerateJsonResults(const std::vector<std::pair<uint32_t, uint32_t>>& doc_ids,
                                              size_t num_results,
                                              bool demo_mode,
                                              const std::string& error) {
    std::string json;
    json.reserve(1024 + num_results * 256);  // Pre-allocate based on result count

    json = "{\"results\":[";

    for (size_t i = 0; i < num_results; i++) {
        uint32_t doc_id = doc_ids[i].first;
        uint32_t score = doc_ids[i].second;

        if (i > 0)
            json += ",";

        json += "{\"id\":" + std::to_string(doc_id);

        // Handle document metadata based on mode
        if (!demo_mode && engine_initialized_) {
            auto doc_opt = query_manager_->query_engines_[0]->GetDocument(doc_id);

            if (doc_opt) {
                json += ",\"url\":\"" + EscapeJsonString(SanitizeText(doc_opt->url)) + "\"";

                std::string title = FormatDocumentTitle(doc_opt->title);
                json += ",\"title\":\"" + EscapeJsonString(SanitizeText(title)) + "\"";

                json +=
                    ",\"snippet\":\"Document #" + std::to_string(doc_id) + ", Score: " + std::to_string(score) + "\"";
            } else {
                json += ",\"url\":\"http://example.com/doc/" + std::to_string(doc_id) + "\"";
                json += ",\"title\":\"Document " + std::to_string(doc_id) + "\"";
                json += ",\"snippet\":\"Document metadata not available\"";
            }
        } else {
            // Use mock results or placeholders
            size_t mock_index = (doc_id - 1) % MOCK_RESULTS.size();

            json += ",\"url\":\"" + EscapeJsonString(MOCK_RESULTS[mock_index].first) + "\"";
            json += ",\"title\":\"" + EscapeJsonString(MOCK_RESULTS[mock_index].second) + "\"";
            std::string snippet_type = demo_mode ? "demo" : "fallback";
            json += ",\"snippet\":\"This is a " + snippet_type + " search result.\"";
        }

        json += "}";
    }

    json += "],\"total\":" + std::to_string(doc_ids.size());
    json += ",\"time_ms\":0";  // Placeholder to be replaced with actual timing

    if (demo_mode)
        json += ",\"demo_mode\":true";
    if (!error.empty())
        json += ",\"error\":\"" + EscapeJsonString(error) + "\"";

    json += "}";
    return json;
}

std::string SearchPlugin::GenerateJsonResults(const QueryResults& doc_ids,
                                              size_t num_results,
                                              bool demo_mode,
                                              const std::string& error) {
    std::string json;
    json.reserve(1024 + num_results * 256);  // Pre-allocate based on result count

    json = "{\"results\":[";

    for (size_t i = 0; i < num_results; i++) {
        uint32_t doc_id = std::get<0>(doc_ids[i]);
        uint32_t score = std::get<1>(doc_ids[i]);

        if (i > 0)
            json += ",";

        json += "{\"id\":" + std::to_string(doc_id);

        // Handle document metadata based on mode
        std::string url = std::get<2>(doc_ids[i]);
        std::string title = FormatDocumentTitle(std::get<3>(doc_ids[i]));

        if ((not url.empty()) and (not title.empty())) {
            json += ",\"url\":\"" + EscapeJsonString(SanitizeText(url)) + "\"";

            
            json += ",\"title\":\"" + EscapeJsonString(SanitizeText(title)) + "\"";

            json += ",\"snippet\":\"Document #" + std::to_string(doc_id) + ", Score: " + std::to_string(score) + "\"";
        } else {
            json += ",\"url\":\"http://example.com/doc/" + std::to_string(doc_id) + "\"";
            json += ",\"title\":\"Document " + std::to_string(doc_id) + "\"";
            json += ",\"snippet\":\"Document metadata not available\"";
        }

        json += "}";
    }

    json += "],\"total\":" + std::to_string(doc_ids.size());
    json += ",\"time_ms\":0";  // Placeholder to be replaced with actual timing

    if (demo_mode)
        json += ",\"demo_mode\":true";
    if (!error.empty())
        json += ",\"error\":\"" + EscapeJsonString(error) + "\"";

    json += "}";
    return json;
}

std::string SearchPlugin::GenerateJsonResults(const std::vector<uint32_t>& doc_ids,
                                              size_t num_results,
                                              bool demo_mode,
                                              const std::string& error) {
    std::vector<std::pair<uint32_t, uint32_t>> doc_id_pairs;
    for (size_t i = 0; i < num_results; ++i) {
        doc_id_pairs.push_back({doc_ids[i], 0});
    }

    return GenerateJsonResults(doc_id_pairs, num_results, demo_mode, error);
}

std::string SearchPlugin::EscapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.size() * 2);

    for (char c : input) {
        switch (c) {
        case '\"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (c < 32) {
                char buffer[8];
                snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                output += buffer;
            } else {
                output += c;
            }
        }
    }

    return output;
}

std::string SearchPlugin::FormatDocumentTitle(const std::vector<std::string>& title_words) {
    if (title_words.empty())
        return "Untitled Document";

    // Calculate total length for pre-allocation
    size_t total_len = 0;
    for (const auto& word : title_words) {
        total_len += word.size() + 1;  // +1 for space
    }

    std::string title;
    title.reserve(total_len);

    for (const auto& word : title_words) {
        if (!title.empty())
            title += ' ';
        title += word;
    }

    return title;
}

std::string SearchPlugin::SanitizeText(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (unsigned char c : input) {
        // Keep ASCII printable and valid UTF-8 start bytes
        if ((c >= 0x20 && c <= 0x7E) || (c >= 0xC2 && c <= 0xF4)) {
            output += c;
        } else {
            // Replace invalid characters with space
            // Avoid consecutive spaces
            if (output.empty() || output.back() != ' ') {
                output += ' ';
            }
        }
    }

    return output;
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