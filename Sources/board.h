#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <random>
#include <sstream>
#include <thread>
#include <optional>

#include "geometry.h"

namespace sudoku_hpc {
#if defined(__GNUC__) || defined(__clang__)
#define SUDOKU_FORCE_INLINE inline __attribute__((always_inline))
#define SUDOKU_LIKELY(x) (__builtin_expect(!!(x), 1))
#define SUDOKU_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define SUDOKU_FORCE_INLINE inline
#define SUDOKU_LIKELY(x) (x)
#define SUDOKU_UNLIKELY(x) (x)
#endif

struct GenericTopology {
    int box_rows = 0;
    int box_cols = 0;
    int n = 0;
    int nn = 0;
    int box_rows_count = 0;
    int box_cols_count = 0;
    std::vector<int> cell_row;
    std::vector<int> cell_col;
    std::vector<int> cell_box;
    std::vector<uint32_t> cell_rcb_packed;
    std::vector<int> cell_center_sym;
    std::vector<int> houses_flat;
    std::vector<int> house_offsets;
    std::vector<std::vector<int>> houses;
    std::vector<std::vector<int>> peers;
    std::vector<int> peer_offsets;
    std::vector<int> peers_flat;

    static std::optional<GenericTopology> build(int box_rows, int box_cols) {
        if (box_rows <= 0 || box_cols <= 0) {
            return std::nullopt;
        }
        geometria::zainicjalizuj_geometrie();
        GenericTopology topo;
        topo.box_rows = box_rows;
        topo.box_cols = box_cols;
        topo.n = box_rows * box_cols;
        // Current runtime mask backend is uint64_t.
        if (topo.n < 4 || topo.n > 64) {
            return std::nullopt;
        }
        if (!geometria::czy_obslugiwana(topo.n, box_rows, box_cols)) {
            return std::nullopt;
        }
        topo.nn = topo.n * topo.n;
        topo.box_rows_count = topo.n / topo.box_rows;  // liczba bloków w pionie
        topo.box_cols_count = topo.n / topo.box_cols;  // liczba bloków w poziomie
        topo.cell_row.assign(static_cast<size_t>(topo.nn), 0);
        topo.cell_col.assign(static_cast<size_t>(topo.nn), 0);
        topo.cell_box.assign(static_cast<size_t>(topo.nn), 0);
        topo.cell_rcb_packed.assign(static_cast<size_t>(topo.nn), 0U);
        topo.cell_center_sym.assign(static_cast<size_t>(topo.nn), 0);
        topo.houses_flat.clear();
        topo.house_offsets.clear();
        topo.houses_flat.reserve(static_cast<size_t>(topo.n * topo.n * 3));
        topo.house_offsets.reserve(static_cast<size_t>(topo.n * 3 + 1));

        for (int idx = 0; idx < topo.nn; ++idx) {
            const int r = idx / topo.n;
            const int c = idx % topo.n;
            // Poprawny wzór na indeks bloku dla geometrii asymetrycznych
            const int b = (r / topo.box_rows) * topo.box_cols_count + (c / topo.box_cols);
            topo.cell_row[static_cast<size_t>(idx)] = r;
            topo.cell_col[static_cast<size_t>(idx)] = c;
            topo.cell_box[static_cast<size_t>(idx)] = b;
            const uint32_t packed = (static_cast<uint32_t>(r) & 63U) |
                                    ((static_cast<uint32_t>(c) & 63U) << 6U) |
                                    ((static_cast<uint32_t>(b) & 63U) << 12U);
            topo.cell_rcb_packed[static_cast<size_t>(idx)] = packed;
            const int sym_idx = (topo.n - 1 - r) * topo.n + (topo.n - 1 - c);
            topo.cell_center_sym[static_cast<size_t>(idx)] = sym_idx;
        }

        // Wiersze
        for (int r = 0; r < topo.n; ++r) {
            topo.house_offsets.push_back(static_cast<int>(topo.houses_flat.size()));
            for (int c = 0; c < topo.n; ++c) {
                topo.houses_flat.push_back(r * topo.n + c);
            }
        }
        // Kolumny
        for (int c = 0; c < topo.n; ++c) {
            topo.house_offsets.push_back(static_cast<int>(topo.houses_flat.size()));
            for (int r = 0; r < topo.n; ++r) {
                topo.houses_flat.push_back(r * topo.n + c);
            }
        }
        // Bloki
        for (int brg = 0; brg < topo.box_rows_count; ++brg) {
            for (int bcg = 0; bcg < topo.box_cols_count; ++bcg) {
                topo.house_offsets.push_back(static_cast<int>(topo.houses_flat.size()));
                for (int dr = 0; dr < topo.box_rows; ++dr) {
                    for (int dc = 0; dc < topo.box_cols; ++dc) {
                        const int r = brg * topo.box_rows + dr;
                        const int c = bcg * topo.box_cols + dc;
                        topo.houses_flat.push_back(r * topo.n + c);
                    }
                }
            }
        }
        topo.house_offsets.push_back(static_cast<int>(topo.houses_flat.size()));

        topo.peer_offsets.clear();
        topo.peers_flat.clear();
        topo.peer_offsets.reserve(static_cast<size_t>(topo.nn + 1));
        topo.peers_flat.reserve(static_cast<size_t>(topo.nn) * static_cast<size_t>(std::max(1, 3 * topo.n - 2)));
        std::vector<int> peer_mark(static_cast<size_t>(topo.nn), -1);
        int peer_stamp = 0;
        for (int idx = 0; idx < topo.nn; ++idx) {
            topo.peer_offsets.push_back(static_cast<int>(topo.peers_flat.size()));
            const int r = topo.cell_row[static_cast<size_t>(idx)];
            const int c = topo.cell_col[static_cast<size_t>(idx)];
            const int b = topo.cell_box[static_cast<size_t>(idx)];
            ++peer_stamp;
            auto push_peer = [&](int peer_idx) {
                if (peer_idx == idx) {
                    return;
                }
                int& mark = peer_mark[static_cast<size_t>(peer_idx)];
                if (mark == peer_stamp) {
                    return;
                }
                mark = peer_stamp;
                topo.peers_flat.push_back(peer_idx);
            };

            for (int c2 = 0; c2 < topo.n; ++c2) {
                push_peer(r * topo.n + c2);
            }
            for (int r2 = 0; r2 < topo.n; ++r2) {
                push_peer(r2 * topo.n + c);
            }
            const int brg = b / topo.box_cols_count;
            const int bcg = b % topo.box_cols_count;
            const int r0 = brg * topo.box_rows;
            const int c0 = bcg * topo.box_cols;
            for (int dr = 0; dr < topo.box_rows; ++dr) {
                for (int dc = 0; dc < topo.box_cols; ++dc) {
                    push_peer((r0 + dr) * topo.n + (c0 + dc));
                }
            }
        }
        topo.peer_offsets.push_back(static_cast<int>(topo.peers_flat.size()));
        return topo;
    }
};

struct alignas(64) GenericThreadScratch {
    int prepared_n = 0;
    int prepared_nn = 0;
    std::vector<int> order;
    std::vector<uint64_t> row_tmp;
    std::vector<uint64_t> col_tmp;
    std::vector<uint64_t> box_tmp;
    std::vector<int> row_count_tmp;
    std::vector<int> col_count_tmp;
    std::vector<int> box_count_tmp;
    std::vector<int> digit_count_tmp;

    void ensure(const GenericTopology& topo) {
        if (prepared_nn != topo.nn) {
            order.resize(static_cast<size_t>(topo.nn));
            prepared_nn = topo.nn;
        }
        if (prepared_n != topo.n) {
            row_tmp.resize(static_cast<size_t>(topo.n));
            col_tmp.resize(static_cast<size_t>(topo.n));
            box_tmp.resize(static_cast<size_t>(topo.n));
            row_count_tmp.resize(static_cast<size_t>(topo.n));
            col_count_tmp.resize(static_cast<size_t>(topo.n));
            box_count_tmp.resize(static_cast<size_t>(topo.n));
            digit_count_tmp.resize(static_cast<size_t>(topo.n));
            prepared_n = topo.n;
        }
    }
};

static thread_local GenericThreadScratch g_generic_tls{};

static GenericThreadScratch& generic_tls_for(const GenericTopology& topo) {
    g_generic_tls.ensure(topo);
    return g_generic_tls;
}

struct GenericBoard {
    const GenericTopology* topo = nullptr;
    uint64_t full_mask = 0;
    int empty_cells = 0;
    std::vector<uint16_t> values;
    std::vector<uint8_t> fixed;
    std::vector<uint64_t> row_used;
    std::vector<uint64_t> col_used;
    std::vector<uint64_t> box_used;

    static uint64_t full_mask_for_n(int n) {
        return n >= 64 ? ~0ULL : ((1ULL << n) - 1ULL);
    }

    static SUDOKU_FORCE_INLINE int packed_row(uint32_t rcb) {
        return static_cast<int>(rcb & 63U);
    }
    static SUDOKU_FORCE_INLINE int packed_col(uint32_t rcb) {
        return static_cast<int>((rcb >> 6U) & 63U);
    }
    static SUDOKU_FORCE_INLINE int packed_box(uint32_t rcb) {
        return static_cast<int>((rcb >> 12U) & 63U);
    }

    SUDOKU_FORCE_INLINE uint64_t used_mask_for_packed(uint32_t rcb) const {
        const int r = packed_row(rcb);
        const int c = packed_col(rcb);
        const int b = packed_box(rcb);
        return row_used[static_cast<size_t>(r)] |
               col_used[static_cast<size_t>(c)] |
               box_used[static_cast<size_t>(b)];
    }

    SUDOKU_FORCE_INLINE uint64_t candidate_mask_for_packed(uint32_t rcb) const {
        return (~used_mask_for_packed(rcb)) & full_mask;
    }

    void reset(const GenericTopology& t) {
        topo = &t;
        full_mask = full_mask_for_n(t.n);
        empty_cells = t.nn;
        values.resize(static_cast<size_t>(t.nn));
        fixed.resize(static_cast<size_t>(t.nn));
        row_used.resize(static_cast<size_t>(t.n));
        col_used.resize(static_cast<size_t>(t.n));
        box_used.resize(static_cast<size_t>(t.n));
        std::fill(values.begin(), values.end(), 0);
        std::fill(fixed.begin(), fixed.end(), 0);
        std::fill(row_used.begin(), row_used.end(), 0ULL);
        std::fill(col_used.begin(), col_used.end(), 0ULL);
        std::fill(box_used.begin(), box_used.end(), 0ULL);
    }

    SUDOKU_FORCE_INLINE uint64_t candidate_mask_for_idx(int idx) const {
        if (SUDOKU_UNLIKELY(values[static_cast<size_t>(idx)] != 0)) {
            return 0ULL;
        }
        const uint32_t rcb = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        return candidate_mask_for_packed(rcb);
    }

    SUDOKU_FORCE_INLINE bool can_place(int idx, int digit) const {
        if (SUDOKU_UNLIKELY(values[static_cast<size_t>(idx)] != 0)) return false;
        const uint64_t bit = (1ULL << (digit - 1));
        const uint32_t rcb = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        return (used_mask_for_packed(rcb) & bit) == 0ULL;
    }

    SUDOKU_FORCE_INLINE void place(int idx, int digit, bool mark_fixed = false) {
        const uint64_t bit = (1ULL << (digit - 1));
        const uint32_t rcb = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = packed_row(rcb);
        const int c = packed_col(rcb);
        const int b = packed_box(rcb);
        const uint16_t prev = values[static_cast<size_t>(idx)];
        if (SUDOKU_LIKELY(prev == 0)) {
            --empty_cells;
        }
        values[static_cast<size_t>(idx)] = static_cast<uint16_t>(digit);
        row_used[static_cast<size_t>(r)] |= bit;
        col_used[static_cast<size_t>(c)] |= bit;
        box_used[static_cast<size_t>(b)] |= bit;
        if (SUDOKU_UNLIKELY(mark_fixed)) {
            fixed[static_cast<size_t>(idx)] = 1;
        }
    }

    SUDOKU_FORCE_INLINE void unplace(int idx, int digit) {
        const uint64_t bit = (1ULL << (digit - 1));
        const uint32_t rcb = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = packed_row(rcb);
        const int c = packed_col(rcb);
        const int b = packed_box(rcb);
        if (SUDOKU_UNLIKELY(values[static_cast<size_t>(idx)] != 0)) {
            ++empty_cells;
        }
        values[static_cast<size_t>(idx)] = 0;
        row_used[static_cast<size_t>(r)] &= ~bit;
        col_used[static_cast<size_t>(c)] &= ~bit;
        box_used[static_cast<size_t>(b)] &= ~bit;
    }

    bool init_from_puzzle(const std::vector<uint16_t>& puzzle, bool mark_fixed = true) {
        if (topo == nullptr || static_cast<int>(puzzle.size()) != topo->nn) {
            return false;
        }
        const GenericTopology& t = *topo;
        full_mask = full_mask_for_n(t.n);

        values.resize(static_cast<size_t>(t.nn));
        fixed.resize(static_cast<size_t>(t.nn));
        row_used.resize(static_cast<size_t>(t.n));
        col_used.resize(static_cast<size_t>(t.n));
        box_used.resize(static_cast<size_t>(t.n));
        std::fill(row_used.begin(), row_used.end(), 0ULL);
        std::fill(col_used.begin(), col_used.end(), 0ULL);
        std::fill(box_used.begin(), box_used.end(), 0ULL);
        if (mark_fixed) {
            std::fill(fixed.begin(), fixed.end(), 0);
        }
        const uint16_t* const puzzle_ptr = puzzle.data();
        uint16_t* const values_ptr = values.data();
        uint8_t* const fixed_ptr = fixed.data();
        uint64_t* const row_ptr = row_used.data();
        uint64_t* const col_ptr = col_used.data();
        uint64_t* const box_ptr = box_used.data();
        const uint32_t* const packed_ptr = t.cell_rcb_packed.data();

        empty_cells = 0;
        if (mark_fixed) {
            for (int idx = 0; idx < t.nn; ++idx) {
                const int d = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
                values_ptr[static_cast<size_t>(idx)] = static_cast<uint16_t>(d);
                if (d == 0) {
                    ++empty_cells;
                    continue;
                }
                if (d < 1 || d > t.n) {
                    return false;
                }
                const uint64_t bit = (1ULL << (d - 1));
                const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
                const int r = packed_row(rcb);
                const int c = packed_col(rcb);
                const int b = packed_box(rcb);
                const uint64_t used = row_ptr[static_cast<size_t>(r)] |
                                      col_ptr[static_cast<size_t>(c)] |
                                      box_ptr[static_cast<size_t>(b)];
                if ((used & bit) != 0ULL) {
                    return false;
                }
                row_ptr[static_cast<size_t>(r)] |= bit;
                col_ptr[static_cast<size_t>(c)] |= bit;
                box_ptr[static_cast<size_t>(b)] |= bit;
                fixed_ptr[static_cast<size_t>(idx)] = 1;
            }
        } else {
            for (int idx = 0; idx < t.nn; ++idx) {
                const int d = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
                values_ptr[static_cast<size_t>(idx)] = static_cast<uint16_t>(d);
                if (d == 0) {
                    ++empty_cells;
                    continue;
                }
                if (d < 1 || d > t.n) {
                    return false;
                }
                const uint64_t bit = (1ULL << (d - 1));
                const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
                const int r = packed_row(rcb);
                const int c = packed_col(rcb);
                const int b = packed_box(rcb);
                const uint64_t used = row_ptr[static_cast<size_t>(r)] |
                                      col_ptr[static_cast<size_t>(c)] |
                                      box_ptr[static_cast<size_t>(b)];
                if ((used & bit) != 0ULL) {
                    return false;
                }
                row_ptr[static_cast<size_t>(r)] |= bit;
                col_ptr[static_cast<size_t>(c)] |= bit;
                box_ptr[static_cast<size_t>(b)] |= bit;
            }
        }
        return true;
    }

    bool all_filled() const {
        return empty_cells == 0;
    }
};

} // namespace sudoku_hpc
