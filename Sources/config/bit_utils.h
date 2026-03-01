#pragma once

#include <bit>
#include <cstdint>

namespace sudoku_hpc::config {

inline int bit_ctz_u64(uint64_t v) {
    if (v == 0ULL) {
        return -1;
    }
    return static_cast<int>(std::countr_zero(v));
}

inline uint64_t bit_clear_lsb_u64(uint64_t v) {
    return v & (v - 1ULL);
}

inline uint64_t bit_lsb(uint64_t v) {
    return v & (~v + 1ULL);
}

inline int single_digit_from_mask(uint64_t mask) {
    if (mask == 0ULL || (mask & (mask - 1ULL)) != 0ULL) {
        return 0;
    }
    return bit_ctz_u64(mask) + 1;
}

inline int single_digit(uint64_t mask) {
    return single_digit_from_mask(mask);
}

} // namespace sudoku_hpc::config
