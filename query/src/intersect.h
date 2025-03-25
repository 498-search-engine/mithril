#ifndef INTERSECT_H
#define INTERSECT_H

#include <vector>
#include <cstdint>

using u32 = std::uint32_t;
using std::vector;
using std::size_t;

vector<u32> intersect_zipper_vec(const vector<u32>& a, const vector<u32>& b);
vector<u32> intersect_gallop_vec(const vector<u32>& a, const vector<u32>& b);

size_t intersect_zipper(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size);
size_t intersect_gallop(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size);
size_t intersect_gallop_opt(const uint32_t* a, const uint32_t* b, uint32_t* c, size_t a_size, size_t b_size);
size_t intersect_gallop_opt2(const uint32_t* a, const uint32_t* b, uint32_t* c, size_t a_size, size_t b_size);
size_t intersect_simd_sse(const u32* a, const u32* b, u32* c, size_t a_size, size_t b_size);

#endif