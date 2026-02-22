// ============================================================================
// SUDOKU HPC - STRATEGIE LOGICZNE (WSPARCIE DLA ASYMETRII I HPC)
// Plik: strategie_types.h
// ============================================================================

#ifndef STRATEGIE_TYPES_H
#define STRATEGIE_TYPES_H

#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <bit>     // std::popcount, std::countr_zero

namespace sudoku_strategie {

struct StrategyConfig {
    int box_rows = 3;
    int box_cols = 3;
    int difficulty_level = 1;
    int min_clues = 0;
    int max_clues = 0;
};

inline int get_N(const StrategyConfig& cfg) {
    return cfg.box_rows * cfg.box_cols;
}

inline int get_NN(const StrategyConfig& cfg) {
    return get_N(cfg) * get_N(cfg);
}

// ============================================================================
// ALIGNAS(64) - Ochrona przed False Sharing w architekturze NUMA/HPC
// ============================================================================
template<int N>
struct alignas(64) BoardSoA {
    static constexpr uint64_t FULL_MASK = static_cast<uint64_t>((1ULL << N) - 1ULL);
    static constexpr int NN = N * N;
    
    std::array<uint8_t, NN> values{};       
    std::array<uint64_t, NN> cand_mask{};   
    std::array<uint8_t, NN> fixed{};        
    
    std::array<uint64_t, N> row_used{};
    std::array<uint64_t, N> col_used{};
    std::array<uint64_t, N> box_used{};  
    
    int box_rows;
    int box_cols;
    
    BoardSoA() : box_rows(0), box_cols(0) {}
    
    void clear() {
        values.fill(0);
        cand_mask.fill(FULL_MASK);
        fixed.fill(0);
        row_used.fill(0ULL);
        col_used.fill(0ULL);
        box_used.fill(0ULL);
    }
    
    void init_geometry(int br, int bc) {
        box_rows = br;
        box_cols = bc;
    }
    
    inline int cell_row(int idx) const { return idx / N; }
    inline int cell_col(int idx) const { return idx % N; }
    inline int box_index(int row, int col) const {
        return (row / box_rows) * (N / box_cols) + (col / box_cols);
    }
    
    inline int max_box_index() const {
        return box_rows * box_cols - 1;
    }
    
    uint64_t candidate_mask_for_idx(int idx) const {
        if (values[static_cast<size_t>(idx)] != 0) return 0ULL;
        int r = cell_row(idx);
        int c = cell_col(idx);
        int b = box_index(r, c);
        return static_cast<uint64_t>(~(row_used[static_cast<size_t>(r)] | col_used[static_cast<size_t>(c)] | box_used[static_cast<size_t>(b)]) & FULL_MASK);
    }
    
    bool can_place(int idx, int digit) const {
        if (values[static_cast<size_t>(idx)] != 0) return false;
        uint64_t bit = static_cast<uint64_t>(1ULL << (digit - 1));
        int r = cell_row(idx);
        int c = cell_col(idx);
        int b = box_index(r, c);
        return (row_used[static_cast<size_t>(r)] & bit) == 0 &&
               (col_used[static_cast<size_t>(c)] & bit) == 0 &&
               (box_used[static_cast<size_t>(b)] & bit) == 0;
    }
    
    void place(int idx, int digit, bool mark_fixed = false) {
        uint64_t bit = static_cast<uint64_t>(1ULL << (digit - 1));
        int r = cell_row(idx);
        int c = cell_col(idx);
        int b = box_index(r, c);
        values[static_cast<size_t>(idx)] = static_cast<uint8_t>(digit);
        row_used[static_cast<size_t>(r)] |= bit;
        col_used[static_cast<size_t>(c)] |= bit;
        box_used[static_cast<size_t>(b)] |= bit;
        if (mark_fixed) fixed[static_cast<size_t>(idx)] = 1;
    }
    
    void unplace(int idx, int digit) {
        uint64_t bit = static_cast<uint64_t>(1ULL << (digit - 1));
        int r = cell_row(idx);
        int c = cell_col(idx);
        int b = box_index(r, c);
        values[static_cast<size_t>(idx)] = 0;
        row_used[static_cast<size_t>(r)] &= ~bit;
        col_used[static_cast<size_t>(c)] &= ~bit;
        box_used[static_cast<size_t>(b)] &= ~bit;
    }
    
    bool init_from_puzzle(const std::array<uint8_t, NN>& puzzle, bool mark_fixed = true) {
        clear();
        for (int idx = 0; idx < NN; ++idx) {
            int d = puzzle[static_cast<size_t>(idx)];
            if (d == 0) continue;
            if (!can_place(idx, d)) return false;
            place(idx, d, mark_fixed);
        }
        return true;
    }
    
    int clue_count() const {
        int count = 0;
        for (int idx = 0; idx < NN; ++idx) {
            if (values[static_cast<size_t>(idx)] != 0) ++count;
        }
        return count;
    }
    
    bool is_full() const {
        for (int idx = 0; idx < NN; ++idx) {
            if (values[static_cast<size_t>(idx)] == 0) return false;
        }
        return true;
    }
};

template<int N>
struct TopologyCache {
    static constexpr int NN = N * N;
    static constexpr int NUM_HOUSES = N * 3;  // Prawidłowe dla asymetrii
    int box_rows;
    int box_cols;

    std::array<std::array<int, N>, NUM_HOUSES> houses{};
    std::array<std::vector<int>, NN> peers{};
    std::array<int, NN> peer_count{};

    static constexpr int NUM_PEER_WORDS = (NN + 63) / 64;
    std::array<std::array<uint64_t, NUM_PEER_WORDS>, NN> peer_matrix_bits{};
    
    TopologyCache() : box_rows(0), box_cols(0) {}
    
    void build(int br, int bc) {
        box_rows = br;
        box_cols = bc;
        build_houses();
        build_peers();
    }
    
    void build_houses() {
        int h = 0;
        int num_box_rows = N / box_rows;
        int num_box_cols = N / box_cols;
        
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) { houses[h][c] = r * N + c; }
            ++h;
        }
        for (int c = 0; c < N; ++c) {
            for (int r = 0; r < N; ++r) { houses[h][r] = r * N + c; }
            ++h;
        }
        for (int br = 0; br < num_box_rows; ++br) {
            for (int bc = 0; bc < num_box_cols; ++bc) {
                int k = 0;
                for (int dr = 0; dr < box_rows; ++dr) {
                    for (int dc = 0; dc < box_cols; ++dc) {
                        houses[h][k++] = (br * box_rows + dr) * N + (bc * box_cols + dc);
                    }
                }
                ++h;
            }
        }
    }
    
    void build_peers() {
        for (int idx = 0; idx < NN; ++idx) {
            peers[idx].clear();
            std::array<bool, NN> seen{};
            int r = idx / N;
            int c = idx % N;
            int b = (r / box_rows) * (N / box_cols) + (c / box_cols);
            int count = 0;

            for (int i = 0; i < N; ++i) {
                int rr = r * N + i;
                if (rr != idx && !seen[static_cast<size_t>(rr)]) { peers[idx].push_back(rr); seen[static_cast<size_t>(rr)] = true; ++count; }
            }
            for (int i = 0; i < N; ++i) {
                int cc = i * N + c;
                if (cc != idx && !seen[static_cast<size_t>(cc)]) { peers[idx].push_back(cc); seen[static_cast<size_t>(cc)] = true; ++count; }
            }
            int num_box_cols = N / box_cols;
            int br = (b / num_box_cols) * box_rows;
            int bc = (b % num_box_cols) * box_cols;
            for (int dr = 0; dr < box_rows; ++dr) {
                for (int dc = 0; dc < box_cols; ++dc) {
                    int p = (br + dr) * N + (bc + dc);
                    if (p != idx && !seen[static_cast<size_t>(p)]) { peers[idx].push_back(p); seen[static_cast<size_t>(p)] = true; ++count; }
                }
            }
            peer_count[idx] = count;
            for (int w = 0; w < NUM_PEER_WORDS; ++w) { peer_matrix_bits[idx][w] = 0ULL; }
            for (int p : peers[idx]) { peer_matrix_bits[idx][p / 64] |= (1ULL << (p % 64)); }
        }
    }
    
    inline int cell_row(int idx) const { return idx / N; }
    inline int cell_col(int idx) const { return idx % N; }
    inline int box_index(int row, int col) const { return (row / box_rows) * (N / box_cols) + (col / box_cols); }
};

enum class ApplyResult : uint8_t { NoProgress = 0, Progress = 1, Contradiction = 2 };

struct StrategyStats {
    uint64_t use_count = 0;
    uint64_t hit_count = 0;
    uint64_t placements = 0;
    uint64_t elapsed_ns = 0;
};

struct LogicCertifyResult {
    bool solved = false;
    bool timed_out = false;
    int steps = 0;
    
    bool used_naked_single = false, used_hidden_single = false;
    bool used_pointing = false, used_box_line = false;
    bool used_naked_pair = false, used_hidden_pair = false;
    bool used_naked_triple = false, used_hidden_triple = false;
    bool used_x_wing = false, used_swordfish = false;
    
    std::array<StrategyStats, 64> strategy_stats{};
    
    template<int N>
    void copy_solution(const BoardSoA<N>& board, std::array<uint8_t, N*N>& out) {
        for (int i = 0; i < N*N; ++i) { out[static_cast<size_t>(i)] = board.values[static_cast<size_t>(i)]; }
    }
};

enum class StrategyId : uint8_t {
    NakedSingle = 0, HiddenSingle = 1, PointingPairs = 2, PointingTriples = 3, BoxLineReduction = 4,
    NakedPair = 5, HiddenPair = 6, NakedTriple = 7, HiddenTriple = 8, NakedQuad = 9, HiddenQuad = 10,
    XWing = 11, YWing = 12, Skyscraper = 13, TwoStringKite = 14, EmptyRectangle = 15, RemotePairs = 16,
    Swordfish = 17, XYZWing = 18, FinnedXWing = 19, Sashimi = 20, UniqueRectangle = 21, BUGPlus1 = 22,
    WWing = 23, SimpleColoring = 24, Jellyfish = 25, WXYZWing = 26, FinnedSwordfish = 27, FinnedJellyfish = 28,
    XChain = 29, XYChain = 30, ALSXZ = 31, UniqueLoop = 32, AvoidableRectangle = 33, BivalueOddagon = 34,
    Medusa3D = 35, AIC = 36, GroupedAIC = 37, GroupedXCycle = 38, ContinuousNiceLoop = 39, ALSXYWing = 40,
    ALSChain = 41, SueDeCoq = 42, DeathBlossom = 43, FrankenFish = 44, MutantFish = 45, KrakenFish = 46,
    MSLS = 47, Exocet = 48, SeniorExocet = 49, SKLoop = 50, PatternOverlay = 51, ForcingChains = 52,
    Backtracking = 63
};

inline const char* strategy_name(StrategyId id) {
    switch (id) {
        case StrategyId::NakedSingle: return "NakedSingle";
        case StrategyId::HiddenSingle: return "HiddenSingle";
        case StrategyId::PointingPairs: return "PointingPairs";
        case StrategyId::PointingTriples: return "PointingTriples";
        case StrategyId::BoxLineReduction: return "BoxLineReduction";
        case StrategyId::NakedPair: return "NakedPair";
        case StrategyId::HiddenPair: return "HiddenPair";
        case StrategyId::NakedTriple: return "NakedTriple";
        case StrategyId::HiddenTriple: return "HiddenTriple";
        case StrategyId::XWing: return "X-Wing";
        case StrategyId::YWing: return "Y-Wing";
        case StrategyId::Swordfish: return "Swordfish";
        case StrategyId::SueDeCoq: return "SueDeCoq";
        case StrategyId::MSLS: return "MSLS";
        case StrategyId::Exocet: return "Exocet";
        default: return "Unknown";
    }
}

inline int strategy_level(StrategyId id) {
    if (id <= StrategyId::HiddenSingle) return 1;
    if (id <= StrategyId::BoxLineReduction) return 2;
    if (id <= StrategyId::HiddenTriple) return 3;
    if (id <= StrategyId::RemotePairs) return 4;
    if (id <= StrategyId::SimpleColoring) return 5;
    if (id <= StrategyId::BivalueOddagon) return 6;
    if (id <= StrategyId::KrakenFish) return 7;
    if (id <= StrategyId::ForcingChains) return 8;
    return 9;
}

inline uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline int popcnt64(uint64_t v) { return std::popcount(v); }

inline int single_digit_from_mask(uint64_t mask) {
    if (mask == 0 || (mask & (mask - 1ULL)) != 0) return 0;
    return std::countr_zero(mask) + 1;
}

template<typename F>
inline void for_each_digit(uint64_t mask, F&& func) {
    while (mask != 0) {
        int d = std::countr_zero(mask) + 1;
        func(d);
        mask &= ~(1ULL << (d - 1));
    }
}

} // namespace sudoku_strategie

#endif // STRATEGIE_TYPES_H