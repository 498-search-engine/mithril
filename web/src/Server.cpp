#include "Server.h"
#include "Plugin.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <sstream>

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
    // Copy your existing MimeTable here or include it from a common file
    {  ".3g2", "video/3gpp2"},
    {  ".3gp", "video/3gpp"},
    {   ".7z", "application/x-7z-compressed"},
    {  ".aac", "audio/aac"},
    {  ".abw", "application/x-abiword"},
    {  ".arc", "application/octet-stream"},
    {  ".avi", "video/x-msvideo"},
    {  ".azw", "application/vnd.amazon.ebook"},
    {  ".bin", "application/octet-stream"},
    {   ".bz", "application/x-bzip"},
    {  ".bz2", "application/x-bzip2"},
    {  ".csh", "application/x-csh"},
    {  ".css", "text/css"},
    {  ".csv", "text/csv"},
    {  ".doc", "application/msword"},
    { ".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {  ".eot", "application/vnd.ms-fontobject"},
    { ".epub", "application/epub+zip"},
    {  ".gif", "image/gif"},
    {  ".htm", "text/html"},
    { ".html", "text/html"},
    {  ".ico", "image/x-icon"},
    {  ".ics", "text/calendar"},
    {  ".jar", "application/java-archive"},
    { ".jpeg", "image/jpeg"},
    {  ".jpg", "image/jpeg"},
    {   ".js", "application/javascript"},
    { ".json", "application/json"},
    {  ".mid", "audio/midi"},
    { ".midi", "audio/midi"},
    { ".mpeg", "video/mpeg"},
    { ".mpkg", "application/vnd.apple.installer+xml"},
    {  ".odp", "application/vnd.oasis.opendocument.presentation"},
    {  ".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {  ".odt", "application/vnd.oasis.opendocument.text"},
    {  ".oga", "audio/ogg"},
    {  ".ogv", "video/ogg"},
    {  ".ogx", "application/ogg"},
    {  ".otf", "font/otf"},
    {  ".pdf", "application/pdf"},
    {  ".png", "image/png"},
    {  ".ppt", "application/vnd.ms-powerpoint"},
    { ".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {  ".rar", "application/x-rar-compressed"},
    {  ".rtf", "application/rtf"},
    {   ".sh", "application/x-sh"},
    {  ".svg", "image/svg+xml"},
    {  ".swf", "application/x-shockwave-flash"},
    {  ".tar", "application/x-tar"},
    {  ".tif", "image/tiff"},
    { ".tiff", "image/tiff"},
    {   ".ts", "application/typescript"},
    {  ".ttf", "font/ttf"},
    {  ".vsd", "application/vnd.visio"},
    {  ".wav", "audio/x-wav"},
    { ".weba", "audio/webm"},
    { ".webm", "video/webm"},
    { ".webp", "image/webp"},
    { ".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {  ".xls", "application/vnd.ms-excel"},
    { ".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {  ".xml", "application/xml"},
    {  ".xul", "application/vnd.mozilla.xul+xml"},
    {  ".zip", "application/zip"}
};

// Platform-specific sendfile wrapper
static ssize_t SendFile(int out_fd, int in_fd, off_t offset, size_t count) {
#ifdef __APPLE__
    off_t len = count;
    int result = sendfile(in_fd, out_fd, offset, &len, nullptr, 0);
    return (result == -1) ? -1 : len;
#else
    return sendfile(out_fd, in_fd, nullptr, count);
#endif
}

// Safe file descriptor wrapper
class FileDescriptor {
    int fd_;
public:
    explicit FileDescriptor(int fd) : fd_(fd) {}
    ~FileDescriptor() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    operator int() const { return fd_; }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
};

HttpServer::HttpServer(int port, const std::string& doc_root)
    : port_(port), doc_root_(doc_root), listen_socket_(-1), running_(false) {
    
    // Remove trailing slash if present
    if (!doc_root_.empty() && doc_root_.back() == '/') {
        doc_root_.pop_back();
    }
}

HttpServer::~HttpServer() {
    if (listen_socket_ >= 0) {
        close(listen_socket_);
    }
}

void HttpServer::Run() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
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
    spdlog::info("Serving files from: {}", doc_root_);
    
    // Accept and handle connections
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (running_) {
        int client_sock = accept(listen_socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted system call, retry
            }
            spdlog::error("Accept failed: {}", strerror(errno));
            continue;
        }
        
        ClientContext* context = new ClientContext{client_sock, doc_root_};
        if (!context) {
            spdlog::error("Failed to allocate memory for client context");
            close(client_sock);
            continue;
        }
        
        pthread_t thread;
        if (pthread_create(&thread, nullptr, HandleClient, context) != 0) {
            spdlog::error("Failed to create thread: {}", strerror(errno));
            delete context;
            close(client_sock);
            continue;
        }
        
        pthread_detach(thread);
    }

    // Cleanup
    close(listen_socket_);
    listen_socket_ = -1;
}

void* HttpServer::HandleClient(void* socket_ptr) {
    // Changed parameter name to match usage in function
    std::unique_ptr<ClientContext> context(static_cast<ClientContext*>(socket_ptr));
    int sock = context->socket;
    std::string doc_root = context->doc_root;
    
    // Read request
    char buffer[8192] = {0};
    ssize_t bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        return nullptr;
    }
    
    // Parse request line
    std::string request(buffer, bytes_read);
    std::istringstream request_stream(request);
    std::string method, path, version;
    
    request_stream >> method >> path >> version;
    
    if (method.empty() || path.empty()) {
        SendErrorResponse(sock, 400, "Bad Request");
        return nullptr;
    }
    
    // Check if this is a plugin path
    if (Plugin && Plugin->MagicPath(path)) {
        std::string response = Plugin->ProcessRequest(request);
        send(sock, response.c_str(), response.length(), 0);
        return nullptr;
    }
    
    // Handle regular file request
    if (method == "GET") {
        // Decode URL
        std::string decoded_path = DecodeUrlPath(path);
        
        // Validate path
        if (!IsPathSafe(decoded_path)) {
            SendErrorResponse(sock, 403, "Forbidden");
            return nullptr;
        }
        
        // Default to index.html for root requests
        if (decoded_path == "/") {
            decoded_path = "/index.html";
        }
        
        // Build full file path using the passed doc_root
        std::string file_path = doc_root + decoded_path;
        
        // Open file
        FileDescriptor file_fd(open(file_path.c_str(), O_RDONLY));
        if (file_fd < 0) {
            SendErrorResponse(sock, 404, "Not Found");
            return nullptr;
        }
        
        // Check if it's a directory
        off_t file_size = GetFileSize(file_fd);
        if (file_size < 0) {
            SendErrorResponse(sock, 403, "Forbidden");
            return nullptr;
        }
        
        // Send response headers
        std::string headers = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: " + std::string(GetMimeType(file_path)) + "\r\n"
                             "Content-Length: " + std::to_string(file_size) + "\r\n"
                             "Connection: close\r\n\r\n";
        
        if (send(sock, headers.c_str(), headers.length(), 0) < 0) {
            return nullptr;
        }
        
        // Send file content using sendfile
        size_t remaining = file_size;
        while (remaining > 0) {
            ssize_t sent = SendFile(sock, file_fd, file_size - remaining, remaining);
            if (sent < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted system call, retry
                }
                break;
            }
            remaining -= sent;
        }
    } else {
        // Method not supported
        SendErrorResponse(sock, 501, "Not Implemented");
    }
    
    return nullptr;
}

const char* HttpServer::GetMimeType(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    
    const char* extension = filename.c_str() + dot_pos;
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
            char c1 = path[i+1];
            char c2 = path[i+2];
            
            int v1 = (c1 >= '0' && c1 <= '9') ? c1 - '0' : 
                     (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10 :
                     (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10 : -1;
                     
            int v2 = (c2 >= '0' && c2 <= '9') ? c2 - '0' : 
                     (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10 :
                     (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10 : -1;
            
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
    
    if ((file_info.st_mode & S_IFMT) == S_IFDIR) {
        return -1;  // It's a directory
    }
    
    return file_info.st_size;
}

void HttpServer::SendErrorResponse(int socket, int status_code, const std::string& status_text) {
    std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n"
                         + "Content-Length: 0\r\n"
                         + "Connection: close\r\n\r\n";
    
    send(socket, response.c_str(), response.length(), 0);
}

} // namespace mithril
