#ifndef INTERSECT_H
#define INTERSECT_H

#include <vector>
#include <cstdint>

using u32 = std::uint32_t;
using std::vector;
using std::size_t;



// Simple merge-based intersection algorithm
// Time complexity: O(m + n) where m and n are the sizes of the arrays
std::vector<uint32_t> intersect_simple(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result;
    result.reserve(std::min(a.size(), b.size()));  // Best case scenario
    
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            result.push_back(a[i]);
            i++;
            j++;
        } else if (a[i] < b[j]) {
            i++;
        } else {
            j++;
        }
    }
    
    return result;
}

// Union of two sorted arrays - returns sorted array with no duplicates
// Time complexity: O(m + n) where m and n are the sizes of the arrays
std::vector<uint32_t> union_simple(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> result;
    result.reserve(a.size() + b.size());  // Worst case scenario
    
    size_t i = 0, j = 0;
    
    // Process both arrays until we reach the end of one
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) {
            // Add element from first array
            result.push_back(a[i]);
            i++;
        } 
        else if (b[j] < a[i]) {
            // Add element from second array
            result.push_back(b[j]);
            j++;
        }
        else {
            // Equal elements - add only once
            result.push_back(a[i]);
            i++;
            j++;
        }
    }
    
    // Add remaining elements from first array
    while (i < a.size()) {
        result.push_back(a[i]);
        i++;
    }
    
    // Add remaining elements from second array
    while (j < b.size()) {
        result.push_back(b[j]);
        j++;
    }
    
    return result;
}

// vector<u32> intersect_zipper_vec(const vector<u32>& a, const vector<u32>& b);
// vector<u32> intersect_gallop_vec(const vector<u32>& a, const vector<u32>& b);

// size_t intersect_zipper(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size);
// size_t intersect_gallop(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size);
// size_t intersect_gallop_opt(const uint32_t* a, const uint32_t* b, uint32_t* c, size_t a_size, size_t b_size);
// size_t intersect_gallop_opt2(const uint32_t* a, const uint32_t* b, uint32_t* c, size_t a_size, size_t b_size);
// size_t intersect_simd_sse(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size);

#endif