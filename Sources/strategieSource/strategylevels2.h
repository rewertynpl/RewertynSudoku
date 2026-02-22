// ============================================================================
// SUDOKU HPC - STRATEGIE POZIOMU 2
// Plik: strategylevels2.cpp
// ============================================================================

#ifndef STRATEGIE_LEVELS_2_H
#define STRATEGIE_LEVELS_2_H

#include "strategie_types.h"
#include <algorithm>
#include <cstring>

namespace sudoku_strategie {
namespace level2 {

template<int N>
ApplyResult apply_pointing_pairs_triples(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    const int box_count = (N / board.box_rows) * (N / board.box_cols);
    
    for (int bi = 0; bi < box_count; ++bi) {
        const int br = bi / (N / board.box_cols);
        const int bc = bi % (N / board.box_cols);
        
        for (int d = 1; d <= N; ++d) {
            const uint64_t bit = static_cast<uint64_t>(1ULL << (d - 1));
            std::array<int, N> positions{};
            int pos_count = 0;
            
            for (int dr = 0; dr < board.box_rows && pos_count < N; ++dr) {
                for (int dc = 0; dc < board.box_cols && pos_count < N; ++dc) {
                    const int r = br * board.box_rows + dr;
                    const int c = bc * board.box_cols + dc;
                    const int idx = r * N + c;
                    
                    if (board.values != 0) continue;
                    
                    const uint64_t mask = board.candidate_mask_for_idx(idx);
                    if (mask & bit) {
                        positions = idx;
                    }
                }
            }
            
            if (pos_count < 2) continue;
            
            bool same_row = true;
            int row_idx = positions / N;
            for (int i = 1; i < pos_count; ++i) {
                if (positions / N != row_idx) { same_row = false; break; }
            }
            if (same_row && pos_count > board.box_cols) same_row = false;
            
            if (same_row) {
                bool eliminated = false;
                for (int c = 0; c < N; ++c) {
                    const int idx = row_idx * N + c;
                    const int cell_box = (row_idx / board.box_rows) * (N / board.box_cols) + (c / board.box_cols);
                    if (cell_box != bi && board.values == 0) {
                        (void)eliminated; // Solvery wdrożą unplace
                        result.used_pointing = true;
                    }
                }
            }
            
            bool same_col = true;
            int col_idx = positions % N;
            for (int i = 1; i < pos_count; ++i) {
                if (positions % N != col_idx) { same_col = false; break; }
            }
            if (same_col && pos_count > board.box_rows) same_col = false;
            
            if (same_col) {
                bool eliminated = false;
                for (int r = 0; r < N; ++r) {
                    const int idx = r * N + col_idx;
                    const int cell_box = (r / board.box_rows) * (N / board.box_cols) + (col_idx / board.box_cols);
                    if (cell_box != bi && board.values == 0) {
                        (void)eliminated; // Solvery wdrożą unplace
                        result.used_pointing = true;
                    }
                }
            }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress; 
}

template<int N>
ApplyResult apply_box_line_reduction(BoardSoA<N>& board, const TopologyCache<N>& topo, LogicCertifyResult& result, size_t stats_idx) {
    const uint64_t t0 = now_ns();
    auto& stats = result.strategy_stats;
    ++stats.use_count;
    
    for (int r = 0; r < N; ++r) {
        for (int d = 1; d <= N; ++d) {
            const uint64_t bit = static_cast<uint64_t>(1ULL << (d - 1));
            std::array<int, N> positions{};
            int pos_count = 0;
            
            for (int c = 0; c < N && pos_count < N; ++c) {
                const int idx = r * N + c;
                if (board.values != 0) continue;
                
                const uint64_t mask = board.candidate_mask_for_idx(idx);
                if (mask & bit) { positions = idx; }
            }
            if (pos_count < 2) continue;
            
            bool same_box = true;
            int box_idx = (r / board.box_rows) * (N / board.box_cols) + (positions % N) / board.box_cols;
            for (int i = 1; i < pos_count; ++i) {
                int c = positions % N;
                int b = (r / board.box_rows) * (N / board.box_cols) + c / board.box_cols;
                if (b != box_idx) { same_box = false; break; }
            }
            if (same_box) { result.used_box_line = true; }
        }
    }
    
    for (int c = 0; c < N; ++c) {
        for (int d = 1; d <= N; ++d) {
            const uint64_t bit = static_cast<uint64_t>(1ULL << (d - 1));
            std::array<int, N> positions{};
            int pos_count = 0;
            
            for (int r = 0; r < N && pos_count < N; ++r) {
                const int idx = r * N + c;
                if (board.values != 0) continue;
                
                const uint64_t mask = board.candidate_mask_for_idx(idx);
                if (mask & bit) { positions = idx; }
            }
            if (pos_count < 2) continue;
            
            bool same_box = true;
            int box_idx = (positions / N) / board.box_rows * (N / board.box_cols) + c / board.box_cols;
            for (int i = 1; i < pos_count; ++i) {
                int r = positions / N;
                int b = (r / board.box_rows) * (N / board.box_cols) + c / board.box_cols;
                if (b != box_idx) { same_box = false; break; }
            }
            if (same_box) { result.used_box_line = true; }
        }
    }
    
    stats.elapsed_ns += now_ns() - t0;
    return ApplyResult::NoProgress;
}

template<int N>
class StrategyLevel2 {
public:
    static constexpr size_t IDX_POINTING = 0;
    static constexpr size_t IDX_BOX_LINE = 1;
    const TopologyCache<N>& topo;
    
    StrategyLevel2(const TopologyCache<N>& t) : topo(t) {}
    
    ApplyResult apply_all(BoardSoA<N>& board, LogicCertifyResult& result) {
        ApplyResult r1 = apply_pointing_pairs_triples(board, topo, result, IDX_POINTING);
        if (r1 != ApplyResult::NoProgress) return r1;
        return apply_box_line_reduction(board, topo, result, IDX_BOX_LINE);
    }
    
    ApplyResult apply_strategy(BoardSoA<N>& board, LogicCertifyResult& result, StrategyId strategy) {
        switch (strategy) {
            case StrategyId::PointingPairs:
            case StrategyId::PointingTriples: return apply_pointing_pairs_triples(board, topo, result, IDX_POINTING);
            case StrategyId::BoxLineReduction: return apply_box_line_reduction(board, topo, result, IDX_BOX_LINE);
            default: return ApplyResult::NoProgress;
        }
    }
    
    bool was_strategy_used(const LogicCertifyResult& result, StrategyId strategy) const {
        switch (strategy) {
            case StrategyId::PointingPairs:
            case StrategyId::PointingTriples:
            case StrategyId::BoxLineReduction: return result.used_box_line;
            default: return false;
        }
    }
};

} // namespace level2
} // namespace sudoku_strategie

#endif // STRATEGIE_LEVELS_2_H