#include "QueryManager.h"

#include <algorithm>
#include <mutex>
#include <string>

namespace mithril {

using QueryResult_t = QueryManager::QueryResult;

QueryManager::QueryManager(const std::vector<std::string>& index_dirs)
    : stop_(false), query_available_(index_dirs.size(), 0), worker_completion_count_(0) {
    const auto num_workers = index_dirs.size();
    marginal_results_.resize(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        query_engines_.emplace_back(std::make_unique<QueryEngine>(index_dirs[i]));
        threads_.emplace_back(&QueryManager::WorkerThread, this, i);
    }
}

QueryManager::~QueryManager() {
    // tell all workers to stop
    {
        std::scoped_lock lock{mtx_};
        stop_ = true;
        worker_cv_.notify_all();
    }

    // join all threads
    for (auto& t : threads_) {
        if (t.joinable())
            t.join();
    }
}

QueryResult_t QueryManager::AnswerQuery(const std::string& query) {
    // prepare new query
    {
        std::scoped_lock lock{mtx_};
        current_query_ = query;
        worker_completion_count_ = 0;
        for (auto& result : marginal_results_)
            result.clear();
        for (auto& flag : query_available_)
            flag = 1;
        worker_cv_.notify_all();
    }

    // wait for workers to finish
    {
        std::unique_lock lock{mtx_};
        main_cv_.wait(lock, [this]() { return worker_completion_count_ == threads_.size(); });
        for (auto& flag : query_available_)  // TODO: this is redundant but safer
            flag = 0;
    }

    // aggregate results
    QueryResult_t aggregated;
    for (const auto& marginal : marginal_results_)
        aggregated.insert(aggregated.end(), marginal.begin(), marginal.end());
    std::sort(aggregated.begin(), aggregated.end());

    return aggregated;
}

void QueryManager::WorkerThread(size_t worker_id) {
    while (true) {
        std::string query_to_run;
        // wait for new query
        {
            std::unique_lock lock{mtx_};
            worker_cv_.wait(lock, [this, worker_id]() { return query_available_[worker_id] == 1 || stop_; });
            if (stop_)
                break;
            query_to_run = current_query_;
        }

        // Evaluate query over this thread's index
        auto result = query_engines_[worker_id]->EvaluateQuery(query_to_run);
        QueryResult_t result_ranked = HandleRanking(result);
        // TODO: optimize this to not use mutex
        {
            std::scoped_lock lock{mtx_};
            marginal_results_[worker_id] = std::move(result_ranked);
            ++worker_completion_count_;  // TODO: change this to std::atomic increment?
            // if finished, tell main thread
            if (worker_completion_count_ == threads_.size())
                main_cv_.notify_one();

            // record that worker finished current query
            query_available_[worker_id] = 0;
        }
    }
}

QueryResult_t QueryManager::HandleRanking(std::vector<uint32_t>& matches) {
    QueryResult_t ranked_matches;
    ranked_matches.reserve(matches.size());

    for (uint32_t match : matches) {
        ranked_matches.push_back({match, 0});  // TODO: replace 0 with actual score
    }

    return ranked_matches;
}

}  // namespace mithril
