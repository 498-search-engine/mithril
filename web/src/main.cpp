#include "Server.h"
#include <iostream>
#include <string>
#include <spdlog/spdlog.h>

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
        spdlog::info("Starting Mithril Search Server");
        
        // Create and run HTTP server
        mithril::HttpServer server(port, web_root);
        server.Run();  // This is blocking
        
        return 0;
    }
    catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}