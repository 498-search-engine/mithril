#ifndef COMMON_FILESYSTEM_H
#define COMMON_FILESYSTEM_H

#include <string>

bool DirectoryExists(const char* path);
bool FileExists(const char* path);
bool RmRf(const char* path);

bool CopyFile(const char* src, const char* dst);

std::string Dirname(const char* path);

#endif
