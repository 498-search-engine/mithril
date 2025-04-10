#ifndef WEB_SEARCHPLUGIN_H
#define WEB_SEARCHPLUGIN_H

#include "Plugin.h"
#include "QueryCoordinator.h"

#include <nlohmann/json.hpp>

class SearchPlugin : public mithril::PluginObject {
public:
    SearchPlugin(const std::string& server_config_path);
    ~SearchPlugin() override;

    bool MagicPath(const std::string path) override;
    std::string ProcessRequest(std::string request) override;

private:
    std::unique_ptr<mithril::QueryCoordinator> query_coordinator_;
    bool coordinator_initialized_ = false;
    std::string config_path_;

    // LRU cache
    struct CacheEntry {
        std::string result;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::unordered_map<std::string, CacheEntry> query_cache_;
    std::mutex cache_mutex_;

    static constexpr std::chrono::seconds QUERY_TIMEOUT{3};  // 3 second timeout

    // Cache config
    static constexpr size_t MAX_CACHE_SIZE = 100;
    static constexpr std::chrono::seconds CACHE_TTL{300};  // 5 minutes

    nlohmann::json ExecuteQuery(const std::string& query_text, int max_results = 50);
    void CleanExpiredCache();
    std::string DecodeUrlString(const std::string& encoded);
    bool TryInitializeCoordinator();

    static const std::vector<nlohmann::json> MOCK_RESULTS;
};

#endif  // WEB_SEARCHPLUGIN_H
