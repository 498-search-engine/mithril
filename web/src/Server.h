#ifndef WEB_HTTPSERVER_H
#define WEB_HTTPSERVER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace mithril {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    // Add new task to the pool (using std::function for type erasure)
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    void shutdown();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

class FileDescriptor {
    int fd_;

public:
    explicit FileDescriptor(int fd) : fd_(fd) {}
    ~FileDescriptor() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }
    operator int() const { return fd_; }
    int release() {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
};

class HttpServer {
public:
    HttpServer(int port, const std::string& doc_root, size_t num_threads = std::thread::hardware_concurrency());
    ~HttpServer();

    // Run the server (blocking call)
    void Run();

    // nicly stop the server
    void Stop();

private:
    // Using shared_ptr for thread-safe reference counting of contexts
    struct ClientContext {
        int socket;
        std::string doc_root;
        std::chrono::steady_clock::time_point start_time;
    };

    struct HttpRequest {
        std::string method;
        std::string path;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
    };

    // Server config
    int port_;
    std::string doc_root_;
    int listen_socket_;
    std::atomic<bool> running_;
    ThreadPool thread_pool_;

    // Connection handling
    void HandleClient(std::shared_ptr<ClientContext> context);
    // Request parsing
    HttpRequest ParseRequest(const std::string& request_data);
    // Response building helpers
    void SendResponse(int socket,
                      int status_code,
                      const std::string& status_text,
                      const std::string& content_type,
                      const std::string& body);
    void SendFile(int socket, const std::string& file_path);
    void SendErrorResponse(int socket, int status_code, const std::string& status_text);

    // Helper functions
    static const char* GetMimeType(const std::string& filename);
    static std::string DecodeUrlPath(const std::string& path);
    static bool IsPathSafe(const std::string& path);
    static off_t GetFileSize(int fd);
    static std::string GetQueryParam(const std::string& path, const std::string& param);

    // Prevent copying
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
};

}  // namespace mithril

#endif  // WEB_HTTPSERVER_H
