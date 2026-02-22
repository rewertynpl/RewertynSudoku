// ============================================================================
// SUDOKU HPC - THREAD_LOCAL SCRATCH POOLS
// Plik: strategie_scratch.h
// ============================================================================

#ifndef STRATEGIE_SCRATCH_H
#define STRATEGIE_SCRATCH_H

#include <array>
#include <cstdint>
#include <cstring>

namespace sudoku_strategie {
namespace scratch {

constexpr int MAX_N = 36;
constexpr int MAX_NN = MAX_N * MAX_N;  
constexpr int MAX_HOUSES = 27 * MAX_N;  
constexpr int MAX_PEERS = MAX_NN * 20;  

template<int N>
struct Level1Scratch {
    static constexpr int NN = N * N;
    std::array<uint64_t, NN> temp_masks{};
    std::array<int, N> house_positions{};  
    void reset() { temp_masks.fill(0ULL); house_positions.fill(-1); }
};

template<int N>
struct Level2Scratch {
    static constexpr int NN = N * N;
    static constexpr int BOX_COUNT = (N / 2) * (N / 2);  
    std::array<std::array<int, N>, 3> box_positions{};  
    std::array<int, N> row_positions{};
    std::array<int, N> col_positions{};
    std::array<bool, NN> eliminate{};
    
    void reset() {
        for (auto& arr : box_positions) arr.fill(-1);
        row_positions.fill(-1);
        col_positions.fill(-1);
        eliminate.fill(false);
    }
};

template<int N>
struct Level3Scratch {
    static constexpr int NN = N * N;
    std::array<uint64_t, NN> pair_masks{};
    std::array<int, 3> triple_cells{};  
    std::array<uint64_t, 3> triple_digits{};
    std::array<int, N + 1> digit_count{};  
    
    void reset() {
        pair_masks.fill(0ULL);
        triple_cells.fill(-1);
        triple_digits.fill(0ULL);
        digit_count.fill(0);
    }
};

template<int N>
struct Level4PlusScratch {
    static constexpr int NN = N * N;
    std::array<std::array<int, 4>, N> fish_rows{};  
    std::array<std::array<int, 4>, N> fish_cols{};  
    std::array<int, N> row_counts{};
    std::array<int, N> col_counts{};
    
    struct WingCell {
        int idx = -1;
        uint64_t mask = 0ULL;
        int pivot_shared = 0;  
    };
    std::array<WingCell, 3> wing_cells{};  
    
    std::array<int, N + 1> als_digits{};
    std::array<int, N> als_cells{};
    int als_digit_count = 0;
    int als_cell_count = 0;
    
    void reset() {
        for (auto& arr : fish_rows) arr.fill(-1);
        for (auto& arr : fish_cols) arr.fill(-1);
        row_counts.fill(0);
        col_counts.fill(0);
        wing_cells.fill(WingCell{});
        als_digits.fill(-1);
        als_cells.fill(-1);
        als_digit_count = 0;
        als_cell_count = 0;
    }
};

template<int N>
struct alignas(64) ThreadLocalScratch {
    Level1Scratch<N> level1;
    Level2Scratch<N> level2;
    Level3Scratch<N> level3;
    Level4PlusScratch<N> level4plus;
    
    void reset_all() {
        level1.reset(); level2.reset(); level3.reset(); level4plus.reset();
    }
};

// Instancje dla prekompilowanych wielkości
template<> struct alignas(64) ThreadLocalScratch<6> {
    Level1Scratch<6> level1; Level2Scratch<6> level2; Level3Scratch<6> level3; Level4PlusScratch<6> level4plus;
    void reset_all() { level1.reset(); level2.reset(); level3.reset(); level4plus.reset(); }
};
template<> struct alignas(64) ThreadLocalScratch<9> {
    Level1Scratch<9> level1; Level2Scratch<9> level2; Level3Scratch<9> level3; Level4PlusScratch<9> level4plus;
    void reset_all() { level1.reset(); level2.reset(); level3.reset(); level4plus.reset(); }
};
template<> struct alignas(64) ThreadLocalScratch<12> {
    Level1Scratch<12> level1; Level2Scratch<12> level2; Level3Scratch<12> level3; Level4PlusScratch<12> level4plus;
    void reset_all() { level1.reset(); level2.reset(); level3.reset(); level4plus.reset(); }
};
template<> struct alignas(64) ThreadLocalScratch<16> {
    Level1Scratch<16> level1; Level2Scratch<16> level2; Level3Scratch<16> level3; Level4PlusScratch<16> level4plus;
    void reset_all() { level1.reset(); level2.reset(); level3.reset(); level4plus.reset(); }
};

template<int N>
inline ThreadLocalScratch<N>& get_scratch() {
    thread_local static ThreadLocalScratch<N> scratch;
    return scratch;
}

inline ThreadLocalScratch<MAX_N>& get_generic_scratch() {
    thread_local static ThreadLocalScratch<MAX_N> scratch;
    return scratch;
}

} // namespace scratch
} // namespace sudoku_strategie

#endif // STRATEGIE_SCRATCH_H