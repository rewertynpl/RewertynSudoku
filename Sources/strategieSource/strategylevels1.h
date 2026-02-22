// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 1
// Plik: strategylevels1.cpp
// ============================================================================

#ifndef STRATEGIE_LEVELS_1_H
#define STRATEGIE_LEVELS_1_H

#include "strategie_types.h"
#include "strategie_scratch.h"
#include <algorithm>

namespace sudoku_strategie {
namespace level1 {

template<int N>
ApplyResult apply_naked_single(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats[stats_idx];
    ++stats.use_count;

    auto& scratch = sudoku_strategie::scratch::get_scratch<N>();
    scratch.level1.reset();

    for (int idx = 0; idx < N*N; ++idx) {
        if (board.values[static_cast<size_t>(idx)] != 0) continue;
        const uint64_t mask = board.candidate_mask_for_idx(idx);

        if (mask == 0ULL) {
            stats.elapsed_ns += now_ns() - t0;
            return ApplyResult::Contradiction;
        }

        const int d = single_digit_from_mask(mask);
        if (d > 0) {
            board.place(idx, d);
            ++stats.hit_count;
            ++stats.placements;
            ++result.steps;
            result.used_naked_single = true;
            stats.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
    }

    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
ApplyResult apply_hidden_single(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats[stats_idx];
    ++stats.use_count;

    auto& scratch = sudoku_strategie::scratch::get_scratch<N>();
    scratch.level1.reset();

    for (int h = 0; h < topo.NUM_HOUSES; ++h) {
        for (int d = 1; d <= N; ++d) {
            const uint64_t bit = static_cast<uint64_t>(1ULL << (d - 1));
            int pos = -1;
            int count = 0;

            for (int k = 0; k < N; ++k) {
                const int idx = topo.houses[h][k];
                if (board.values[static_cast<size_t>(idx)] != 0) continue;

                const uint64_t mask = board.candidate_mask_for_idx(idx);
                if (mask & bit) {
                    pos = idx;
                    ++count;
                    if (count > 1) break;
                }
            }

            if (count == 1 && pos >= 0) {
                board.place(pos, d);
                ++stats.hit_count;
                ++stats.placements;
                ++result.steps;
                result.used_hidden_single = true;
                stats.elapsed_ns += now_ns() - t0;
                return ApplyResult::Progress;
            }
        }
    }

    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
class StrategyLevel1 {
public:
    static constexpr size_t IDX_NAKED = 0;
    static constexpr size_t IDX_HIDDEN = 1;
    const TopologyCache<N>& topo;
    
    StrategyLevel1(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r1 = apply_naked_single(board, topo, result, IDX_NAKED);
        if (r1 != ApplyResult::NoProgress) return r1;
        return apply_hidden_single(board, topo, result, IDX_HIDDEN);
    }
    
    ApplyResult apply_strategy(BoardSoA<N>& board, LogicCertifyResult& result, StrategyId strategy) {
        switch (strategy) {
            case StrategyId::NakedSingle: return apply_naked_single(board, topo, result, IDX_NAKED);
            case StrategyId::HiddenSingle: return apply_hidden_single(board, topo, result, IDX_HIDDEN);
            default: return ApplyResult::NoProgress;
        }
    }
    
    bool was_strategy_used(const LogicCertifyResult& result, StrategyId strategy) const {
        switch (strategy) {
            case StrategyId::NakedSingle: return result.used_naked_single;
            case StrategyId::HiddenSingle: return result.used_hidden_single;
            default: return false;
        }
    }
};

// Uwaga architektoniczna: Usunięto DynamicStrategyLevel1 aby uniknąć błędów VLA i False Sharing.
// Zawsze należy korzystać z instancji prekompilowanych szablonów.

} // namespace level1
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_1_H