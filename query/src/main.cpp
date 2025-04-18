#include <chrono>
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

namespace mithril {

struct SearchResult {
    std::string document_path;
    double score;
    std::string snippet;
};

}  // namespace mithril

namespace {
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signal) {
    shutdown_requested = signal;
}

void setup_signal_handling() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);
}

void print_results(const std::vector<mithril::SearchResult>& results, long long elapsed_ms) {
    if (results.empty()) {
        std::cout << "No results found.\n";
        return;
    }

    std::cout << "Found " << results.size() << " results in " << elapsed_ms << "ms:\n";

    size_t display_count = std::min(results.size(), size_t(10));
    for (size_t i = 0; i < display_count; ++i) {
        const auto& r = results[i];
        std::cout << i + 1 << ". " << r.document_path << " (score: " << std::fixed << std::setprecision(4) << r.score
                  << ")\n";
        if (!r.snippet.empty())
            std::cout << "   " << r.snippet << "\n";
    }

    if (results.size() > display_count)
        std::cout << "... and " << (results.size() - display_count) << " more results.\n";
}

void run_query_engine(const std::string& index_dir, const std::string& query = "") {

    // TODO: Load index path and whatnot
    spdlog::info("Loading index from: {}", index_dir);

    try {
        // TODO: RUN ENGINE
        // mithril::QueryEngine engine(index_dir);
        spdlog::info("Index loaded.");

        if (!query.empty()) {
            auto start = std::chrono::steady_clock::now();
            // TODO: RUn results
            // auto results = engine.search(query);
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            // TODO: PRint results
            // print_results(results, elapsed);
            return;
        }

        std::string user_query;
        std::cout << "Interactive mode. Type 'exit' to quit.\n";
        while (!shutdown_requested) {
            std::cout << "\n> ";
            if (!std::getline(std::cin, user_query) || user_query == "exit" || user_query == "quit")
                break;
            if (user_query.empty())
                continue;

            auto start = std::chrono::steady_clock::now();
            // TODO: Finish
            // auto results = engine.search(user_query);
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            // TODO: Finish
            // print_results(results, elapsed);
        }

    } catch (const std::exception& e) {
        spdlog::error("Query engine error: {}", e.what());
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <index_directory> [query]\n";
        return 1;
    }

    std::string index_dir = argv[1];
    std::string query;

    if (argc > 2) {
        for (int i = 2; i < argc; ++i) {
            if (i > 2)
                query += " ";
            query += argv[i];
        }
    }

#if !defined(NDEBUG)
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif

    setup_signal_handling();

    if (!std::filesystem::exists(index_dir)) {
        spdlog::error("Index directory not found: {}", index_dir);
        return 1;
    }

    _engine(index_dir, query);
    return shutdown_requested ? 1 : 0;
}
