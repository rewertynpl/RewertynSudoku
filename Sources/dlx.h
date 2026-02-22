#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace sudoku_hpc {

struct GenericUniquenessCounter {
    static constexpr int kUnifiedMaxN = 64;

    struct UnifiedWideDlx {
        int n = 0;
        int nn = 0;
        int rows = 0;
        int cols = 0;
        int row_words = 0;
        int col_words = 0;
        int max_depth = 0;

        std::vector<std::array<uint16_t, 4>> row_cols;
        std::vector<uint64_t> col_rows_bits; // [cols * row_words]

        // Mutable state for current search run.
        std::vector<uint64_t> active_rows;    // [row_words]
        std::vector<uint64_t> uncovered_cols; // [col_words]

        // Undo logs (value snapshots).
        std::vector<uint16_t> undo_active_idx;
        std::vector<uint64_t> undo_active_old;
        std::vector<uint16_t> undo_col_idx;
        std::vector<uint64_t> undo_col_old;

        // Recursion-local best candidate rows buffer.
        // Layout: [depth0 row_words][depth1 row_words]...[depth(max_depth-1) row_words]
        std::vector<uint64_t> recursion_stack;

        bool matches(const GenericTopology& topo) const {
            return n == topo.n && nn == topo.nn;
        }
    };

    mutable UnifiedWideDlx ws_;

    static int row_id_for(int n, int r, int c, int d0) {
        return ((r * n + c) * n) + d0;
    }

    static inline int bit_ctz_u64(uint64_t v) {
#if (defined(__x86_64__) || defined(__i386__)) && defined(__BMI__)
        return static_cast<int>(_tzcnt_u64(v));
#else
        return static_cast<int>(std::countr_zero(v));
#endif
    }

    static inline uint64_t bit_clear_lsb_u64(uint64_t v) {
#if (defined(__x86_64__) || defined(__i386__)) && defined(__BMI__)
        return _blsr_u64(v);
#else
        return v & (v - 1ULL);
#endif
    }

    void build_if_needed(const GenericTopology& topo) const {
        if (topo.n <= 0 || topo.n > kUnifiedMaxN) {
            return;
        }
        if (ws_.matches(topo)) {
            return;
        }

        UnifiedWideDlx w;
        w.n = topo.n;
        w.nn = topo.nn;
        w.rows = topo.n * topo.n * topo.n;
        w.cols = 4 * topo.nn;
        w.row_words = (w.rows + 63) / 64;
        w.col_words = (w.cols + 63) / 64;
        w.max_depth = topo.nn + 1;

        w.row_cols.resize(static_cast<size_t>(w.rows));
        w.col_rows_bits.assign(static_cast<size_t>(w.cols) * static_cast<size_t>(w.row_words), 0ULL);
        w.active_rows.assign(static_cast<size_t>(w.row_words), 0ULL);
        w.uncovered_cols.assign(static_cast<size_t>(w.col_words), 0ULL);

        // Conservative reserves to reduce re-allocations during deep search.
        const size_t reserve_words = static_cast<size_t>(w.row_words) * 16ULL;
        w.undo_active_idx.reserve(reserve_words);
        w.undo_active_old.reserve(reserve_words);
        w.undo_col_idx.reserve(static_cast<size_t>(w.col_words) * 16ULL);
        w.undo_col_old.reserve(static_cast<size_t>(w.col_words) * 16ULL);

        w.recursion_stack.assign(static_cast<size_t>(w.max_depth) * static_cast<size_t>(w.row_words), 0ULL);

        for (int r = 0; r < topo.n; ++r) {
            for (int c = 0; c < topo.n; ++c) {
                const int b = topo.cell_box[static_cast<size_t>(r * topo.n + c)];
                for (int d0 = 0; d0 < topo.n; ++d0) {
                    const int row_id = row_id_for(topo.n, r, c, d0);
                    const int col_cell = r * topo.n + c;
                    const int col_row_digit = topo.nn + r * topo.n + d0;
                    const int col_col_digit = 2 * topo.nn + c * topo.n + d0;
                    const int col_box_digit = 3 * topo.nn + b * topo.n + d0;

                    w.row_cols[static_cast<size_t>(row_id)] = {
                        static_cast<uint16_t>(col_cell),
                        static_cast<uint16_t>(col_row_digit),
                        static_cast<uint16_t>(col_col_digit),
                        static_cast<uint16_t>(col_box_digit)};

                    const int rw = row_id >> 6;
                    const uint64_t bit = 1ULL << (row_id & 63);
                    w.col_rows_bits[static_cast<size_t>(col_cell) * static_cast<size_t>(w.row_words) + static_cast<size_t>(rw)] |= bit;
                    w.col_rows_bits[static_cast<size_t>(col_row_digit) * static_cast<size_t>(w.row_words) + static_cast<size_t>(rw)] |= bit;
                    w.col_rows_bits[static_cast<size_t>(col_col_digit) * static_cast<size_t>(w.row_words) + static_cast<size_t>(rw)] |= bit;
                    w.col_rows_bits[static_cast<size_t>(col_box_digit) * static_cast<size_t>(w.row_words) + static_cast<size_t>(rw)] |= bit;
                }
            }
        }

        ws_ = std::move(w);
    }

    void rollback_to(size_t active_marker, size_t col_marker) const {
        while (ws_.undo_active_idx.size() > active_marker) {
            const uint16_t idx = ws_.undo_active_idx.back();
            const uint64_t old = ws_.undo_active_old.back();
            ws_.undo_active_idx.pop_back();
            ws_.undo_active_old.pop_back();
            ws_.active_rows[static_cast<size_t>(idx)] = old;
        }
        while (ws_.undo_col_idx.size() > col_marker) {
            const uint16_t idx = ws_.undo_col_idx.back();
            const uint64_t old = ws_.undo_col_old.back();
            ws_.undo_col_idx.pop_back();
            ws_.undo_col_old.pop_back();
            ws_.uncovered_cols[static_cast<size_t>(idx)] = old;
        }
    }

    bool apply_row(int row_id) const {
        const int rw = row_id >> 6;
        const uint64_t rbit = 1ULL << (row_id & 63);
        if ((ws_.active_rows[static_cast<size_t>(rw)] & rbit) == 0ULL) {
            return false;
        }

        const auto& cols4 = ws_.row_cols[static_cast<size_t>(row_id)];
        for (int k = 0; k < 4; ++k) {
            const int col = static_cast<int>(cols4[static_cast<size_t>(k)]);
            const int cw = col >> 6;
            const uint64_t cbit = 1ULL << (col & 63);
            if ((ws_.uncovered_cols[static_cast<size_t>(cw)] & cbit) == 0ULL) {
                return false;
            }
        }

        for (int k = 0; k < 4; ++k) {
            const int col = static_cast<int>(cols4[static_cast<size_t>(k)]);
            const int cw = col >> 6;
            const uint64_t cbit = 1ULL << (col & 63);

            const uint64_t old_col_word = ws_.uncovered_cols[static_cast<size_t>(cw)];
            const uint64_t new_col_word = old_col_word & ~cbit;
            if (new_col_word != old_col_word) {
                ws_.undo_col_idx.push_back(static_cast<uint16_t>(cw));
                ws_.undo_col_old.push_back(old_col_word);
                ws_.uncovered_cols[static_cast<size_t>(cw)] = new_col_word;
            }

            const uint64_t* const col_rows =
                &ws_.col_rows_bits[static_cast<size_t>(col) * static_cast<size_t>(ws_.row_words)];
            for (int w = 0; w < ws_.row_words; ++w) {
                const uint64_t old_word = ws_.active_rows[static_cast<size_t>(w)];
                const uint64_t new_word = old_word & ~col_rows[static_cast<size_t>(w)];
                if (new_word != old_word) {
                    ws_.undo_active_idx.push_back(static_cast<uint16_t>(w));
                    ws_.undo_active_old.push_back(old_word);
                    ws_.active_rows[static_cast<size_t>(w)] = new_word;
                }
            }
        }
        return true;
    }

    bool search_with_limit(int& out_count, int limit, SearchAbortControl* budget, int depth) const {
        if (budget != nullptr && !budget->step()) {
            return false;
        }

        bool has_uncovered = false;
        for (int cw = 0; cw < ws_.col_words; ++cw) {
            if (ws_.uncovered_cols[static_cast<size_t>(cw)] != 0ULL) {
                has_uncovered = true;
                break;
            }
        }
        if (!has_uncovered) {
            ++out_count;
            return out_count >= limit;
        }

        if (depth < 0 || depth >= ws_.max_depth) {
            return false;
        }

        const size_t best_base = static_cast<size_t>(depth) * static_cast<size_t>(ws_.row_words);
        uint64_t* const local_best = &ws_.recursion_stack[best_base];

        int best_col = -1;
        int best_count = std::numeric_limits<int>::max();

        for (int cw = 0; cw < ws_.col_words; ++cw) {
            uint64_t col_word = ws_.uncovered_cols[static_cast<size_t>(cw)];
            while (col_word != 0ULL) {
                const int bit = bit_ctz_u64(col_word);
                const int col = (cw << 6) + bit;
                col_word = bit_clear_lsb_u64(col_word);
                if (col >= ws_.cols) {
                    continue;
                }

                const uint64_t* const col_rows =
                    &ws_.col_rows_bits[static_cast<size_t>(col) * static_cast<size_t>(ws_.row_words)];
                int cnt = 0;
                for (int w = 0; w < ws_.row_words; ++w) {
                    const uint64_t v = ws_.active_rows[static_cast<size_t>(w)] & col_rows[static_cast<size_t>(w)];
                    cnt += static_cast<int>(std::popcount(v));
                }

                if (cnt == 0) {
                    return false;
                }

                if (cnt < best_count) {
                    best_count = cnt;
                    best_col = col;
                    for (int w = 0; w < ws_.row_words; ++w) {
                        local_best[static_cast<size_t>(w)] =
                            ws_.active_rows[static_cast<size_t>(w)] & col_rows[static_cast<size_t>(w)];
                    }
                    if (cnt == 1) {
                        break;
                    }
                }
            }
            if (best_count == 1) {
                break;
            }
        }

        if (best_col < 0) {
            return false;
        }

        for (int w = 0; w < ws_.row_words; ++w) {
            uint64_t rows_word = local_best[static_cast<size_t>(w)];
            while (rows_word != 0ULL) {
                const int rb = bit_ctz_u64(rows_word);
                const int row_id = (w << 6) + rb;
                rows_word = bit_clear_lsb_u64(rows_word);
                if (row_id >= ws_.rows) {
                    continue;
                }

                const size_t active_marker = ws_.undo_active_idx.size();
                const size_t col_marker = ws_.undo_col_idx.size();
                if (!apply_row(row_id)) {
                    rollback_to(active_marker, col_marker);
                    continue;
                }
                if (search_with_limit(out_count, limit, budget, depth + 1)) {
                    return true;
                }
                rollback_to(active_marker, col_marker);
                if (budget != nullptr && budget->aborted()) {
                    return false;
                }
            }
        }

        return false;
    }

    int count_solutions_limit(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        int limit,
        SearchAbortControl* budget = nullptr) const {
        if (limit <= 0) {
            return 0;
        }
        if (topo.n <= 0 || topo.n > kUnifiedMaxN) {
            return 0;
        }
        if (static_cast<int>(puzzle.size()) != topo.nn) {
            return 0;
        }

        build_if_needed(topo);
        if (!ws_.matches(topo)) {
            return 0;
        }

        std::fill(ws_.active_rows.begin(), ws_.active_rows.end(), ~0ULL);
        const int valid_row_bits = ws_.rows & 63;
        if (valid_row_bits != 0) {
            ws_.active_rows[static_cast<size_t>(ws_.row_words - 1)] = (1ULL << valid_row_bits) - 1ULL;
        }

        std::fill(ws_.uncovered_cols.begin(), ws_.uncovered_cols.end(), ~0ULL);
        const int valid_col_bits = ws_.cols & 63;
        if (valid_col_bits != 0) {
            ws_.uncovered_cols[static_cast<size_t>(ws_.col_words - 1)] = (1ULL << valid_col_bits) - 1ULL;
        }

        ws_.undo_active_idx.clear();
        ws_.undo_active_old.clear();
        ws_.undo_col_idx.clear();
        ws_.undo_col_old.clear();

        const uint16_t* const puzzle_ptr = puzzle.data();
        const uint32_t* const packed_ptr = topo.cell_rcb_packed.data();
        for (int idx = 0; idx < topo.nn; ++idx) {
            const int d = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
            if (d == 0) {
                continue;
            }
            if (d < 1 || d > topo.n) {
                return 0;
            }
            const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
            const int r = GenericBoard::packed_row(rcb);
            const int c = GenericBoard::packed_col(rcb);
            const int row_id = row_id_for(topo.n, r, c, d - 1);
            const size_t active_marker = ws_.undo_active_idx.size();
            const size_t col_marker = ws_.undo_col_idx.size();
            if (!apply_row(row_id)) {
                rollback_to(active_marker, col_marker);
                return 0;
            }
        }

        int out_count = 0;
        const bool finished = search_with_limit(out_count, limit, budget, 0);
        const bool aborted = budget != nullptr && budget->aborted() && !finished;
        if (aborted) {
            return -1;
        }
        return out_count;
    }

    int count_solutions_limit2(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        SearchAbortControl* budget = nullptr) const {
        return count_solutions_limit(puzzle, topo, 2, budget);
    }
};

} // namespace sudoku_hpc
