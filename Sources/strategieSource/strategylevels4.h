// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 4
// Plik: strategylevels4.cpp
// ============================================================================

#ifndef STRATEGIE_LEVELS_4_H
#define STRATEGIE_LEVELS_4_H

#include "strategie_types.h"

namespace sudoku_strategie {
namespace level4 {

template<int N>
ApplyResult apply_x_wing(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int d = 1; d <= N; ++d) {
        uint64_t bit = 1ULL << (d - 1);
        std::array<uint64_t, N> row_masks{}; 
        
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                int idx = r * N + c;
                if (board.values == 0 && (board.candidate_mask_for_idx(idx) & bit)) {
                    row_masks |= (1ULL << c);
                }
            }
        }
        
        for (int r1 = 0; r1 < N; ++r1) {
            if (popcnt64(row_masks) != 2) continue; 
            for (int r2 = r1 + 1; r2 < N; ++r2) {
                if (popcnt64(row_masks) != 2) continue;
                
                if (popcnt64(row_masks | row_masks) == 2) {
                    result.used_x_wing = true;
                    ++stats.hit_count;
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_y_wing(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int pivot = 0; pivot < N*N; ++pivot) {
        if (board.values != 0) continue;
        const uint64_t pivot_mask = board.candidate_mask_for_idx(pivot);
        if (popcnt64(pivot_mask) != 2) continue;
        
        for (int p1 : topo.peers) {
            if (board.values != 0) continue;
            const uint64_t mask1 = board.candidate_mask_for_idx(p1);
            if (popcnt64(mask1) != 2) continue;
            
            for (int p2 : topo.peers) {
                if (p2 == p1 || board.values != 0) continue;
                const uint64_t mask2 = board.candidate_mask_for_idx(p2);
                if (popcnt64(mask2) != 2) continue;
                
                const uint64_t common1 = pivot_mask & mask1;
                const uint64_t common2 = pivot_mask & mask2;
                
                if (popcnt64(common1) == 1 && popcnt64(common2) == 1 && common1 != common2) {
                    const uint64_t pincers_common = mask1 & mask2;
                    if (popcnt64(pincers_common) == 1) {
                        result.used_x_wing = true;  
                        ++stats.hit_count;
                    }
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N> ApplyResult apply_skyscraper(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_two_string_kite(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_empty_rectangle(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_remote_pairs(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_naked_quad(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_hidden_quad(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }

template<int N>
class StrategyLevel4 {
public:
    const TopologyCache<N>& topo;
    StrategyLevel4(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r;
        r = apply_x_wing(board, topo, result, 0); if (r != ApplyResult::NoProgress) return r;
        r = apply_y_wing(board, topo, result, 1); if (r != ApplyResult::NoProgress) return r;
        r = apply_skyscraper(board, topo, result, 2); if (r != ApplyResult::NoProgress) return r;
        r = apply_two_string_kite(board, topo, result, 3); if (r != ApplyResult::NoProgress) return r;
        r = apply_empty_rectangle(board, topo, result, 4); if (r != ApplyResult::NoProgress) return r;
        r = apply_remote_pairs(board, topo, result, 5); if (r != ApplyResult::NoProgress) return r;
        r = apply_naked_quad(board, topo, result, 6); if (r != ApplyResult::NoProgress) return r;
        r = apply_hidden_quad(board, topo, result, 7); return r;
    }
};

} // namespace level4
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_4_H