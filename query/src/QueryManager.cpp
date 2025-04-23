#include "QueryManager.h"

#include "Ranker.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <spdlog/spdlog.h>

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
    std::sort(aggregated.begin(), aggregated.end(), [](const auto& a, const auto& b) {
        if (std::get<1>(a) != std::get<1>(b)) {
            return std::get<1>(a) > std::get<1>(b);
        }
        return std::get<0>(a) > std::get<0>(b);
    });

    spdlog::info("Returning results of size: {}", aggregated.size());
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

        // if no results found, tell main thread that this worker is done
        // Could also mean nothing was found because of a parsing error
        if (result.empty()) {
            spdlog::warn("No results found for query: {}", query_to_run);
            std::scoped_lock lock{mtx_};
            ++worker_completion_count_;  
            if (worker_completion_count_ == threads_.size())
                main_cv_.notify_one();
            query_available_[worker_id] = 0;
            continue;
        }

        QueryResult_t result_ranked = HandleRanking(query_to_run, worker_id, result);
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

QueryResult_t QueryManager::HandleRanking(const std::string& query, size_t worker_id, std::vector<uint32_t>& matches) {
    spdlog::info("Ranking results of size: {}", matches.size());
    if (matches.empty()) {
        return {};
    }

    QueryResult_t ranked_matches;
    ranked_matches.reserve(matches.size());

    auto& query_engine = query_engines_[worker_id];

    // Anu - code for getting the multiplicities here
    Lexer lex(query);

    // unordered_map<string, int> of token multiplicities you can just index
    auto token_multiplicities = lex.GetTokenFrequencies();

    // you can do whatever you want with token_multiplicities now

    std::vector<std::pair<std::string, int>> tokens;
    std::string current;
    std::cout << "tokens: ";
    for (char c : query) {
        if (c == ' ') {
            if (!current.empty()) {
                std::cout << current << " ";
                tokens.emplace_back(std::move(current), 1);
                current = "";
            }
            continue;
        }
        current += c;
    }

    if (!current.empty()) {
        std::cout << current << " ";
        tokens.emplace_back(std::move(current), 1);
    }

    std::cout << std::endl;

    std::unordered_map<std::string, uint32_t> map = ranking::GetDocumentFrequencies(query_engine->term_dict_, tokens);

    for (uint32_t match : matches) {
        const std::optional<data::Document>& doc_opt = query_engine->GetDocument(match);
        if (!doc_opt.has_value()) {
            ranked_matches.push_back({match, 0, "", {}});
            continue;
        }

        const data::Document& doc = doc_opt.value();
        const DocInfo& docInfo = query_engine->GetDocumentInfo(match);

        uint32_t score = ranking::GetFinalScore(tokens, doc, docInfo, query_engine->position_index_, map);
        ranked_matches.emplace_back(match, score, doc.url, doc.title);
    }

    return ranked_matches;
}

}  // namespace mithril
