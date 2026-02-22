// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 3
// Plik: strategylevels3.cpp
// ============================================================================

#ifndef STRATEGIE_LEVELS_3_H
#define STRATEGIE_LEVELS_3_H

#include "strategie_types.h"
#include <algorithm>
#include <cstring>

namespace sudoku_strategie {
namespace level3 {

template<int N>
ApplyResult apply_naked_pair(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int h = 0; h < topo.NUM_HOUSES; ++h) {
        for (int i = 0; i < N; ++i) {
            const int idx1 = topo.houses;
            if (board.values != 0) continue;
            
            const uint64_t mask1 = board.candidate_mask_for_idx(idx1);
            if (popcnt64(mask1) != 2) continue;
            
            for (int j = i + 1; j < N; ++j) {
                const int idx2 = topo.houses;
                if (board.values != 0) continue;
                
                const uint64_t mask2 = board.candidate_mask_for_idx(idx2);
                if (mask1 == mask2) {
                    result.used_naked_pair = true;
                    ++stats.hit_count;
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_hidden_pair(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int h = 0; h < topo.NUM_HOUSES; ++h) {
        for (int d1 = 1; d1 <= N; ++d1) {
            for (int d2 = d1 + 1; d2 <= N; ++d2) {
                const uint64_t bit1 = static_cast<uint64_t>(1ULL << (d1 - 1));
                const uint64_t bit2 = static_cast<uint64_t>(1ULL << (d2 - 1));
                
                std::array<int, N> positions{};
                int pos_count = 0;
                
                for (int k = 0; k < N; ++k) {
                    const int idx = topo.houses;
                    if (board.values != 0) continue;
                    
                    const uint64_t mask = board.candidate_mask_for_idx(idx);
                    if ((mask & bit1) || (mask & bit2)) {
                        positions = idx;
                    }
                }
                
                if (pos_count == 2) {
                    result.used_hidden_pair = true;
                    ++stats.hit_count;
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_naked_triple(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int h = 0; h < topo.NUM_HOUSES; ++h) {
        for (int i = 0; i < N; ++i) {
            const int idx1 = topo.houses;
            if (board.values != 0) continue;
            
            const uint64_t mask1 = board.candidate_mask_for_idx(idx1);
            int cnt1 = popcnt64(mask1);
            if (cnt1 < 2 || cnt1 > 3) continue;
            
            for (int j = i + 1; j < N; ++j) {
                const int idx2 = topo.houses;
                if (board.values != 0) continue;
                
                const uint64_t mask2 = board.candidate_mask_for_idx(idx2);
                const uint64_t combined12 = mask1 | mask2;
                int cnt12 = popcnt64(combined12);
                if (cnt12 > 3) continue;
                
                for (int k = j + 1; k < N; ++k) {
                    const int idx3 = topo.houses;
                    if (board.values != 0) continue;
                    
                    const uint64_t mask3 = board.candidate_mask_for_idx(idx3);
                    const uint64_t combined = combined12 | mask3;
                    
                    if (popcnt64(combined) == 3) {
                        result.used_naked_triple = true;
                        ++stats.hit_count;
                        stats.elapsed_ns += now_ns() - t0;
                        return ApplyResult::NoProgress;
                    }
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_hidden_triple(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int h = 0; h < topo.NUM_HOUSES; ++h) {
        std::array<uint64_t, N + 1> digit_pos_mask{};
        
        for (int k = 0; k < N; ++k) {
            const int idx = topo.houses;
            if (board.values != 0) continue;
            uint64_t cell_mask = board.candidate_mask_for_idx(idx);
            for (int d = 1; d <= N; ++d) {
                if (cell_mask & (1ULL << (d - 1))) {
                    digit_pos_mask |= (1ULL << k);
                }
            }
        }
        
        for (int d1 = 1; d1 <= N; ++d1) {
            if (popcnt64(digit_pos_mask) < 2) continue; 
            for (int d2 = d1 + 1; d2 <= N; ++d2) {
                for (int d3 = d2 + 1; d3 <= N; ++d3) {
                    uint64_t union_mask = digit_pos_mask | digit_pos_mask | digit_pos_mask;
                    
                    if (popcnt64(union_mask) == 3) {
                        result.used_hidden_triple = true;
                        ++stats.hit_count;
                        return ApplyResult::NoProgress; 
                    }
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
class StrategyLevel3 {
public:
    static constexpr size_t IDX_NAKED_PAIR = 0;
    static constexpr size_t IDX_HIDDEN_PAIR = 1;
    static constexpr size_t IDX_NAKED_TRIPLE = 2;
    static constexpr size_t IDX_HIDDEN_TRIPLE = 3;
    
    const TopologyCache<N>& topo;
    
    StrategyLevel3(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r;
        r = apply_naked_pair(board, topo, result, IDX_NAKED_PAIR);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_hidden_pair(board, topo, result, IDX_HIDDEN_PAIR);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_naked_triple(board, topo, result, IDX_NAKED_TRIPLE);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_hidden_triple(board, topo, result, IDX_HIDDEN_TRIPLE);
        return r;
    }
};

} // namespace level3
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_3_H