#include "intersect.h"

#include <algorithm>
#include <iostream>

vector<u32> intersect_zipper_vec(const vector<u32>& a, const vector<u32>& b) {
    vector<u32> output;
    output.reserve(std::min(a.size(), b.size()));
    auto ai = a.begin(), bi = b.begin();
    while (ai != a.end() && bi != b.end()) {
        if (*ai == *bi) {
            output.push_back(*ai);
            ++ai;
            ++bi;
        } else if (*ai < *bi) {
            ++ai;
        } else {
            ++bi;
        }
    }
    return output;
}

vector<u32> intersect_gallop_vec(const vector<u32>& a, const vector<u32>& b) {
    vector<u32> output;
    output.reserve(std::min(a.size(), b.size()));
    auto ai = a.begin(), bi = b.begin();
    while (ai < a.end() && bi < b.end()) {
        // std::cout << (ai-a.begin()) << "\n";
        if (*ai == *bi) {
            output.push_back(*ai);
            ++ai;
            ++bi;
        } else if (*ai < *bi) {
            bi = std::lower_bound(++bi, b.end(), *ai);
        } else {
            ai = std::lower_bound(++ai, a.end(), *bi);
        }
    }
    return output;
}

size_t intersect_zipper(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size) {
    const auto a_end = a + a_size, b_end = b + b_size;
    const auto c_start = c;
    while (a != a_end && b != b_end) {
        if (*a == *b) [[unlikely]] {
            *c++ = *a++;
            ++b;
        } else if (*a < *b) {
            ++a;
        } else {
            ++b;
        }
    }
    return static_cast<size_t>(c - c_start);
}

size_t intersect_gallop(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size) {
    auto c_start = c;
    const auto a_end = a + a_size, b_end = b + b_size;
    while (a < a_end && b < b_end) [[unlikely]] {
        if (*a == *b) {
            *c++ = *a++;
            ++b;
        } else if (*a < *b) {
            b = std::lower_bound(++b, b_end, *a);
        } else {
            a = std::lower_bound(++a, a_end, *b);
        }
    }
    return static_cast<size_t>(c - c_start);
}

// Helper: perform exponential (galloping) search.
// Given a sorted range [begin, end) and a target value,
// first double the step until we find an element >= value,
// then use std::lower_bound to locate the first element not less than value.
static inline const uint32_t* gallop(const uint32_t* begin, const uint32_t* end, uint32_t value) {
    size_t step = 1;
    // If the first element is already not less than value, we don’t need to gallop.
    if (begin == end || *begin >= value) {
        return begin;
    }
    while (begin + step < end && *(begin + step) < value) {
        step *= 2;
    }
    // Narrow the search to the interval [begin + step/2, min(begin + step, end))
    const uint32_t* new_begin = begin + step / 2;
    const uint32_t* new_end   = std::min(begin + step, end);
    return std::lower_bound(new_begin, new_end, value);
}

// Optimized intersection function.
// This function assumes both arrays are sorted in ascending order.
size_t intersect_gallop_opt(const uint32_t* a, const uint32_t* b, uint32_t* c,
                          size_t a_size, size_t b_size) {
    // Always iterate over the smaller list.
    if (a_size > b_size) {
        return intersect_gallop(b, a, c, b_size, a_size);
    }

    auto c_start = c;
    const uint32_t* a_end = a + a_size;
    const uint32_t* b_end = b + b_size;

    while (a < a_end && b < b_end) {
        if (*a == *b) {
            *c++ = *a;
            ++a;
            ++b;
        } else if (*a < *b) {
            // *a is too small, so gallop in a to catch up to *b.
            a = gallop(a, a_end, *b);
        } else {
            // *b is too small, so gallop in b to catch up to *a.
            b = gallop(b, b_end, *a);
        }
    }
    return static_cast<size_t>(c - c_start);
}

// Optimized intersection function.
// This function assumes both arrays are sorted in ascending order.
size_t intersect_gallop_opt2(const uint32_t* a, const uint32_t* b, uint32_t* c, size_t a_size, size_t b_size) {
    auto c_start = c;
    const uint32_t* b_ptr = b;
    const uint32_t* b_end = b + b_size;

    for (const uint32_t* a_ptr = a; a_ptr < a + a_size; ++a_ptr) {
        // Advance b_ptr using galloping search to find the first element not less than *a_ptr.
        b_ptr = gallop(b_ptr, b_end, *a_ptr);
        if (b_ptr == b_end) break;  // No more candidates in b.
        if (*a_ptr == *b_ptr) {
            *c++ = *a_ptr; // Record match.
            ++b_ptr;       // Advance b_ptr to avoid duplicate matches.
        }
    }
    return c - c_start;
}

size_t intersect_simd_sse(u32* a, u32* b, u32* c, size_t a_size, size_t b_size) {
    return 0;
}