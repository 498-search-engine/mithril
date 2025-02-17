#include "InvertedIndex.h"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <spdlog/spdlog.h>

namespace {
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signal) {
    shutdown_requested = signal;
}

void setup_signals() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void print_progress(size_t processed, size_t start_time) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - std::chrono::steady_clock::time_point(std::chrono::seconds(start_time)))
                       .count();

    if (elapsed == 0)
        elapsed = 1;  // Avoid division by zero

    double rate = static_cast<double>(processed) / elapsed;

    std::cout << "\rProcessed " << processed << " documents"
              << " (" << std::fixed << std::setprecision(1) << rate << " docs/sec)" << std::flush;
}

bool validate_directories(const std::string& input_dir, const std::string& output_dir, bool force) {
    if (!std::filesystem::exists(input_dir)) {
        spdlog::error("Input directory does not exist: {}", input_dir);
        return false;
    }

    if (std::filesystem::exists(output_dir)) {
        if (!force) {
            spdlog::error("Output directory exists. Use --force to overwrite: {}", output_dir);
            return false;
        }
        spdlog::warn("Overwriting existing output directory: {}", output_dir);
        std::filesystem::remove_all(output_dir);
    }

    try {
        std::filesystem::create_directories(output_dir);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create output directory: {} ({})", output_dir, e.what());
        return false;
    }

    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <crawl_directory> [--output=<dir>] [--force] [--quiet]" << std::endl;
        return 1;
    }

    // Parse arguments
    std::string input_dir = argv[1];
    std::string output_dir = "index_output";
    bool force = false;
    bool quiet = false;

    for (int i = 2; i < argc; i++) {
        std::string_view arg(argv[i]);
        if (arg.starts_with("--output=")) {
            output_dir = arg.substr(9);
        } else if (arg == "--force") {
            force = true;
        } else if (arg == "--quiet") {
            quiet = true;
        }
    }

    // Setup logging
    if (quiet) {
        spdlog::set_level(spdlog::level::warn);
    } else {
#if !defined(NDEBUG)
        spdlog::set_level(spdlog::level::debug);
#else
        spdlog::set_level(spdlog::level::info);
#endif
    }

    setup_signals();

    try {
        if (!validate_directories(input_dir, output_dir, force)) {
            return 1;
        }

        spdlog::info("Starting index build...");
        spdlog::info("Input directory: {}", input_dir);
        spdlog::info("Output directory: {}", output_dir);

        mithril::IndexBuilder builder(output_dir);

        size_t processed = 0;
        auto start_time = std::chrono::steady_clock::now();

        for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
            if (shutdown_requested) {
                spdlog::warn("\nShutdown requested. Cleaning up...");
                break;
            }

            try {
                builder.add_document(entry.path().string());
                processed++;

                if (!quiet && (processed % 1000 == 0 || processed == 1)) {
                    print_progress(
                        processed,
                        std::chrono::duration_cast<std::chrono::seconds>(start_time.time_since_epoch()).count());
                }
            } catch (const std::exception& e) {
                spdlog::error("\nError processing {}: {}", entry.path().string(), e.what());
            }
        }

        if (!quiet)
            std::cout << std::endl;

        if (!shutdown_requested) {
            spdlog::info("Finalizing index...");
            builder.finalize();

            auto end_time = std::chrono::steady_clock::now();
            auto elapsed =
                std::max(1LL, std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count());

            spdlog::info("Completed! Processed {} documents in {} seconds ({:.1f} docs/sec)",
                         processed,
                         elapsed,
                         static_cast<double>(processed) / elapsed);
        }

        return shutdown_requested ? 1 : 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}