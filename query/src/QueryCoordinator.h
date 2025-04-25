#ifndef QUERY_COORDINATOR_H_
#define QUERY_COORDINATOR_H_

#include "Parser.h"
#include "Query.h"
#include "QueryConfig.h"
#include "QueryManager.h"
#include "Util.h"

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace mithril {

// class QueryResult {
// public:
//     QueryResult(uint32_t docId, float score) : docId_(docId), score_(score) {}

//     uint32_t getDocId() const { return docId_; }
//     float getScore() const { return score_; }

// private:
//     uint32_t docId_;
//     float score_;
//     // TODO: Add other result attributes (e.g., title, snippet, etc.)
// };


// The main coordinator that distributes tasks to workers
class QueryCoordinator {
public:
    using QueryResults = QueryManager::QueryResult;
    struct ServerConfig {
        std::string ip;
        uint16_t port;
    };

    QueryCoordinator(const std::string& conf_path);

    void print_server_configs() const;

    std::pair<QueryResults, size_t> send_query_to_workers(const std::string& query);

    static void
    handle_worker_response(const ServerConfig& server_config, QueryResults& results, const std::string& query);

private:
    std::vector<ServerConfig> server_configs_;


    static std::pair<QueryResults, size_t> handle_worker_response(const ServerConfig& server_config,
                                                                  const std::string& query);

    // TODO: Add caching for frequently executed queries
    // TODO: Add query suggestion/autocomplete functionality
    // TODO: Add query expansion/synonym handling
};


}  // namespace mithril

#endif  // QUERY_COORDINATOR_H_
