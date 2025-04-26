#include "Server.h"

#include "Plugin.h"
#include "ResponseWriter.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __linux__
#    include <sys/sendfile.h>
#elif defined(__APPLE__)
#    include <sys/uio.h>
#endif

#include <spdlog/spdlog.h>

namespace mithril {

// External plugin instance
PluginObject* Plugin = nullptr;

// MIME types table
struct MimetypeMap {
    const char *Extension, *Mimetype;
};

static const MimetypeMap MimeTable[] = {
    {  ".3g2",                                                               "video/3gpp2"},
    {  ".3gp",                                                                "video/3gpp"},
    {   ".7z",                                               "application/x-7z-compressed"},
    {  ".aac",                                                                 "audio/aac"},
    {  ".abw",                                                     "application/x-abiword"},
    {  ".arc",                                                  "application/octet-stream"},
    {  ".avi",                                                           "video/x-msvideo"},
    {  ".azw",                                              "application/vnd.amazon.ebook"},
    {  ".bin",                                                  "application/octet-stream"},
    {   ".bz",                                                        "application/x-bzip"},
    {  ".bz2",                                                       "application/x-bzip2"},
    {  ".csh",                                                         "application/x-csh"},
    {  ".css",                                                                  "text/css"},
    {  ".csv",                                                                  "text/csv"},
    {  ".doc",                                                        "application/msword"},
    { ".docx",   "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {  ".eot",                                             "application/vnd.ms-fontobject"},
    { ".epub",                                                      "application/epub+zip"},
    {  ".gif",                                                                 "image/gif"},
    {  ".htm",                                                                 "text/html"},
    { ".html",                                                                 "text/html"},
    {  ".ico",                                                              "image/x-icon"},
    {  ".ics",                                                             "text/calendar"},
    {  ".jar",                                                  "application/java-archive"},
    { ".jpeg",                                                                "image/jpeg"},
    {  ".jpg",                                                                "image/jpeg"},
    {   ".js",                                                    "application/javascript"},
    { ".json",                                                          "application/json"},
    {  ".mid",                                                                "audio/midi"},
    { ".midi",                                                                "audio/midi"},
    { ".mpeg",                                                                "video/mpeg"},
    { ".mpkg",                                       "application/vnd.apple.installer+xml"},
    {  ".odp",                           "application/vnd.oasis.opendocument.presentation"},
    {  ".ods",                            "application/vnd.oasis.opendocument.spreadsheet"},
    {  ".odt",                                   "application/vnd.oasis.opendocument.text"},
    {  ".oga",                                                                 "audio/ogg"},
    {  ".ogv",                                                                 "video/ogg"},
    {  ".ogx",                                                           "application/ogg"},
    {  ".otf",                                                                  "font/otf"},
    {  ".pdf",                                                           "application/pdf"},
    {  ".png",                                                                 "image/png"},
    {  ".ppt",                                             "application/vnd.ms-powerpoint"},
    { ".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {  ".rar",                                              "application/x-rar-compressed"},
    {  ".rtf",                                                           "application/rtf"},
    {   ".sh",                                                          "application/x-sh"},
    {  ".svg",                                                             "image/svg+xml"},
    {  ".swf",                                             "application/x-shockwave-flash"},
    {  ".tar",                                                         "application/x-tar"},
    {  ".tif",                                                                "image/tiff"},
    { ".tiff",                                                                "image/tiff"},
    {   ".ts",                                                    "application/typescript"},
    {  ".ttf",                                                                  "font/ttf"},
    {  ".vsd",                                                     "application/vnd.visio"},
    {  ".wav",                                                               "audio/x-wav"},
    { ".weba",                                                                "audio/webm"},
    { ".webm",                                                                "video/webm"},
    { ".webp",                                                                "image/webp"},
    { ".woff",                                                                 "font/woff"},
    {".woff2",                                                                "font/woff2"},
    {".xhtml",                                                     "application/xhtml+xml"},
    {  ".xls",                                                  "application/vnd.ms-excel"},
    { ".xlsx",         "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {  ".xml",                                                           "application/xml"},
    {  ".xul",                                           "application/vnd.mozilla.xul+xml"},
    {  ".zip",                                                           "application/zip"}
};

// Constants
constexpr int DEFAULT_READ_TIMEOUT_MS = 30000;  // 30s
constexpr int DEFAULT_THREAD_POOL_SIZE = 16;
constexpr int MAX_REQUEST_SIZE = 8192;  // Max request size in bytes

static ssize_t SendFileData(int out_fd, int in_fd, off_t offset, size_t count) {
#ifdef __APPLE__
    off_t len = count;
    int result = sendfile(in_fd, out_fd, offset, &len, nullptr, 0);
    return (result == -1) ? -1 : len;
#else
    return sendfile(out_fd, in_fd, nullptr, count);
#endif
}

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    size_t num_threads = (threads > 0) ? threads : DEFAULT_THREAD_POOL_SIZE;
    spdlog::info("Creating thread pool with {} threads", num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) {
                        return;
                    }
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

HttpServer::HttpServer(int port, const std::string& doc_root, size_t num_threads)
    : port_(port), doc_root_(doc_root), listen_socket_(-1), running_(false), thread_pool_(num_threads) {

    // Remove trailing slash if present
    if (!doc_root_.empty() && doc_root_.back() == '/') {
        doc_root_.pop_back();
    }

    spdlog::info("Initializing HTTP server on port {}", port_);
    spdlog::info("Document root: {}", doc_root_);
}

HttpServer::~HttpServer() {
    Stop();
}

void HttpServer::Stop() {
    if (running_.exchange(false)) {
        spdlog::info("Stopping HTTP server...");

        // Close the listening socket to unblock accept()
        if (listen_socket_ >= 0) {
            shutdown(listen_socket_, SHUT_RDWR);
            close(listen_socket_);
            listen_socket_ = -1;
        }

        // Stop the thread pool
        thread_pool_.shutdown();

        spdlog::info("HTTP server stopped");
    }
}

void HttpServer::Run() {
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    // Create socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        spdlog::error("Failed to create socket: {}", strerror(errno));
        return;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        spdlog::error("setsockopt failed: {}", strerror(errno));
        close(listen_socket_);
        listen_socket_ = -1;
        return;
    }

    // Set non-blocking mode for accept
    int flags = fcntl(listen_socket_, F_GETFL, 0);
    fcntl(listen_socket_, F_SETFL, flags | O_NONBLOCK);

    // Prepare server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    // Bind socket
    if (bind(listen_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        spdlog::error("Bind failed: {}", strerror(errno));
        close(listen_socket_);
        listen_socket_ = -1;
        return;
    }

    // Listen for connections
    if (listen(listen_socket_, SOMAXCONN) < 0) {
        spdlog::error("Listen failed: {}", strerror(errno));
        close(listen_socket_);
        listen_socket_ = -1;
        return;
    }

    running_ = true;
    spdlog::info("HTTP server running on port {}", port_);

    // Accept loop
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (running_) {
        // Use select to add timeout to accept
        fd_set readfds;
        struct timeval timeout;

        FD_ZERO(&readfds);
        FD_SET(listen_socket_, &readfds);

        timeout.tv_sec = 1;  // 1 second timeout for checking running_ flag
        timeout.tv_usec = 0;

        int select_result = select(listen_socket_ + 1, &readfds, NULL, NULL, &timeout);

        if (select_result < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted, check running_ flag
            }
            spdlog::error("Select failed: {}", strerror(errno));
            break;
        }

        if (select_result == 0) {
            // Timeout, check running_ flag
            continue;
        }

        int client_sock = accept(listen_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Non-blocking socket, no connections waiting
                continue;
            }
            if (errno == EINTR) {
                continue;  // Interrupted system call, retry
            }
            if (!running_) {
                break;  // Server is shutting down
            }
            spdlog::error("Accept failed: {}", strerror(errno));
            continue;
        }

        // Set client socket to non-blocking
        int client_flags = fcntl(client_sock, F_GETFL, 0);
        fcntl(client_sock, F_SETFL, client_flags | O_NONBLOCK);

        // Create client context
        auto context = std::make_shared<ClientContext>();
        context->socket = client_sock;
        context->doc_root = doc_root_;
        context->start_time = std::chrono::steady_clock::now();

        // Get client IP address for logging
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        spdlog::debug("New connection from {}", client_ip);

        // Queue the connection for processing
        try {
            thread_pool_.enqueue([this, context]() { HandleClient(context); });
        } catch (const std::exception& e) {
            spdlog::error("Failed to enqueue client: {}", e.what());
            close(client_sock);
        }
    }

    // Cleanup
    if (listen_socket_ >= 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
}

void HttpServer::HandleClient(std::shared_ptr<ClientContext> context) {
    // Use RAII for socket cleanup
    FileDescriptor client_fd(context->socket);

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = DEFAULT_READ_TIMEOUT_MS / 1000;
    tv.tv_usec = (DEFAULT_READ_TIMEOUT_MS % 1000) * 1000;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Buffer for reading request
    char buffer[MAX_REQUEST_SIZE] = {0};
    ssize_t bytes_read = 0;

    // Read with timeout
    auto now = std::chrono::steady_clock::now();
    auto timeout = context->start_time + std::chrono::milliseconds(DEFAULT_READ_TIMEOUT_MS);

    while (now < timeout) {
        ssize_t result = recv(client_fd, buffer + bytes_read, sizeof(buffer) - bytes_read - 1, 0);

        if (result > 0) {
            bytes_read += result;
            buffer[bytes_read] = '\0';  // Null terminate

            // Check if we have a complete request
            if (strstr(buffer, "\r\n\r\n") != nullptr || bytes_read >= MAX_REQUEST_SIZE - 1) {
                break;
            }
        } else if (result == 0) {
            // Connection closed
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, try again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                now = std::chrono::steady_clock::now();
                continue;
            }
            // Error
            spdlog::error("Error reading from socket: {}", strerror(errno));
            return;
        }
    }

    if (bytes_read <= 0) {
        spdlog::warn("Empty or timed out request");
        return;
    }

    // Parse the request
    std::string request_str(buffer, bytes_read);
    auto request = ParseRequest(request_str);

    // Log the request
    spdlog::info("HTTP Request: {} {}", request.method, request.path);

    // Check if this is a plugin path
    if (Plugin && Plugin->MagicPath(request.path)) {
        ResponseWriter writer{client_fd};
        Plugin->ProcessRequest(request_str, writer);
        return;
    }

    // Handle regular file request
    if (request.method == "GET") {
        // Decode URL
        std::string decoded_path = DecodeUrlPath(request.path);

        // Validate path
        if (!IsPathSafe(decoded_path)) {
            spdlog::warn("Attempted unsafe path access: {}", decoded_path);
            SendErrorResponse(client_fd, 403, "Forbidden");
            return;
        }

        // Default to index.html for root requests
        if (decoded_path == "/") {
            decoded_path = "/index.html";
        }

        // Extract query parameters if present
        size_t query_pos = decoded_path.find('?');
        if (query_pos != std::string::npos) {
            decoded_path = decoded_path.substr(0, query_pos);
        }

        // Build full file path
        std::string file_path = context->doc_root + decoded_path;

        // Serve the file
        SendFile(client_fd, file_path);
    } else {
        // Method not supported
        spdlog::warn("Unsupported HTTP method: {}", request.method);
        SendErrorResponse(client_fd, 501, "Not Implemented");
    }
}

HttpServer::HttpRequest HttpServer::ParseRequest(const std::string& request_data) {
    HttpRequest request;
    std::istringstream stream(request_data);
    std::string line;

    // Parse request line
    if (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path >> request.version;
    }

    // Parse headers
    while (std::getline(stream, line) && !line.empty()) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty())
            break;  // End of headers

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim leading/trailing whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            request.headers[name] = value;
        }
    }

    // Read body if Content-Length is present
    auto it = request.headers.find("Content-Length");
    if (it != request.headers.end()) {
        size_t content_length = std::stoul(it->second);
        if (content_length > 0) {
            std::string body;
            body.resize(content_length);
            stream.read(&body[0], content_length);
            request.body = body;
        }
    }

    return request;
}

void HttpServer::SendFile(int socket, const std::string& file_path) {
    // Open file
    FileDescriptor file_fd(open(file_path.c_str(), O_RDONLY));
    if (file_fd < 0) {
        spdlog::info("File not found: {}", file_path);
        SendErrorResponse(socket, 404, "Not Found");
        return;
    }

    // Get file info
    struct stat file_info;
    if (fstat(file_fd, &file_info) != 0) {
        spdlog::error("Failed to get file info: {}", strerror(errno));
        SendErrorResponse(socket, 500, "Internal Server Error");
        return;
    }

    // Check if it's a directory
    if (S_ISDIR(file_info.st_mode)) {
        spdlog::info("Requested path is a directory: {}", file_path);
        SendErrorResponse(socket, 403, "Forbidden");
        return;
    }

    // Get file size
    off_t file_size = file_info.st_size;

    // Get content type
    const char* content_type = GetMimeType(file_path);

    // Send headers
    std::string headers = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: " +
                          std::string(content_type) +
                          "\r\n"
                          "Content-Length: " +
                          std::to_string(file_size) +
                          "\r\n"
                          "Connection: close\r\n"
                          "Server: MithrilSearch/1.0\r\n\r\n";

    send(socket, headers.c_str(), headers.length(), 0);

    // Send file content
    if (file_size > 0) {
        off_t offset = 0;
        while (offset < file_size) {
            ssize_t sent = SendFileData(socket, file_fd, offset, file_size - offset);
            if (sent <= 0) {
                if (errno == EINTR || errno == EAGAIN) {
                    continue;  // Retry on interrupt or would block
                }
                spdlog::error("Failed to send file: {}", strerror(errno));
                break;
            }
            offset += sent;
        }
    }

    spdlog::debug("Sent file: {} ({} bytes)", file_path, file_size);
}

void HttpServer::SendResponse(int socket,
                              int status_code,
                              const std::string& status_text,
                              const std::string& content_type,
                              const std::string& body) {
    std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n" +
                           "Content-Type: " + content_type + "\r\n" +
                           "Content-Length: " + std::to_string(body.length()) + "\r\n" + "Connection: close\r\n" +
                           "Server: MithrilSearch/1.0\r\n\r\n" + body;

    send(socket, response.c_str(), response.length(), 0);
}

void HttpServer::SendErrorResponse(int socket, int status_code, const std::string& status_text) {
    std::string body = "<html><head><title>" + std::to_string(status_code) + " " + status_text + "</title></head>" +
                       "<body><h1>" + std::to_string(status_code) + " " + status_text + "</h1>" +
                       "<p>mithril web</p></body></html>";

    SendResponse(socket, status_code, status_text, "text/html", body);
    spdlog::debug("Sent error response: {} {}", status_code, status_text);
}

const char* HttpServer::GetMimeType(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }

    const char* extension = filename.c_str() + dot_pos;

    // Binary search in the MIME table
    int left = 0;
    int right = sizeof(MimeTable) / sizeof(MimeTable[0]) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(MimeTable[mid].Extension, extension);
        if (cmp == 0) {
            return MimeTable[mid].Mimetype;
        }
        if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return "application/octet-stream";
}

std::string HttpServer::DecodeUrlPath(const std::string& path) {
    std::string result;
    result.reserve(path.length());

    for (size_t i = 0; i < path.length(); i++) {
        if (path[i] == '%' && i + 2 < path.length()) {
            // Handle percent encoding
            char c1 = path[i + 1];
            char c2 = path[i + 2];

            int v1 = (c1 >= '0' && c1 <= '9')   ? c1 - '0'
                     : (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10
                     : (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10
                                                : -1;

            int v2 = (c2 >= '0' && c2 <= '9')   ? c2 - '0'
                     : (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10
                     : (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10
                                                : -1;

            if (v1 >= 0 && v2 >= 0) {
                result += static_cast<char>((v1 << 4) | v2);
                i += 2;
            } else {
                result += path[i];
            }
        } else if (path[i] == '+') {
            result += ' ';
        } else {
            result += path[i];
        }
    }

    return result;
}

bool HttpServer::IsPathSafe(const std::string& path) {
    // Path must start with '/'
    if (path.empty() || path[0] != '/') {
        return false;
    }

    // Check for directory traversal attempts
    int depth = 0;
    size_t pos = 0;

    while (pos < path.length()) {
        // Check for consecutive slashes
        if (path[pos] == '/' && pos + 1 < path.length() && path[pos + 1] == '/') {
            return false;
        }

        // Check for ".."
        if (path[pos] == '.' && pos + 1 < path.length() && path[pos + 1] == '.') {
            if (pos + 2 >= path.length() || path[pos + 2] == '/') {
                depth--;
                if (depth < 0) {
                    return false;  // Trying to go above root
                }
                pos += 2;
                continue;
            }
        }

        if (path[pos] == '/') {
            depth++;
        }

        pos++;
    }

    return true;
}

off_t HttpServer::GetFileSize(int fd) {
    struct stat file_info;
    if (fstat(fd, &file_info) != 0) {
        return -1;
    }

    if (S_ISDIR(file_info.st_mode)) {
        return -1;  // It's a directory
    }

    return file_info.st_size;
}

std::string HttpServer::GetQueryParam(const std::string& path, const std::string& param) {
    size_t query_start = path.find('?');
    if (query_start == std::string::npos) {
        return "";
    }

    std::string query = path.substr(query_start + 1);
    std::string param_prefix = param + "=";
    size_t param_pos = query.find(param_prefix);

    if (param_pos == std::string::npos) {
        return "";
    }

    size_t value_start = param_pos + param_prefix.length();
    size_t value_end = query.find('&', value_start);

    if (value_end == std::string::npos) {
        return query.substr(value_start);
    }

    return query.substr(value_start, value_end - value_start);
}

}  // namespace mithril
