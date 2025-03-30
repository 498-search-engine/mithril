#ifndef QUERY_COORDINATOR_H_
#define QUERY_COORDINATOR_H_

#include "Parser.h"
#include "Query.h"
#include "QueryConfig.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace mithril {

class QueryResult {
public:
    QueryResult(uint32_t docId, float score) : docId_(docId), score_(score) {}
    
    uint32_t getDocId() const { return docId_; }
    float getScore() const { return score_; }
    
private:
    uint32_t docId_;
    float score_;
    // TODO: Add other result attributes (e.g., title, snippet, etc.)
};

// Forward declaration of QueryWorker
class QueryWorker;

// The main coordinator that distributes tasks to workers
class QueryCoordinator {
public:
    // Initialize QueryCoordinator with index directory and number of workers
    explicit QueryCoordinator(const std::string& index_dir, size_t num_workers = std::thread::hardware_concurrency()) 
        : index_dir_(index_dir), is_running_(true) {
        
        // Set the index path in the configuration
        query::QueryConfig::IndexPath = index_dir;
        
        // Create worker threads
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.emplace_back(std::make_unique<QueryWorker>(this, i));
        }
    }
    
    ~QueryCoordinator() {
        shutdown();
    }
    
    // Submit a query and get results asynchronously
    std::future<std::vector<QueryResult>> submitQuery(const std::string& query_string, bool use_streaming = false, int limit = 100) {
        auto task = std::make_shared<std::packaged_task<std::vector<QueryResult>()>>(
            [this, query_string, use_streaming, limit]() {
                return use_streaming ? 
                    executeStreamQuery(query_string, limit) : 
                    executeSimdQuery(query_string);
            });
        
        std::future<std::vector<QueryResult>> future = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tasks_.push([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return future;
    }
    
    // Get a task from the queue for a worker to execute
    bool getTask(std::function<void()>& task) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        condition_.wait(lock, [this] { return !tasks_.empty() || !is_running_; });
        
        if (!is_running_) {
            return false;
        }
        
        if (!tasks_.empty()) {
            task = std::move(tasks_.front());
            tasks_.pop();
            return true;
        }
        
        return false;
    }
    
    // Get the query plan for explanation/debugging
    std::string explainQuery(const std::string& query_string) {
        try {
            auto query_tree = parseQuery(query_string);
            return "Query Plan:\n" + query_tree->to_string();
        }
        catch (const std::exception& e) {
            return "Error explaining query: " + std::string(e.what());
        }
    }
    
    // Shutdown all workers
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            is_running_ = false;
        }
        condition_.notify_all();
        
        workers_.clear();
    }
    
    // Get tokens for a query string (for debugging)
    std::vector<Token> tokenizeQuery(const std::string& query_string) {
        Parser parser(query_string);
        return parser.get_tokens();
    }
    
private:
    // Execute a query using SIMD acceleration
    std::vector<QueryResult> executeSimdQuery(const std::string& query_string) {
        try {
            // Parse the query
            auto query_tree = parseQuery(query_string);
            
            // Execute the query and get document IDs
            std::vector<uint32_t> doc_ids = query_tree->evaluate();
            
            // Score and sort the results
            return scoreResults(doc_ids);
        } 
        catch (const ParseException& e) {
            throw std::runtime_error("Query parsing error: " + std::string(e.what()));
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Query execution error: " + std::string(e.what()));
        }
    }
    
    // Execute a stream-based query using ISR components
    std::vector<QueryResult> executeStreamQuery(const std::string& query_string, int limit = 100) {
        try {
            // Parse the query
            auto query_tree = parseQuery(query_string);
            
            // Generate the index stream reader
            auto isr = query_tree->generate_isr();
            
            // Process results as a stream
            std::vector<uint32_t> doc_ids;
            doc_ids.reserve(limit);
            
            int count = 0;
            while (isr->hasNext() && count < limit) {
                doc_ids.push_back(isr->currentDocID());
                isr->moveNext();
                count++;
            }
            
            // Score and sort the results
            return scoreResults(doc_ids);
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Stream query error: " + std::string(e.what()));
        }
    }
    
    // Parse a query string into a query tree
    std::unique_ptr<Query> parseQuery(const std::string& query_string) {
        // Create a parser with the query string
        Parser parser(query_string);
        
        // Parse the query into a query tree
        return parser.parse();
    }
    
    // Score and rank the results
    std::vector<QueryResult> scoreResults(const std::vector<uint32_t>& doc_ids) {
        // TODO: Implement proper ranking with relevance scoring
        
        std::vector<QueryResult> results;
        results.reserve(doc_ids.size());
        
        // For now, assign a default score of 1.0 to all results
        for (const auto& doc_id : doc_ids) {
            results.emplace_back(doc_id, 1.0f);
        }
        
        return results;
    }
    
    std::string index_dir_;
    std::vector<std::unique_ptr<QueryWorker>> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool is_running_;
    
    // TODO: Add caching for frequently executed queries
    // TODO: Add query suggestion/autocomplete functionality
    // TODO: Add query expansion/synonym handling
};

// Worker class that processes queries
class QueryWorker {
public:
    QueryWorker(QueryCoordinator* coordinator, size_t id) 
        : coordinator_(coordinator), id_(id), thread_(&QueryWorker::run, this) {}
    
    ~QueryWorker() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
private:
    void run() {
        while (true) {
            std::function<void()> task;
            bool got_task = coordinator_->getTask(task);
            
            if (!got_task) {
                break;
            }
            
            task();
        }
    }
    
    QueryCoordinator* coordinator_;
    size_t id_;
    std::thread thread_;
};

} // namespace mithril

#endif // QUERY_COORDINATOR_H_
