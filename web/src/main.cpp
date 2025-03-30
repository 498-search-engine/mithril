#include "Server.h"

#include <atomic>
#include <csignal>
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
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <web_root>" << std::endl;
        return 1;
    }

    try {
        int port = std::stoi(argv[1]);
        std::string web_root = argv[2];

        // Initialize logging
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Starting mithril web");

        // Register signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Create HTTP server
        mithril::HttpServer server(port, web_root);
        server_ptr = &server;

        // Run the server in a separate thread so we can monitor for shutdown
        std::thread server_thread([&server]() { server.Run(); });

        // Wait for the server thread to finish (after Stop() is called from signal handler)
        server_thread.join();

        spdlog::info("Server shutdown complete");
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}