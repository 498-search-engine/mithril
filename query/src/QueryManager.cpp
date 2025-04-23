#include "QueryManager.h"

#include "Ranker.h"
#include "TextPreprocessor.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <spdlog/spdlog.h>

#define RESULTS_REQUIRED_TO_SHORTCIRCUIT 50000
#define SCORE_FOR_SHORTCIRCUIT_REQUIRED 5500
#define RESULTS_COLLECTED_AFTER_SHORTCIRCUIT 100

namespace mithril {
using QueryResult_t = QueryManager::QueryResult;

namespace {
#include <algorithm>
#include <vector>

QueryResult_t TopKElementsFast(QueryResult_t& results, int k = 50) {
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

QueryResult_t TopKFromSortedLists(const std::vector<QueryResult_t>& sortedLists, size_t k = 50) {
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
}  // namespace


QueryManager::QueryManager(const std::vector<std::string>& index_dirs)
    : stop_(false), query_available_(index_dirs.size(), 0), worker_completion_count_(0) {
    const auto num_workers = index_dirs.size();
    marginal_results_.resize(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        spdlog::info("about to query manager for {}", index_dirs[i]);
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
    QueryResult_t filtered_results = TopKFromSortedLists(marginal_results_);

    spdlog::info("Returning results of size: {}", filtered_results.size());
    return filtered_results;
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

        QueryResult_t result_ranked = {};
        if (!result.empty()) {
            result_ranked = HandleRanking(query_to_run, worker_id, result);
        }
        
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

/**
    Assumes matches is sorted by DOCID.
*/
QueryResult_t QueryManager::HandleRanking(const std::string& query, size_t worker_id, std::vector<uint32_t>& matches) {
    spdlog::info("Ranking results of size: {}", matches.size());
    if (matches.empty()) {
        return {};
    }

    QueryResult_t ranked_matches;
    ranked_matches.reserve(matches.size());

    auto& query_engine = query_engines_[worker_id];

    // Parse query terms
    Lexer lex(query);
    auto token_multiplicities = lex.GetTokenFrequencies();

    // Extract actual query terms (excluding operators)
    std::vector<std::pair<std::string, int>> query_terms;
    for (const auto& [term, count] : token_multiplicities) {
        if (term != "AND" && term != "OR" && term != "NOT" && term.length() >= 2) {
            query_terms.emplace_back(term, count);
        }
    }

    // Get document frequencies for ranking
    std::unordered_map<std::string, uint32_t> doc_freq_map =
        ranking::GetDocumentFrequencies(query_engine->term_dict_, query_terms);

    // Prepare term pointer map for more efficient position access
    std::unordered_map<std::string, const char*> term_to_pointer;
    const char* data = query_engine->position_index_.data_file_.data();
    
    for (const auto& [term, _] : query_terms) {
        if (StopwordFilter::isStopword(term)) {
            continue;
        }

        // Regular term
        auto it = query_engine->position_index_.posDict_.find(term);
        if (it != query_engine->position_index_.posDict_.end()) {
            term_to_pointer[term] = data + (it->second.data_offset);
        } else {
            term_to_pointer[term] = nullptr;
        }

        // Decorated term
        std::string descToken = mithril::TokenNormalizer::decorateToken(term, FieldType::DESC);
        it = query_engine->position_index_.posDict_.find(descToken);
        if (it != query_engine->position_index_.posDict_.end()) {
            term_to_pointer[descToken] = data + (it->second.data_offset);
        }
    }

    // Add shortcircuit logic
    bool shortCircuit = matches.size() > RESULTS_REQUIRED_TO_SHORTCIRCUIT;
    uint32_t resultsCollectedAboveMin = 0;

    for (uint32_t match : matches) {
        const std::optional<data::Document>& doc_opt = query_engine->GetDocument(match);
        if (!doc_opt.has_value()) {
            // Add empty result when document not found
            ranked_matches.push_back({match, 0, "", {}, {}});
            continue;
        }

        const data::Document& doc = doc_opt.value();
        const DocInfo& docInfo = query_engine->GetDocumentInfo(match);

        // Calculate score using BM25Lib from main
        uint32_t score = ranking::GetFinalScore(
            query_engine->BM25Lib_, query_terms, doc, docInfo, query_engine->position_index_, doc_freq_map, term_to_pointer);

        // Collect position data for query terms
        TermPositionMap term_positions;

        // Only collect positions for actual query terms, not operators
        for (const auto& [term, _] : query_terms) {
            // Skip stopwords
            if (StopwordFilter::isStopword(term)) {
                continue;
            }
            
            // Get positions for the term in this document
            std::vector<uint16_t> positions = query_engine->position_index_.getPositions(term, match);

            // Only keep first 2 positions to keep data size small
            if (positions.size() > 2) {
                positions.resize(2);
            }

            // Only add non-empty position lists
            if (!positions.empty()) {
                term_positions[term] = std::move(positions);
            }
            
            // Also check for decorated term positions
            std::string descToken = mithril::TokenNormalizer::decorateToken(term, FieldType::DESC);
            std::vector<uint16_t> descPositions = query_engine->position_index_.getPositions(descToken, match);
            
            if (descPositions.size() > 2) {
                descPositions.resize(2);
            }
            
            if (!descPositions.empty()) {
                term_positions[descToken] = std::move(descPositions);
            }
        }

        // Add to results with all data
        ranked_matches.emplace_back(match,                     // Document ID
                                   score,                     // Score
                                   doc.url,                   // URL
                                   doc.title,                 // Title words
                                   std::move(term_positions)  // Term positions
        );
        
        // Add shortcircuit logic
        if (shortCircuit && score >= SCORE_FOR_SHORTCIRCUIT_REQUIRED) {
            resultsCollectedAboveMin += 1;
            if (resultsCollectedAboveMin >= RESULTS_COLLECTED_AFTER_SHORTCIRCUIT) {
                break;
            }
        }
    }

    // Use TopK optimization
    return TopKElementsFast(ranked_matches);
}

}  // namespace mithril
