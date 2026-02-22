// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 7
// Plik: strategylevels7.cpp
// Opis: 3D Medusa, AIC/Grouped AIC, Grouped X-Cycle, Continuous Nice Loop,
//       ALS-XY-Wing, ALS-Chain, Sue de Coq, Death Blossom,
//       Franken/Mutant Fish, Kraken Fish
// Zakres: obsługa wszystkich rozmiarów 4x4 do 36x36
// ============================================================================

#ifndef STRATEGIE_LEVELS_7_H
#define STRATEGIE_LEVELS_7_H

#include "strategie_types.h"

namespace sudoku_strategie {
namespace level7 {

template<int N>
ApplyResult apply_3d_medusa(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    // result.used_x_wing = true;  // Medusa jest zaawansowaną techniką
    // ++stats.hit_count;
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_aic(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_grouped_aic(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_grouped_x_cycle(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_continuous_nice_loop(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_als_xy_wing(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_als_chain(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_sue_de_coq(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_death_blossom(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_franken_fish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_mutant_fish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_kraken_fish(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// KLASA STRATEGII POZIOMU 7
// ============================================================================

template<int N>
class StrategyLevel7 {
public:
    const TopologyCache<N>& topo;
    StrategyLevel7(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r;
        
        r = apply_3d_medusa(board, topo, result, 0);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_aic(board, topo, result, 1);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_grouped_aic(board, topo, result, 2);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_grouped_x_cycle(board, topo, result, 3);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_continuous_nice_loop(board, topo, result, 4);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_als_xy_wing(board, topo, result, 5);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_als_chain(board, topo, result, 6);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_sue_de_coq(board, topo, result, 7);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_death_blossom(board, topo, result, 8);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_franken_fish(board, topo, result, 9);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_mutant_fish(board, topo, result, 10);
        if (r != ApplyResult::NoProgress) return r;
        
        r = apply_kraken_fish(board, topo, result, 11);
        return r;
    }
};

} // namespace level7
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_7_H