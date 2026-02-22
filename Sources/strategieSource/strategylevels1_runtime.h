#ifndef STRATEGIE_LEVELS_1_RUNTIME_H
#define STRATEGIE_LEVELS_1_RUNTIME_H

#include <bit>
#include <chrono>
#include <cstdint>

#include "../board.h"

namespace sudoku_strategie::level1_runtime {

#ifndef SUDOKU_ENABLE_STRATEGY_TIMING
#define SUDOKU_ENABLE_STRATEGY_TIMING 0
#endif

enum class ApplyResult : uint8_t {
    NoProgress = 0,
    Progress = 1,
    Contradiction = 2
};

SUDOKU_FORCE_INLINE uint64_t candidate_mask_for_idx_fast(const sudoku_hpc::GenericBoard& board, int idx) {
    if (SUDOKU_UNLIKELY(board.values[static_cast<size_t>(idx)] != 0)) {
        return 0ULL;
    }
    const uint32_t rcb = board.topo->cell_rcb_packed[static_cast<size_t>(idx)];
    const int r = sudoku_hpc::GenericBoard::packed_row(rcb);
    const int c = sudoku_hpc::GenericBoard::packed_col(rcb);
    const int b = sudoku_hpc::GenericBoard::packed_box(rcb);
    const uint64_t used = board.row_used[static_cast<size_t>(r)] |
                          board.col_used[static_cast<size_t>(c)] |
                          board.box_used[static_cast<size_t>(b)];
    return (~used) & board.full_mask;
}

inline uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

#if SUDOKU_ENABLE_STRATEGY_TIMING
inline uint64_t strategy_timer_start() {
    return now_ns();
}

template <typename StatsT>
inline void strategy_timer_stop(StatsT& stats, uint64_t t0) {
    stats.elapsed_ns += (now_ns() - t0);
}
#else
inline constexpr uint64_t strategy_timer_start() {
    return 0ULL;
}

template <typename StatsT>
inline void strategy_timer_stop(StatsT&, uint64_t) {}
#endif

template <typename StatsT>
inline ApplyResult apply_naked_single(sudoku_hpc::GenericBoard& board, StatsT& stats) {
    const uint64_t t0 = strategy_timer_start();
    ++stats.use_count;

#if defined(__GNUC__)
#pragma GCC ivdep
#endif
    for (int idx = 0; idx < board.topo->nn; ++idx) {
        if (board.values[static_cast<size_t>(idx)] != 0) {
            continue;
        }
        const uint64_t mask = candidate_mask_for_idx_fast(board, idx);
        if (mask == 0ULL) {
            strategy_timer_stop(stats, t0);
            return ApplyResult::Contradiction;
        }
        if (!std::has_single_bit(mask)) {
            continue;
        }
        const int d = static_cast<int>(std::countr_zero(mask)) + 1;
        board.place(idx, d);
        ++stats.hit_count;
        ++stats.placements;
        strategy_timer_stop(stats, t0);
        return ApplyResult::Progress;
    }

    strategy_timer_stop(stats, t0);
    return ApplyResult::NoProgress;
}

template <int N, typename BoardT, typename StatsT>
inline ApplyResult apply_naked_single_compiled(BoardT& board, StatsT& stats) {
    const uint64_t t0 = strategy_timer_start();
    ++stats.use_count;

#if defined(__GNUC__)
#pragma GCC unroll 8
#endif
    for (int idx = 0; idx < N * N; ++idx) {
        if (board.values[static_cast<size_t>(idx)] != 0) {
            continue;
        }
        const uint64_t mask = board.candidate_mask_for_idx(idx);
        if (mask == 0ULL) {
            strategy_timer_stop(stats, t0);
            return ApplyResult::Contradiction;
        }
        if (!std::has_single_bit(mask)) {
            continue;
        }
        const int d = static_cast<int>(std::countr_zero(mask)) + 1;
        board.place(idx, d);
        ++stats.hit_count;
        ++stats.placements;
        strategy_timer_stop(stats, t0);
        return ApplyResult::Progress;
    }

    strategy_timer_stop(stats, t0);
    return ApplyResult::NoProgress;
}

template <typename StatsT>
inline ApplyResult apply_hidden_single(
    sudoku_hpc::GenericBoard& board,
    const sudoku_hpc::GenericTopology& topo,
    StatsT& stats) {
    const uint64_t t0 = strategy_timer_start();
    ++stats.use_count;

    const int house_count = static_cast<int>(topo.house_offsets.size()) - 1;
    constexpr int kMaxHouse = 64;
    for (int h = 0; h < house_count; ++h) {
        const int begin = topo.house_offsets[static_cast<size_t>(h)];
        const int end = topo.house_offsets[static_cast<size_t>(h + 1)];
        int owner[kMaxHouse];
        for (int d0 = 0; d0 < topo.n; ++d0) {
            owner[d0] = -1;
        }
        for (int p = begin; p < end; ++p) {
            const int idx = topo.houses_flat[static_cast<size_t>(p)];
            if (board.values[static_cast<size_t>(idx)] != 0) {
                continue;
            }
            const uint64_t mask = candidate_mask_for_idx_fast(board, idx);
            if (mask == 0ULL) {
                strategy_timer_stop(stats, t0);
                return ApplyResult::Contradiction;
            }
            uint64_t m = mask;
            while (m != 0ULL) {
                const int d0 = static_cast<int>(std::countr_zero(m));
                int& slot = owner[d0];
                slot = (slot == -1) ? idx : -2;
                m &= (m - 1ULL);
            }
        }
        for (int d0 = 0; d0 < topo.n; ++d0) {
            const int pos = owner[d0];
            if (pos >= 0) {
                board.place(pos, d0 + 1);
                ++stats.hit_count;
                ++stats.placements;
                strategy_timer_stop(stats, t0);
                return ApplyResult::Progress;
            }
        }
    }

    strategy_timer_stop(stats, t0);
    return ApplyResult::NoProgress;
}

template <int N, typename BoardT, typename TopologyT, typename StatsT>
inline ApplyResult apply_hidden_single_compiled(BoardT& board, const TopologyT& topo, StatsT& stats) {
    const uint64_t t0 = strategy_timer_start();
    ++stats.use_count;

#if defined(__GNUC__)
#pragma GCC unroll 3
#endif
    for (int h = 0; h < (3 * N); ++h) {
        int owner[N];
#if defined(__GNUC__)
#pragma GCC unroll 8
#endif
        for (int d0 = 0; d0 < N; ++d0) {
            owner[d0] = -1;
        }

        for (int k = 0; k < N; ++k) {
            const int idx = topo.houses[h][k];
            if (board.values[static_cast<size_t>(idx)] != 0) {
                continue;
            }
            const uint64_t mask = board.candidate_mask_for_idx(idx);
            if (mask == 0ULL) {
                strategy_timer_stop(stats, t0);
                return ApplyResult::Contradiction;
            }
            uint64_t m = mask;
            while (m != 0ULL) {
                const int d0 = static_cast<int>(std::countr_zero(m));
                int& slot = owner[d0];
                slot = (slot == -1) ? idx : -2;
                m &= (m - 1ULL);
            }
        }

#if defined(__GNUC__)
#pragma GCC unroll 8
#endif
        for (int d0 = 0; d0 < N; ++d0) {
            const int pos = owner[d0];
            if (pos >= 0) {
                board.place(pos, d0 + 1);
                ++stats.hit_count;
                ++stats.placements;
                strategy_timer_stop(stats, t0);
                return ApplyResult::Progress;
            }
        }
    }

    strategy_timer_stop(stats, t0);
    return ApplyResult::NoProgress;
}

} // namespace sudoku_strategie::level1_runtime

#endif // STRATEGIE_LEVELS_1_RUNTIME_H
