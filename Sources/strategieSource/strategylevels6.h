// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 6
// Plik: strategylevels6.cpp
// Opis: Jellyfish, WXYZ-Wing, Finned Swordfish/Jellyfish, X-Chain,
//       XY-Chain, ALS-XZ, Unique Loop, Avoidable Rectangle, Bivalue Oddagon
// Zakres: obsługa wszystkich rozmiarów 4x4 do 36x36
// ============================================================================

#ifndef STRATEGIE_LEVELS_6_H
#define STRATEGIE_LEVELS_6_H

#include "strategie_types.h"

namespace sudoku_strategie {
namespace level6 {

template<int N>
ApplyResult apply_jellyfish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    // Gotowe do wdrożenia 4 zagnieżdżonych pętli (wzorcem ze Swordfish z Level 5)
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_wxyz_wing(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_finned_swordfish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_finned_jellyfish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_x_chain(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_xy_chain(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_als_xz(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_unique_loop(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_avoidable_rectangle(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_bivalue_oddagon(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// KLASA STRATEGII POZIOMU 6
// ============================================================================

template<int N>
class StrategyLevel6 {
public:
    const TopologyCache<N>& topo;
    StrategyLevel6(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r;
        
        r = apply_jellyfish(board, topo, result, 0);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_wxyz_wing(board, topo, result, 1);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_finned_swordfish(board, topo, result, 2);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_finned_jellyfish(board, topo, result, 3);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_x_chain(board, topo, result, 4);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_xy_chain(board, topo, result, 5);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_als_xz(board, topo, result, 6);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_unique_loop(board, topo, result, 7);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_avoidable_rectangle(board, topo, result, 8);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_bivalue_oddagon(board, topo, result, 9);
        return r;
    }
};

} // namespace level6
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_6_H