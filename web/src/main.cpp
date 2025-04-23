#include "Ranker.h"
#include "SearchPlugin.h"
#include "Server.h"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <spdlog/spdlog.h>

std::atomic<bool> shutdown_requested{false};
mithril::HttpServer* server_ptr = nullptr;

void signal_handler(int signal) {
    spdlog::info("Received signal {}, initiating graceful shutdown", signal);
    shutdown_requested = true;
    if (server_ptr) {
        server_ptr->Stop();
    }
}

int main(int argc, char** argv) {
    if (argc != 5 && argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <port> <web_root> <server_config_path> <index_path>" << std::endl;
        return 1;
    }

    try {
        int port = std::stoi(argv[1]);
        std::string web_root = argv[2];
        std::string server_config_path = argv[3];
        std::string index_path = argc == 5 ? argv[4] : "";

        // Initialize logging
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Starting mithril web server on port {}", port);
        spdlog::info("Web root: {}", web_root);
        spdlog::info("Server config: {}", server_config_path);
        spdlog::info("Index path: {}", index_path);

        // Check paths exist
        if (!std::filesystem::exists(web_root)) {
            spdlog::error("Web root directory doesn't exist: {}", web_root);
            return 1;
        }

        try {
            // Init ranker
            mithril::ranking::InitRanker(index_path);
        } catch (std::exception& e){
            spdlog::info("ranker was not initialized, make sure this is distributed one");
        }

        // Create and init search plugin with both paths
        auto search_plugin = new SearchPlugin(server_config_path, index_path);
        mithril::Plugin = search_plugin;
        spdlog::info("Search plugin initialized");

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        spdlog::info("Signal handlers registered");

        // Create HTTP server
        mithril::HttpServer server(port, web_root);
        server_ptr = &server;

        // Run the server in a separate thread so we can monitor for shutdown
        spdlog::info("Starting HTTP server...");
        std::thread server_thread([&server]() { server.Run(); });

        // Wait for the server thread to finish (after Stop() is called from signal handler)
        server_thread.join();

        // Clean up resources
        delete search_plugin;
        mithril::Plugin = nullptr;

        spdlog::info("Server shutdown complete");
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}