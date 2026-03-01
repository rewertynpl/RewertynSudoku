#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "geometry.h"

namespace sudoku_hpc {

struct GenericThreadScratch {
    int prepared_n = 0;
    std::vector<uint64_t> row_tmp;
    std::vector<uint64_t> col_tmp;
    std::vector<uint64_t> box_tmp;

    void ensure(const GenericTopology& topo) {
        if (prepared_n == topo.n) {
            return;
        }
        row_tmp.assign(static_cast<size_t>(topo.n), 0ULL);
        col_tmp.assign(static_cast<size_t>(topo.n), 0ULL);
        box_tmp.assign(static_cast<size_t>(topo.n), 0ULL);
        prepared_n = topo.n;
    }
};

inline GenericThreadScratch& generic_tls_for(const GenericTopology& topo) {
    thread_local GenericThreadScratch scratch;
    scratch.ensure(topo);
    return scratch;
}

struct GenericBoard {
    const GenericTopology* topo = nullptr;

    std::vector<uint16_t> values;
    std::vector<uint64_t> row_used;
    std::vector<uint64_t> col_used;
    std::vector<uint64_t> box_used;

    uint64_t full_mask = 0ULL;
    int empty_cells = 0;

    static int packed_row(uint32_t packed) {
        return static_cast<int>(packed & 63U);
    }

    static int packed_col(uint32_t packed) {
        return static_cast<int>((packed >> 6U) & 63U);
    }

    static int packed_box(uint32_t packed) {
        return static_cast<int>((packed >> 12U) & 63U);
    }

    void reset(const GenericTopology& t) {
        topo = &t;
        values.assign(static_cast<size_t>(t.nn), 0);
        row_used.assign(static_cast<size_t>(t.n), 0ULL);
        col_used.assign(static_cast<size_t>(t.n), 0ULL);
        box_used.assign(static_cast<size_t>(t.n), 0ULL);
        full_mask = (t.n >= 64) ? ~0ULL : ((1ULL << t.n) - 1ULL);
        empty_cells = t.nn;
    }

    bool can_place(int idx, int d) const {
        if (topo == nullptr || idx < 0 || idx >= topo->nn || d < 1 || d > topo->n) {
            return false;
        }
        const uint16_t cur = values[static_cast<size_t>(idx)];
        if (cur != 0) {
            return cur == static_cast<uint16_t>(d);
        }

        const uint32_t p = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = packed_row(p);
        const int c = packed_col(p);
        const int b = packed_box(p);

        const uint64_t bit = (1ULL << (d - 1));
        return ((row_used[static_cast<size_t>(r)] | col_used[static_cast<size_t>(c)] | box_used[static_cast<size_t>(b)]) & bit) == 0ULL;
    }

    bool place(int idx, int d) {
        if (!can_place(idx, d)) {
            return false;
        }
        const uint16_t cur = values[static_cast<size_t>(idx)];
        if (cur == static_cast<uint16_t>(d)) {
            return true;
        }

        const uint32_t p = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = packed_row(p);
        const int c = packed_col(p);
        const int b = packed_box(p);
        const uint64_t bit = (1ULL << (d - 1));

        values[static_cast<size_t>(idx)] = static_cast<uint16_t>(d);
        row_used[static_cast<size_t>(r)] |= bit;
        col_used[static_cast<size_t>(c)] |= bit;
        box_used[static_cast<size_t>(b)] |= bit;
        --empty_cells;
        return true;
    }

    void unplace(int idx, int d) {
        if (topo == nullptr || idx < 0 || idx >= topo->nn || d < 1 || d > topo->n) {
            return;
        }
        if (values[static_cast<size_t>(idx)] == 0) {
            return;
        }

        const uint32_t p = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = packed_row(p);
        const int c = packed_col(p);
        const int b = packed_box(p);
        const uint64_t bit = (1ULL << (d - 1));

        values[static_cast<size_t>(idx)] = 0;
        row_used[static_cast<size_t>(r)] &= ~bit;
        col_used[static_cast<size_t>(c)] &= ~bit;
        box_used[static_cast<size_t>(b)] &= ~bit;
        ++empty_cells;
    }

    uint64_t candidate_mask_for_idx(int idx) const {
        if (topo == nullptr || idx < 0 || idx >= topo->nn) {
            return 0ULL;
        }
        if (values[static_cast<size_t>(idx)] != 0) {
            return 0ULL;
        }
        const uint32_t p = topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = packed_row(p);
        const int c = packed_col(p);
        const int b = packed_box(p);
        const uint64_t used = row_used[static_cast<size_t>(r)] | col_used[static_cast<size_t>(c)] | box_used[static_cast<size_t>(b)];
        return (~used) & full_mask;
    }

    bool init_from_puzzle(const std::vector<uint16_t>& puzzle, bool allow_invalid) {
        if (topo == nullptr) {
            return false;
        }
        if (static_cast<int>(puzzle.size()) != topo->nn) {
            return false;
        }
        reset(*topo);

        for (int idx = 0; idx < topo->nn; ++idx) {
            const int v = static_cast<int>(puzzle[static_cast<size_t>(idx)]);
            if (v == 0) {
                continue;
            }
            if (v < 1 || v > topo->n || !place(idx, v)) {
                if (allow_invalid) {
                    return false;
                }
                return false;
            }
        }
        return true;
    }
};

} // namespace sudoku_hpc
