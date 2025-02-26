#include "FileSystem.h"

#include <cerrno>
#include <dirent.h>
#include <unistd.h>
#include <sys/unistd.h>

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
