// #include "intersect.h"

// #include <algorithm>
// #include <iostream>

// #if defined(__x86_64__) || defined(_M_X64)
//     // Intel Mac (x86_64)
//     #include <immintrin.h> // For SSE/AVX intrinsics
// #elif defined(__arm64__) || defined(__aarch64__)
//     // Apple Silicon (ARM)
//     #include <arm_neon.h>
// #endif

// vector<u32> intersect_zipper_vec(const vector<u32>& a, const vector<u32>& b) {
//     vector<u32> output;
//     output.reserve(std::min(a.size(), b.size()));
//     auto ai = a.begin(), bi = b.begin();
//     while (ai != a.end() && bi != b.end()) {
//         if (*ai == *bi) {
//             output.push_back(*ai);
//             ++ai;
//             ++bi;
//         } else if (*ai < *bi) {
//             ++ai;
//         } else {
//             ++bi;
//         }
//     }
//     return output;
// }

// vector<u32> intersect_gallop_vec(const vector<u32>& a, const vector<u32>& b) {
//     vector<u32> output;
//     output.reserve(std::min(a.size(), b.size()));
//     auto ai = a.begin(), bi = b.begin();
//     while (ai < a.end() && bi < b.end()) {
//         // std::cout << (ai-a.begin()) << "\n";
//         if (*ai == *bi) {
//             output.push_back(*ai);
//             ++ai;
//             ++bi;
//         } else if (*ai < *bi) {
//             bi = std::lower_bound(++bi, b.end(), *ai);
//         } else {
//             ai = std::lower_bound(++ai, a.end(), *bi);
//         }
//     }
//     return output;
// }

// size_t intersect_zipper(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size) {
//     const auto a_end = a + a_size, b_end = b + b_size;
//     const auto c_start = c;
//     while (a != a_end && b != b_end) {
//         if (*a == *b) [[unlikely]] {
//             *c++ = *a++;
//             ++b;
//         } else if (*a < *b) {
//             ++a;
//         } else {
//             ++b;
//         }
//     }
//     return static_cast<size_t>(c - c_start);
// }

// size_t intersect_gallop(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size) {
//     auto c_start = c;
//     const auto a_end = a + a_size, b_end = b + b_size;
//     while (a < a_end && b < b_end) [[unlikely]] {
//         if (*a == *b) {
//             *c++ = *a++;
//             ++b;
//         } else if (*a < *b) {
//             b = std::lower_bound(++b, b_end, *a);
//         } else {
//             a = std::lower_bound(++a, a_end, *b);
//         }
//     }
//     return static_cast<size_t>(c - c_start);
// }

// // Helper: perform exponential (galloping) search.
// // Given a sorted range [begin, end) and a target value,
// // first double the step until we find an element >= value,
// // then use std::lower_bound to locate the first element not less than value.
// static inline const uint32_t* gallop(const uint32_t* begin, const uint32_t* end, uint32_t value) {
//     size_t step = 1;
//     // If the first element is already not less than value, we don't need to gallop.
//     if (begin == end || *begin >= value) {
//         return begin;
//     }
//     while (begin + step < end && *(begin + step) < value) {
//         step *= 2;
//     }
//     // Narrow the search to the interval [begin + step/2, min(begin + step, end))
//     const uint32_t* new_begin = begin + step / 2;
//     const uint32_t* new_end   = std::min(begin + step, end);
//     return std::lower_bound(new_begin, new_end, value);
// }

// // Optimized intersection function.
// // This function assumes both arrays are sorted in ascending order.
// size_t intersect_gallop_opt(const uint32_t* a, const uint32_t* b, uint32_t* c,
//                           size_t a_size, size_t b_size) {
//     // Always iterate over the smaller list.
//     if (a_size > b_size) {
//         return intersect_gallop(b, a, c, b_size, a_size);
//     }

//     auto c_start = c;
//     const uint32_t* a_end = a + a_size;
//     const uint32_t* b_end = b + b_size;

//     while (a < a_end && b < b_end) {
//         if (*a == *b) {
//             *c++ = *a;
//             ++a;
//             ++b;
//         } else if (*a < *b) {
//             // *a is too small, so gallop in a to catch up to *b.
//             a = gallop(a, a_end, *b);
//         } else {
//             // *b is too small, so gallop in b to catch up to *a.
//             b = gallop(b, b_end, *a);
//         }
//     }
//     return static_cast<size_t>(c - c_start);
// }

// // Optimized intersection function.
// // This function assumes both arrays are sorted in ascending order.
// size_t intersect_gallop_opt2(const uint32_t* a, const uint32_t* b, uint32_t* c, size_t a_size, size_t b_size) {
//     auto c_start = c;
//     const uint32_t* b_ptr = b;
//     const uint32_t* b_end = b + b_size;

//     for (const uint32_t* a_ptr = a; a_ptr < a + a_size; ++a_ptr) {
//         // Advance b_ptr using galloping search to find the first element not less than *a_ptr.
//         b_ptr = gallop(b_ptr, b_end, *a_ptr);
//         if (b_ptr == b_end) break;  // No more candidates in b.
//         if (*a_ptr == *b_ptr) {
//             *c++ = *a_ptr; // Record match.
//             ++b_ptr;       // Advance b_ptr to avoid duplicate matches.
//         }
//     }
//     return c - c_start;
// }

// size_t intersect_simd_sse(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size) {
//     // Empty arrays check
//     if (a_size == 0 || b_size == 0) {
//         return 0;
//     }
    
//     // Always process the smaller list in the outer loop
//     if (a_size > b_size) {
//         return intersect_simd_sse(b, a, c, b_size, a_size);
//     }
    
//     auto c_start = c;
//     const u32* a_end = a + a_size;
//     const u32* b_end = b + b_size;
    
// #if defined(__x86_64__) || defined(_M_X64)
//     // Intel x86_64 implementation using SSE
//     while (a + 4 <= a_end && b < b_end) {
//         // Load 4 values from array a
//         __m128i v_a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a));
        
//         // Find smallest and largest value in the current SIMD block
//         u32 min_a = _mm_extract_epi32(v_a, 0);
//         u32 max_a = min_a;
//         for (int i = 1; i < 4; i++) {
//             u32 val = _mm_extract_epi32(v_a, i);
//             min_a = std::min(min_a, val);
//             max_a = std::max(max_a, val);
//         }
        
//         // Find range in array b that could contain matches
//         const u32* b_start = std::lower_bound(b, b_end, min_a);
//         const u32* b_stop = std::upper_bound(b_start, b_end, max_a);
        
//         // If no possible matches in b, skip this block
//         if (b_start == b_stop) {
//             a += 4;
//             continue;
//         }
        
//         // Check each value in array b against all values in SIMD register
//         for (const u32* b_ptr = b_start; b_ptr < b_stop; ++b_ptr) {
//             __m128i v_b = _mm_set1_epi32(*b_ptr); // Broadcast b value to all lanes
//             __m128i cmp = _mm_cmpeq_epi32(v_a, v_b); // Compare all 4 values in parallel
//             int mask = _mm_movemask_epi8(cmp); // Get mask of matches
            
//             if (mask != 0) { // If any match found
//                 *c++ = *b_ptr; // Add match to results
//             }
//         }
        
//         a += 4; // Move to next SIMD block in a
//         b = b_stop; // Continue from where we stopped in b
//     }
// #elif defined(__arm64__) || defined(__aarch64__)
//     // Apple Silicon (ARM) implementation using NEON
//     while (a + 4 <= a_end && b < b_end) {
//         // Load 4 values from array a
//         uint32x4_t v_a = vld1q_u32(a);
        
//         // Find smallest and largest value in current SIMD block
//         u32 a_val0 = vgetq_lane_u32(v_a, 0);
//         u32 a_val1 = vgetq_lane_u32(v_a, 1); 
//         u32 a_val2 = vgetq_lane_u32(v_a, 2);
//         u32 a_val3 = vgetq_lane_u32(v_a, 3);
        
//         u32 min_a = std::min(std::min(a_val0, a_val1), std::min(a_val2, a_val3));
//         u32 max_a = std::max(std::max(a_val0, a_val1), std::max(a_val2, a_val3));
        
//         // Find range in array b that could contain matches
//         const u32* b_start = std::lower_bound(b, b_end, min_a);
//         const u32* b_stop = std::upper_bound(b_start, b_end, max_a);
        
//         // If no possible matches in b, skip this block
//         if (b_start == b_stop) {
//             a += 4;
//             continue;
//         }
        
//         // Check each value in array b against all values in SIMD register
//         for (const u32* b_ptr = b_start; b_ptr < b_stop; ++b_ptr) {
//             uint32x4_t v_b = vdupq_n_u32(*b_ptr); // Broadcast b value to all lanes
//             uint32x4_t cmp = vceqq_u32(v_a, v_b); // Compare all 4 values in parallel
            
//             // Check each lane for a match
//             if (vgetq_lane_u32(cmp, 0) || vgetq_lane_u32(cmp, 1) || 
//                 vgetq_lane_u32(cmp, 2) || vgetq_lane_u32(cmp, 3)) {
//                 *c++ = *b_ptr; // Add match to results
//             }
//         }
        
//         a += 4; // Move to next SIMD block in a
//         b = b_stop; // Continue from where we stopped in b
//     }
// #endif

//     // Process remaining elements with scalar code
//     while (a < a_end && b < b_end) {
//         if (*a == *b) {
//             *c++ = *a++;
//             ++b;
//         } else if (*a < *b) {
//             ++a;
//         } else {
//             ++b;
//         }
//     }
    
//     return c - c_start;
// }