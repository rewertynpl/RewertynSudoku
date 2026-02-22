// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 5
// Plik: strategylevels5.cpp
// ============================================================================

#ifndef STRATEGIE_LEVELS_5_H
#define STRATEGIE_LEVELS_5_H

#include "strategie_types.h"

namespace sudoku_strategie {
namespace level5 {

template<int N>
ApplyResult apply_swordfish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
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
            if (popcnt64(row_masks) > 3 || row_masks == 0) continue;
            for (int r2 = r1 + 1; r2 < N; ++r2) {
                if (popcnt64(row_masks) > 3 || row_masks == 0) continue;
                for (int r3 = r2 + 1; r3 < N; ++r3) {
                    if (popcnt64(row_masks) > 3 || row_masks == 0) continue;
                    
                    if (popcnt64(row_masks | row_masks | row_masks) == 3) {
                        result.used_swordfish = true;
                        ++stats.hit_count;
                    }
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_xyz_wing(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int pivot = 0; pivot < N*N; ++pivot) {
        if (board.values != 0) continue;
        const uint64_t mask = board.candidate_mask_for_idx(pivot);
        if (popcnt64(mask) != 3) continue;
        
        for (int p1 : topo.peers) {
            if (board.values != 0) continue;
            const uint64_t mask1 = board.candidate_mask_for_idx(p1);
            if (popcnt64(mask1) != 2) continue;
            
            for (int p2 : topo.peers) {
                if (p2 == p1 || board.values != 0) continue;
                const uint64_t mask2 = board.candidate_mask_for_idx(p2);
                if (popcnt64(mask2) != 2) continue;
                
                result.used_x_wing = true; 
                ++stats.hit_count;
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N> ApplyResult apply_finned_x_wing(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_sashimi(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_unique_rectangle(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_bug_plus_1(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_w_wing(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }
template<int N> ApplyResult apply_simple_coloring(BoardSoA<N>&, const TopologyCache<N>&, LogicCertifyResult&, size_t) { return ApplyResult::NoProgress; }

template<int N>
class StrategyLevel5 {
public:
    const TopologyCache<N>& topo;
    StrategyLevel5(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r;
        r = apply_swordfish(board, topo, result, 0); if (r != ApplyResult::NoProgress) return r;
        r = apply_xyz_wing(board, topo, result, 1); if (r != ApplyResult::NoProgress) return r;
        r = apply_finned_x_wing(board, topo, result, 2); if (r != ApplyResult::NoProgress) return r;
        r = apply_sashimi(board, topo, result, 3); if (r != ApplyResult::NoProgress) return r;
        r = apply_unique_rectangle(board, topo, result, 4); if (r != ApplyResult::NoProgress) return r;
        r = apply_bug_plus_1(board, topo, result, 5); if (r != ApplyResult::NoProgress) return r;
        r = apply_w_wing(board, topo, result, 6); if (r != ApplyResult::NoProgress) return r;
        r = apply_simple_coloring(board, topo, result, 7); return r;
    }
};

} // namespace level5
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_5_H