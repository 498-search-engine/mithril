#include "FileSystem.h"

#include <cerrno>
#include <dirent.h>

bool DirectoryExists(const char* path) {
    auto* dir = opendir(path);
    if (dir != nullptr) {
        closedir(dir);
        return true;
    }
    return false;
}
