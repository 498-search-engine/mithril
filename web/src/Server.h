#ifndef WEB_HTTPSERVER_H
#define WEB_HTTPSERVER_H

#include <string>

namespace mithril {

class HttpServer {
public:
    HttpServer(int port, const std::string& doc_root);
    ~HttpServer();
    
    // Run the server (blocking call)
    void Run();
    
private:
    struct ClientContext {
        int socket;
        std::string doc_root;
    };

    int port_;
    std::string doc_root_;
    int listen_socket_;
    bool running_;
    
    // Updated static thread function signature
    static void* HandleClient(void* context_ptr);
    
    // Helper functions
    static const char* GetMimeType(const std::string& filename);
    static std::string DecodeUrlPath(const std::string& path);
    static bool IsPathSafe(const std::string& path);
    static off_t GetFileSize(int fd);
    
    // Response builders
    static void SendErrorResponse(int socket, int status_code, const std::string& status_text);
    
    // Prevent copying
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
};

} // namespace mithril

#endif // WEB_HTTPSERVER_H
