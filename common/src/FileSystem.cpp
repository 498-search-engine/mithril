#include "FileSystem.h"

#include "spdlog/spdlog.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <libgen.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include <sys/unistd.h>

#if defined(__APPLE__) || defined(__BSD__)
#    include <copyfile.h>
#    define HAS_FCOPYFILE 1
#elif defined(__linux__) || defined(__unix__)
#    include <sys/sendfile.h>
#    define HAS_SENDFILE 1
#else
#    error No good way to copy file
#endif

namespace {

int UnlinkCb(const char* fpath, const struct stat* /*sb*/, int /*typeflag*/, struct FTW* /*ftwbuf*/) {
    return remove(fpath);
}

}  // namespace

bool DirectoryExists(const char* path) {
    auto* dir = opendir(path);
    if (dir != nullptr) {
        closedir(dir);
        return true;
    }
    return false;
}

bool FileExists(const char* path) {
    return access(path, F_OK) != -1;
}

bool RmRf(const char* path) {
    return nftw(path, UnlinkCb, 64, FTW_DEPTH | FTW_PHYS);
}

bool CopyFile(const char* src, const char* dst) {
    bool result = true;

#if defined(HAS_FCOPYFILE)
    // macOS/BSD implementation using fcopyfile
    // Use copyfile directly with path arguments and COPYFILE_CLONE flag
    // which better preserves sparseness and other file attributes
    result = copyfile(src, dst, nullptr, COPYFILE_ALL | COPYFILE_CLONE) == 0;
    if (!result) {
        spdlog::error("copyfile failed: {}", strerror(errno));
    }

#elif defined(HAS_SENDFILE)
    int srcFd = open(src, O_RDONLY);
    if (srcFd == -1) {
        spdlog::error("copy file: open src: {}", strerror(errno));
        return false;
    }

    posix_fadvise(srcFd, 0, 0, POSIX_FADV_SEQUENTIAL);

    int dstFd = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    if (dstFd == -1) {
        // Fall back to normal mode if O_DIRECT fails
        dstFd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dstFd == -1) {
            spdlog::error("copy file: create dst: {}", strerror(errno));
            close(srcFd);
            return false;
        }
    }

    // Get source file size
    struct stat statBuf {};
    if (fstat(srcFd, &statBuf) == -1) {
        spdlog::error("copy file: stat source file: {}", strerror(errno));
        close(srcFd);
        close(dstFd);
        return false;
    }

    // Allocate the full size for the destination file
    if (ftruncate(dstFd, statBuf.st_size) == -1) {
        spdlog::error("copy file: truncate: {}", strerror(errno));
        close(srcFd);
        close(dstFd);
        return false;
    }

    off_t dataOffset = 0;
    off_t holeOffset;
    off_t fileEnd = statBuf.st_size;

    // Process each data segment separately
    while (dataOffset < fileEnd) {
        // Find next data segment
        dataOffset = lseek(srcFd, dataOffset, SEEK_DATA);
        if (dataOffset == -1) {
            if (errno == ENXIO) {
                // No more data segments
                break;
            }
            spdlog::error("copy file: lseek data segment: {}", strerror(errno));
            result = false;
            break;
        }

        // Find the hole after this data segment
        holeOffset = lseek(srcFd, dataOffset, SEEK_HOLE);
        if (holeOffset == -1) {
            spdlog::error("copy file: lseek hole: {}", strerror(errno));
            result = false;
            break;
        }

        // Calculate size of this data segment
        long segmentSize = holeOffset - dataOffset;

        posix_fadvise(srcFd, dataOffset, segmentSize, POSIX_FADV_WILLNEED);

        // Copy the data segment using sendfile
        off_t offset = dataOffset;
        ssize_t bytesSent;
        long remaining = segmentSize;

        // Position destination file pointer
        if (lseek(dstFd, dataOffset, SEEK_SET) == -1) {
            spdlog::error("copy file: lseek destination: {}", strerror(errno));
            result = false;
            break;
        }

        while (remaining > 0) {
            bytesSent = sendfile(dstFd, srcFd, &offset, remaining);
            if (bytesSent <= 0) {
                if (errno == EINTR) {
                    // Retry on interrupt
                    continue;
                }
                spdlog::error("copy file: sendfile: {}", strerror(errno));
                result = false;
                break;
            }
            remaining -= bytesSent;
        }

        if (!result) {
            break;
        }

        // Move to next potential data segment
        dataOffset = holeOffset;
    }

    posix_fadvise(dstFd, 0, 0, POSIX_FADV_DONTNEED);

    close(srcFd);
    close(dstFd);
#endif

    return result;
}

std::string Dirname(const char* path) {
    auto len = strlen(path);
    auto pathCopy = std::vector<char>{};
    pathCopy.resize(len + 1);
    memcpy(pathCopy.data(), path, len + 1);

    return std::string{dirname(pathCopy.data())};
}
