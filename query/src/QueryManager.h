#ifndef QUERY_QUERYMANAGER_H
#define QUERY_QUERYMANAGER_H

#include "QueryEngine.h"

#include <vector>
#include <string>
#include <thread>
#include <memory>
#include <cstdint>
#include <mutex>
#include <condition_variable>

namespace mithril {

class QueryManager {
public:
    using QueryResult = std::vector<uint32_t>;

    QueryManager(const std::vector<std::string>& index_dirs);

    QueryManager(const QueryManager&) = delete;
    QueryManager& operator=(const QueryManager&) = delete;

    ~QueryManager();

    QueryResult AnswerQuery(const std::string& query);

private:
    void WorkerThread(size_t worker_id);
    

private:
    std::vector<std::thread> threads_;
    std::vector<std::unique_ptr<QueryEngine>> query_engines_;
    std::vector<QueryResult> marginal_results_;

    std::mutex mtx_;
    std::condition_variable main_cv_;
    std::condition_variable worker_cv_;

    bool stop_;
    std::vector<char> query_available_; // just vector<bool>, but vec<bool> doesn't work
    std::string current_query_;
    size_t worker_completion_count_;
};

}  // namespace mithril

#endif
