#ifndef WEB_SEARCHPLUGIN_H
#define WEB_SEARCHPLUGIN_H

#include "Plugin.h"
#include "QueryCoordinator.h"
#include "QueryEngine.h"
#include "QueryManager.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class SearchPlugin : public mithril::PluginObject {
public:
    SearchPlugin(const std::string& server_config_path, const std::string& index_path);
    ~SearchPlugin() override;

    bool MagicPath(const std::string path) override;
    std::string ProcessRequest(std::string request) override;

    struct CacheEntry {
        std::string result;
        std::chrono::steady_clock::time_point timestamp;
    };

    static std::string FormatDocumentTitle(const std::vector<std::string>& title_words);
    static std::string SanitizeText(const std::string& input);
    static std::string EscapeJsonString(const std::string& input);
    static std::string DecodeUrlString(const std::string& encoded);

    static constexpr std::chrono::seconds QUERY_TIMEOUT{3};  // 3s
    static constexpr size_t MAX_CACHE_SIZE = 100;
    static constexpr std::chrono::seconds CACHE_TTL{300};  // 5min

private:
    std::unique_ptr<mithril::QueryCoordinator> query_coordinator_;
    std::unique_ptr<QueryManager> query_manager_;
    bool coordinator_initialized_ = false;
    bool engine_initialized_ = false;
    std::string config_path_;
    std::string index_path_;

    std::unordered_map<std::string, CacheEntry> query_cache_;
    std::mutex cache_mutex_;

    std::string ExecuteQuery(const std::string& query_text, int max_results = 50);
    std::string GenerateJsonResults(const std::vector<uint32_t>& doc_ids,
                                    size_t num_results,
                                    bool demo_mode,
                                    const std::string& error = "");
    bool TryInitializeCoordinator();
    void CleanExpiredCache();

    static const std::vector<std::pair<std::string, std::string>> MOCK_RESULTS;
};

#endif  // WEB_SEARCHPLUGIN_H