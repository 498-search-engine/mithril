#include "QueryManager.h"

#include "Ranker.h"
#include "TextPreprocessor.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <regex>
#include <string>
#include <spdlog/spdlog.h>

// If after RESULTS_REQUIRED_TO_SHORTCIRCUIT documents, there are >=RESULTS_COLLECTED_AFTER_SHORTCIRCUIT results with
// score at least SCORE_FOR_SHORTCIRCUIT_REQUIRED, then we will return
#define RESULTS_REQUIRED_TO_SHORTCIRCUIT 30000
#define SCORE_FOR_SHORTCIRCUIT_REQUIRED 5500
#define RESULTS_COLLECTED_AFTER_SHORTCIRCUIT 100

// If after MINIMUM_QUOTA_FOR_RESULTS_CHECK documents, there are <REQUIRED_RESULTS_QTY documents with score
// >=REQUIRED_RESULTS_SCORE, we end ranking since there probably aren't great matches on this chunk.
#define MINIMUM_QUOTA_FOR_RESULTS_CHECK 25000
#define REQUIRED_RESULTS_SCORE 5000
#define REQUIRED_RESULTS_QTY 10

#define RESULTS_HARD_CAP 100000

// The number of milliseconds before query manager tells threads to wrap up ranking
#define SOFT_QUERY_TIMEOUT 150

namespace mithril {
using QueryResult_t = QueryManager::QueryResult;

#include <algorithm>
#include <vector>

QueryResult_t QueryManager::TopKElementsFast(QueryResult_t& results, int k) {
    auto comparator = [](const auto& a, const auto& b) {
        if (std::get<1>(a) != std::get<1>(b)) {
            return std::get<1>(a) > std::get<1>(b);
        }
        return std::get<0>(a) > std::get<0>(b);
    };

    if (results.size() <= k) {
        std::sort(results.begin(), results.end(), comparator);
        return results;
    }

    // The partial sort way (seems to be faster)
    std::partial_sort(results.begin(), results.begin() + k, results.end(), comparator);

    // The nth element way
    // std::nth_element(results.begin(), results.begin() + k, results.end(), comparator);
    // std::sort(results.begin(), results.begin() + k, comparator);

    return QueryResult_t(results.begin(), results.begin() + k);
}

QueryResult_t QueryManager::TopKFromSortedLists(const std::vector<QueryResult_t>& sortedLists, size_t k) {
    if (sortedLists.size() == 1) {
        return sortedLists[0];
    }

    auto comparator = [](const auto& a, const auto& b) {
        if (std::get<1>(a) != std::get<1>(b)) {
            return std::get<1>(a) > std::get<1>(b);
        }
        return std::get<0>(a) > std::get<0>(b);
    };

    QueryResult_t sortedList;
    std::unordered_map<size_t, int> listToIndex;

    for (size_t i = 0; i < k; ++i) {
        bool allEmpty = true;

        QueryResult_t::value_type el;
        size_t associatedList = 0;

        for (size_t j = 0; j < sortedLists.size(); ++j) {
            const auto& list = sortedLists[j];
            int index = listToIndex[j];
            if (index >= list.size()) {
                continue;
            }

            if (allEmpty) {
                el = list[index];
                associatedList = j;
                allEmpty = false;
            } else if (comparator(list[index], el)) {
                associatedList = j;
                el = list[index];
            }
        }

        if (allEmpty) {
            break;
        }

        sortedList.push_back(el);
        listToIndex[associatedList]++;
    }

    return sortedList;
}

QueryManager::QueryManager(const std::vector<std::string>& index_dirs)
    : stop_(false), query_available_(index_dirs.size(), 0), worker_completion_count_(0), curr_result_ct_(0) {
    const auto numWorkers = index_dirs.size();
    marginal_results_.resize(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        spdlog::info("Loading query engine {} at index directory {}", i, index_dirs[i]);
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

    stop_ranking_.test_and_set();

    // join all threads
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

QueryResult_t QueryManager::AnswerQuery(const std::string& query) {
    // prepare new query
    stop_ranking_.clear();

    {
        std::scoped_lock lock{mtx_};
        curr_result_ct_ = 0;
        current_query_ = query;
        worker_completion_count_ = 0;
        for (auto& result : marginal_results_) {
            result.clear();
        }

        for (auto& flag : query_available_) {
            flag = 1;
        }

        worker_cv_.notify_all();
    }

    // wait for workers to finish
    {
        std::unique_lock lock{mtx_};

        // Soft query timeout
        main_cv_.wait_for(lock, std::chrono::milliseconds(SOFT_QUERY_TIMEOUT), [this]() {
            return worker_completion_count_ == threads_.size();
        });

        stop_ranking_.test_and_set();

        // Wait for 50 ms to allow threads to wrap up everything.
        // We do not wait more than 50 ms because sometimes a thread might be stuck inside PositionIndex.
        main_cv_.wait_for(
            lock, std::chrono::milliseconds(50), [this]() { return worker_completion_count_ == threads_.size(); });

        // Wait for at least one thread to complete in case the query timeout isn't responded to fast enough (we want to
        // return at least some results.)
        main_cv_.wait(lock, [this]() { return worker_completion_count_ >= 1; });


        // TODO: this is redundant but safer
        for (auto& flag : query_available_) {
            flag = 0;
        }

        // aggregate results and return
        QueryResult_t filteredResults = TopKFromSortedLists(marginal_results_);

        spdlog::info("Returning results of size: {}", filteredResults.size());

        return filteredResults;
    }
}

void QueryManager::WorkerThread(size_t worker_id) {
    while (true) {
        std::string queryToRun;
        // wait for new query
        {
            std::unique_lock lock{mtx_};
            worker_cv_.wait(lock, [this, worker_id]() { return query_available_[worker_id] == 1 || stop_; });
            if (stop_) {
                break;
            }

            queryToRun = current_query_;
        }

        // Evaluate query over this thread's index
        auto result = query_engines_[worker_id]->EvaluateQuery(queryToRun);
        auto totalSize = result.size();
        QueryResult_t rankedResults = {};
        if (!result.empty()) {
            rankedResults = HandleRanking(queryToRun, worker_id, result);
        }

        // TODO: optimize this to not use mutex
        {
            std::scoped_lock lock{mtx_};
            curr_result_ct_ += totalSize;
            marginal_results_[worker_id] = std::move(rankedResults);
            ++worker_completion_count_;  // TODO: change this to std::atomic increment?

            // If finished, tell main thread
            if (worker_completion_count_ == threads_.size()) {
                main_cv_.notify_one();
            }

            // Record that worker finished current query
            query_available_[worker_id] = 0;
        }
    }
}

void QueryManager::SetupPositionIndexPointers(QueryEngine* query_engine,
                                              std::unordered_map<std::string, const char*>& termToPointer,
                                              const std::vector<std::pair<std::string, int>>& tokens) {
    for (const auto& token : tokens) {
        if (StopwordFilter::isStopword(token.first)) {
            continue;
        }

        const char* data = query_engine->position_index_.data_file_.data();

        auto it = query_engine->position_index_.posDict_.find(token.first);
        if (it != query_engine->position_index_.posDict_.end()) {
            termToPointer[token.first] = data + (it->second.data_offset);
        }

        std::string descToken = mithril::TokenNormalizer::decorateToken(token.first, FieldType::DESC);
        it = query_engine->position_index_.posDict_.find(descToken);
        if (it != query_engine->position_index_.posDict_.end()) {
            termToPointer[descToken] = data + (it->second.data_offset);
        }
    }
}
/**
    Assumes matches is sorted by DOCID.
*/
QueryResult_t QueryManager::HandleRanking(const std::string& query, size_t worker_id, std::vector<uint32_t>& matches) {
    spdlog::info("Ranking results of size: {}", matches.size());
    if (matches.empty()) {
        return {};
    }

    QueryResult_t rankedMatches;
    rankedMatches.reserve(matches.size());

    auto& queryEngine = query_engines_[worker_id];

    std::vector<std::pair<std::string, int>> tokens = ranking::TokenifyQuery(query);
    std::unordered_map<std::string, uint32_t> map = ranking::GetDocumentFrequencies(queryEngine->term_dict_, tokens);
    std::unordered_map<std::string, const char*> termToPointer;
    SetupPositionIndexPointers(queryEngine.get(), termToPointer, tokens);

    bool shortCircuit = matches.size() > RESULTS_REQUIRED_TO_SHORTCIRCUIT;
    uint32_t resultsCollectedAboveMin = 0;

    uint32_t rankedDocuments = 0;
    uint32_t rankedDocumentsAboveMin = 0;
    for (uint32_t match : matches) {
        if (stop_ranking_.test()) {
            spdlog::info("Stopping ranking early on query engine {} due to ranking timeout", worker_id);
            break;
        }

        const std::optional<data::Document>& docOpt = queryEngine->GetDocument(match);
        if (!docOpt.has_value()) {
            rankedMatches.push_back({match, 0, "", {}, {}});
            continue;
        }

        const data::Document& doc = docOpt.value();
        const DocInfo& docInfo = queryEngine->GetDocumentInfo(match);

        if (ranking::ContainsPornKeywords(doc.title) || ranking::ContainsPornKeywords(doc.url)) {
            continue;
        }

        uint32_t score = ranking::GetFinalScore(
            queryEngine->BM25Lib_, tokens, doc, docInfo, queryEngine->position_index_, map, termToPointer);

        rankedMatches.push_back({match, score, doc.url, doc.title, {}});

        if (shortCircuit && score >= SCORE_FOR_SHORTCIRCUIT_REQUIRED) {
            resultsCollectedAboveMin += 1;
            if (resultsCollectedAboveMin >= RESULTS_COLLECTED_AFTER_SHORTCIRCUIT) {
                spdlog::info("Query shortcircuit since enough good results found");
                break;
            }
        }

        rankedDocuments++;
        if (score >= REQUIRED_RESULTS_SCORE) {
            rankedDocumentsAboveMin++;
        }

        if (rankedDocuments >= MINIMUM_QUOTA_FOR_RESULTS_CHECK) {
            if (rankedDocumentsAboveMin < REQUIRED_RESULTS_QTY) {
                spdlog::info("Query shortcircuit since not enough good results found");
                break;
            }
        }

        if (rankedDocuments >= RESULTS_HARD_CAP) {
            break;
        }
    }

    return TopKElementsFast(rankedMatches);
}

}  // namespace mithril
