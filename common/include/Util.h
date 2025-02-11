#ifndef COMMON_UTIL_H
#define COMMON_UTIL_H

#include <string>
#include <string_view>

bool InsensitiveCharEquals(char a, char b);

bool InsensitiveStrEquals(std::string_view a, std::string_view b);

size_t FindCaseInsensitive(const std::string& s, const char* q);

std::string ToLowerCase(std::string_view s);

#endif
