// ============================================================================
// SUDOKU HPC - CORE ENGINES
// Moduł: dlx_solver.h
// Opis: Algorytm Dancing Links (DLX) do walidacji unikalności z mechanizmem
//       narzucania wzorców binarowych (allowed_masks) - Pattern Forcing Bridge.
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <atomic>
#include <vector>

#include "../../core/geometry.h"
#include "../../core/board.h"
#include "../../config/bit_utils.h" 

namespace sudoku_hpc::core_engines {

struct SearchAbortControl {
    bool time_enabled = false;
    bool node_enabled = false;
    std::chrono::steady_clock::time_point deadline{};
    uint64_t node_limit = 0;
    uint64_t nodes = 0;

    const std::atomic<bool>* force_abort_ptr = nullptr;
    const std::atomic<bool>* cancel_ptr = nullptr;
    const std::atomic<bool>* pause_ptr = nullptr;

    bool aborted_by_time = false;
    bool aborted_by_nodes = false;
    bool aborted_by_cancel = false;
    bool aborted_by_pause = false;
    bool aborted_by_force = false;

    bool aborted() const {
        return aborted_by_time || aborted_by_nodes || aborted_by_cancel || aborted_by_pause || aborted_by_force;
    }

    bool step(uint64_t add_nodes = 1) {
        nodes += add_nodes;

        if (force_abort_ptr != nullptr && force_abort_ptr->load(std::memory_order_relaxed)) {
            aborted_by_force = true;
            return false;
        }
        if (cancel_ptr != nullptr && cancel_ptr->load(std::memory_order_relaxed)) {
            aborted_by_cancel = true;
            return false;
        }
        if (pause_ptr != nullptr && pause_ptr->load(std::memory_order_relaxed)) {
            aborted_by_pause = true;
            return false;
        }
        if (node_enabled && node_limit > 0 && nodes >= node_limit) {
            aborted_by_nodes = true;
            return false;
        }
        if (time_enabled && std::chrono::steady_clock::now() >= deadline) {
            aborted_by_time = true;
            return false;
        }
        return true;
    }
};

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

        // Pamięć operacyjna dla aktualnego przebiegu szukania.
        std::vector<uint64_t> active_rows;    // [row_words]
        std::vector<uint64_t> uncovered_cols; // [col_words]

        // Undo logs - logi cofania. Zapewniają unikanie realokacji Node'ów w pamięci
        std::vector<uint16_t> undo_active_idx;
        std::vector<uint64_t> undo_active_old;
        std::vector<uint16_t> undo_col_idx;
        std::vector<uint64_t> undo_col_old;

        // Płaski stos dla zagnieżdżeń DFS z rekurencją zamienioną na "fake-recurrency" w DLX
        std::vector<uint64_t> recursion_stack;
        std::vector<int> solution_rows;
        int solution_depth = 0;

        bool matches(const GenericTopology& topo) const {
            return n == topo.n && nn == topo.nn;
        }
    };

    mutable UnifiedWideDlx ws_;

    static int row_id_for(int n, int r, int c, int d0) {
        return ((r * n + c) * n) + d0;
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

        // Zapobieganie realokacji - pojemność na potężne głębokości (P8/SKLoop testing)
        const size_t reserve_words = static_cast<size_t>(w.row_words) * 16ULL;
        w.undo_active_idx.reserve(reserve_words);
        w.undo_active_old.reserve(reserve_words);
        w.undo_col_idx.reserve(static_cast<size_t>(w.col_words) * 16ULL);
        w.undo_col_old.reserve(static_cast<size_t>(w.col_words) * 16ULL);

        w.recursion_stack.assign(static_cast<size_t>(w.max_depth) * static_cast<size_t>(w.row_words), 0ULL);
        w.solution_rows.assign(static_cast<size_t>(w.max_depth), -1);
        w.solution_depth = 0;

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

    void initialize_state() const {
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
        ws_.solution_depth = 0;
        std::fill(ws_.solution_rows.begin(), ws_.solution_rows.end(), -1);
    }

    // Aplikuje z góry założony wzorzec z pattern_forcing do DLXa
    bool restrict_rows_by_allowed_masks(const GenericTopology& topo, const std::vector<uint64_t>& allowed_masks) const {
        if (static_cast<int>(allowed_masks.size()) != topo.nn) {
            return false;
        }
        const uint64_t full_mask = (topo.n >= 64) ? ~0ULL : ((1ULL << topo.n) - 1ULL);
        for (int idx = 0; idx < topo.nn; ++idx) {
            uint64_t allowed = allowed_masks[static_cast<size_t>(idx)] & full_mask;
            if (allowed == 0ULL) {
                return false;
            }
            if (allowed == full_mask) {
                continue;
            }
            const int row_base = idx * topo.n;
            for (int d0 = 0; d0 < topo.n; ++d0) {
                const uint64_t bit = (1ULL << d0);
                if ((allowed & bit) != 0ULL) {
                    continue;
                }
                const int row_id = row_base + d0;
                const int rw = row_id >> 6;
                ws_.active_rows[static_cast<size_t>(rw)] &= ~(1ULL << (row_id & 63));
            }
        }
        return true;
    }

    bool search_find_one(SearchAbortControl* budget, int depth) const {
        if (budget != nullptr && !budget->step()) return false;

        bool has_uncovered = false;
        for (int cw = 0; cw < ws_.col_words; ++cw) {
            if (ws_.uncovered_cols[static_cast<size_t>(cw)] != 0ULL) {
                has_uncovered = true;
                break;
            }
        }
        if (!has_uncovered) {
            ws_.solution_depth = depth;
            return true;
        }
        if (depth < 0 || depth >= ws_.max_depth) return false;

        const size_t best_base = static_cast<size_t>(depth) * static_cast<size_t>(ws_.row_words);
        uint64_t* const local_best = &ws_.recursion_stack[best_base];
        int best_col = -1;
        int best_count = std::numeric_limits<int>::max();

        // Heurystyka Minimalnego Wyboru (MRV) wyciągająca popcnt z przecięć słów
        for (int cw = 0; cw < ws_.col_words; ++cw) {
            uint64_t col_word = ws_.uncovered_cols[static_cast<size_t>(cw)];
            while (col_word != 0ULL) {
                const int bit = config::bit_ctz_u64(col_word);
                const int col = (cw << 6) + bit;
                col_word = config::bit_clear_lsb_u64(col_word);
                if (col >= ws_.cols) continue;
                
                const uint64_t* const col_rows =
                    &ws_.col_rows_bits[static_cast<size_t>(col) * static_cast<size_t>(ws_.row_words)];
                int cnt = 0;
                for (int w = 0; w < ws_.row_words; ++w) {
                    const uint64_t v = ws_.active_rows[static_cast<size_t>(w)] & col_rows[static_cast<size_t>(w)];
                    cnt += static_cast<int>(std::popcount(v));
                }
                
                if (cnt == 0) return false;
                
                if (cnt < best_count) {
                    best_count = cnt;
                    best_col = col;
                    for (int w = 0; w < ws_.row_words; ++w) {
                        local_best[static_cast<size_t>(w)] =
                            ws_.active_rows[static_cast<size_t>(w)] & col_rows[static_cast<size_t>(w)];
                    }
                    if (cnt == 1) break;
                }
            }
            if (best_count == 1) break;
        }
        
        if (best_col < 0) return false;

        for (int w = 0; w < ws_.row_words; ++w) {
            uint64_t rows_word = local_best[static_cast<size_t>(w)];
            while (rows_word != 0ULL) {
                const int rb = config::bit_ctz_u64(rows_word);
                const int row_id = (w << 6) + rb;
                rows_word = config::bit_clear_lsb_u64(rows_word);
                
                if (row_id >= ws_.rows) continue;
                
                const size_t active_marker = ws_.undo_active_idx.size();
                const size_t col_marker = ws_.undo_col_idx.size();
                ws_.solution_rows[static_cast<size_t>(depth)] = row_id;
                
                if (!apply_row(row_id)) {
                    rollback_to(active_marker, col_marker);
                    ws_.solution_rows[static_cast<size_t>(depth)] = -1;
                    continue;
                }
                
                if (search_find_one(budget, depth + 1)) return true;
                
                rollback_to(active_marker, col_marker);
                ws_.solution_rows[static_cast<size_t>(depth)] = -1;
                
                if (budget != nullptr && budget->aborted()) return false;
            }
        }
        return false;
    }

    bool search_with_limit(int& out_count, int limit, SearchAbortControl* budget, int depth) const {
        if (budget != nullptr && !budget->step()) return false;

        bool has_uncovered = false;
        for (int cw = 0; cw < ws_.col_words; ++cw) {
            if (ws_.uncovered_cols[static_cast<size_t>(cw)] != 0ULL) {
                has_uncovered = true;
                break;
            }
        }
        if (!has_uncovered) {
            ++out_count;
            return out_count >= limit; // True przerywa jako "Osiągnięto limit znalezień"
        }

        if (depth < 0 || depth >= ws_.max_depth) return false;

        const size_t best_base = static_cast<size_t>(depth) * static_cast<size_t>(ws_.row_words);
        uint64_t* const local_best = &ws_.recursion_stack[best_base];

        int best_col = -1;
        int best_count = std::numeric_limits<int>::max();

        for (int cw = 0; cw < ws_.col_words; ++cw) {
            uint64_t col_word = ws_.uncovered_cols[static_cast<size_t>(cw)];
            while (col_word != 0ULL) {
                const int bit = config::bit_ctz_u64(col_word);
                const int col = (cw << 6) + bit;
                col_word = config::bit_clear_lsb_u64(col_word);
                if (col >= ws_.cols) continue;

                const uint64_t* const col_rows =
                    &ws_.col_rows_bits[static_cast<size_t>(col) * static_cast<size_t>(ws_.row_words)];
                int cnt = 0;
                for (int w = 0; w < ws_.row_words; ++w) {
                    const uint64_t v = ws_.active_rows[static_cast<size_t>(w)] & col_rows[static_cast<size_t>(w)];
                    cnt += static_cast<int>(std::popcount(v));
                }

                if (cnt == 0) return false;

                if (cnt < best_count) {
                    best_count = cnt;
                    best_col = col;
                    for (int w = 0; w < ws_.row_words; ++w) {
                        local_best[static_cast<size_t>(w)] =
                            ws_.active_rows[static_cast<size_t>(w)] & col_rows[static_cast<size_t>(w)];
                    }
                    if (cnt == 1) break;
                }
            }
            if (best_count == 1) break;
        }

        if (best_col < 0) return false;

        for (int w = 0; w < ws_.row_words; ++w) {
            uint64_t rows_word = local_best[static_cast<size_t>(w)];
            while (rows_word != 0ULL) {
                const int rb = config::bit_ctz_u64(rows_word);
                const int row_id = (w << 6) + rb;
                rows_word = config::bit_clear_lsb_u64(rows_word);
                
                if (row_id >= ws_.rows) continue;

                const size_t active_marker = ws_.undo_active_idx.size();
                const size_t col_marker = ws_.undo_col_idx.size();
                
                if (!apply_row(row_id)) {
                    rollback_to(active_marker, col_marker);
                    continue;
                }
                
                if (search_with_limit(out_count, limit, budget, depth + 1)) return true;
                
                rollback_to(active_marker, col_marker);
                if (budget != nullptr && budget->aborted()) return false;
            }
        }
        return false;
    }

    int count_solutions_limit(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        int limit,
        SearchAbortControl* budget = nullptr) const {
            
        if (limit <= 0) return 0;
        if (topo.n <= 0 || topo.n > kUnifiedMaxN) return 0;
        if (static_cast<int>(puzzle.size()) != topo.nn) return 0;

        build_if_needed(topo);
        if (!ws_.matches(topo)) return 0;

        initialize_state();

        const uint16_t* const puzzle_ptr = puzzle.data();
        const uint32_t* const packed_ptr = topo.cell_rcb_packed.data();
        
        for (int idx = 0; idx < topo.nn; ++idx) {
            const int d = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
            if (d == 0) continue;
            if (d < 1 || d > topo.n) return 0;
            
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
        
        if (aborted) return -1;
        return out_count;
    }

    int count_solutions_limit2(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        SearchAbortControl* budget = nullptr) const {
        return count_solutions_limit(puzzle, topo, 2, budget);
    }

    // Zwraca rozwiązanie zbudowane z Pattern Forcing Bridge
    bool solve_and_capture(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        std::vector<uint16_t>& out_solution,
        SearchAbortControl* budget = nullptr,
        const std::vector<uint64_t>* allowed_masks = nullptr) const {
            
        if (topo.n <= 0 || topo.n > kUnifiedMaxN) return false;
        if (static_cast<int>(puzzle.size()) != topo.nn) return false;

        build_if_needed(topo);
        if (!ws_.matches(topo)) return false;

        initialize_state();
        
        // Integracja Pattern Forcing z generatorem - to narzuca konkretne
        // ułożenie masek pod Exoceta / SK-Loop
        if (allowed_masks != nullptr && !restrict_rows_by_allowed_masks(topo, *allowed_masks)) {
            return false;
        }

        const uint16_t* const puzzle_ptr = puzzle.data();
        for (int idx = 0; idx < topo.nn; ++idx) {
            const int d = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
            if (d == 0) continue;
            if (d < 1 || d > topo.n) return false;
            
            if (allowed_masks != nullptr) {
                const uint64_t allowed = (*allowed_masks)[static_cast<size_t>(idx)] & ((topo.n >= 64) ? ~0ULL : ((1ULL << topo.n) - 1ULL));
                if ((allowed & (1ULL << (d - 1))) == 0ULL) return false;
            }
            
            const int row_id = idx * topo.n + (d - 1);
            const size_t active_marker = ws_.undo_active_idx.size();
            const size_t col_marker = ws_.undo_col_idx.size();
            if (!apply_row(row_id)) {
                rollback_to(active_marker, col_marker);
                return false;
            }
        }

        const bool found = search_find_one(budget, 0);
        if (!found) return false;
        if (budget != nullptr && budget->aborted()) return false;

        if (out_solution.size() != static_cast<size_t>(topo.nn)) {
            out_solution.resize(static_cast<size_t>(topo.nn));
        }
        std::copy(puzzle.begin(), puzzle.end(), out_solution.begin());
        
        for (int i = 0; i < ws_.solution_depth; ++i) {
            const int row_id = ws_.solution_rows[static_cast<size_t>(i)];
            if (row_id < 0) continue;
            
            const int cell = row_id / topo.n;
            const int d0 = row_id % topo.n;
            out_solution[static_cast<size_t>(cell)] = static_cast<uint16_t>(d0 + 1);
        }
        
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (out_solution[static_cast<size_t>(idx)] == 0) return false;
        }
        return true;
    }
};

} // namespace sudoku_hpc::core_engines
