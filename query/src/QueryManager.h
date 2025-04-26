/**
 * @file QueryManager.h
 * @author Christopher Davis
 * @brief Query Manager: serves queries for local machine
 * @version 0.9
 * @date 2025-04-10
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef QUERY_QUERYMANAGER_H
#define QUERY_QUERYMANAGER_H

#include "QueryEngine.h"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mithril {

/**
 * @brief Serves queries for local machine
 *
 */
class QueryManager {
public:
    using TermPositionMap = std::unordered_map<std::string, std::vector<uint16_t>>;
    using QueryResult =
        std::vector<std::tuple<uint32_t, uint32_t, std::string, std::vector<std::string>, TermPositionMap>>;

    /**
     * @brief Construct a new Query Manager object
     *
     * @param index_dirs; spawns a worker thread to serve each index
     */
    QueryManager(const std::vector<std::string>& index_dirs);

    QueryManager(const QueryManager&) = delete;
    QueryManager& operator=(const QueryManager&) = delete;

    ~QueryManager();

    /**
     * @brief Solves query string over all shards on local machine
     *
     * @param query : query in string form from user
     * @return QueryResult : list of doc id matches
     */
    QueryResult AnswerQuery(const std::string& query);

    std::vector<std::unique_ptr<QueryEngine>> query_engines_;

    size_t curr_result_ct_;

    static QueryResult TopKElementsFast(QueryResult& results, int k = 50);
    static QueryResult TopKFromSortedLists(const std::vector<QueryResult>& sortedLists, size_t k = 50);

private:
    void WorkerThread(size_t worker_id);
    QueryResult HandleRanking(const std::string& query, size_t worker_id, std::vector<uint32_t>& matches);

    std::vector<std::thread> threads_;
    std::vector<QueryResult> marginal_results_;

    std::mutex mtx_;
    std::condition_variable main_cv_;
    std::condition_variable worker_cv_;

    bool stop_;
    std::vector<char> query_available_;  // just vector<bool>, but vec<bool> doesn't work
    std::string current_query_;
    size_t worker_completion_count_;
};

}  // namespace mithril

#endif
