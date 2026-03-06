#pragma once

#include <algorithm>

#include "exact_pattern_scratchpad.h"
#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"

namespace sudoku_hpc::logic::shared {

inline void snapshot_state(CandidateState& st, ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    std::copy_n(st.cands, nn, sp.dyn_cands_backup);
    std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
    std::copy_n(st.board->row_used.data(), n, sp.dyn_row_used_backup);
    std::copy_n(st.board->col_used.data(), n, sp.dyn_col_used_backup);
    std::copy_n(st.board->box_used.data(), n, sp.dyn_box_used_backup);
    sp.dyn_empty_backup = st.board->empty_cells;
}

inline void restore_state(CandidateState& st, ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    std::copy_n(sp.dyn_cands_backup, nn, st.cands);
    std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
    std::copy_n(sp.dyn_row_used_backup, n, st.board->row_used.data());
    std::copy_n(sp.dyn_col_used_backup, n, st.board->col_used.data());
    std::copy_n(sp.dyn_box_used_backup, n, st.board->box_used.data());
    st.board->empty_cells = sp.dyn_empty_backup;
}

inline bool propagate_singles(CandidateState& st, int max_steps) {
    const int nn = st.topo->nn;
    for (int step = 0; step < max_steps; ++step) {
        bool changed = false;
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            const uint64_t m = st.cands[idx];
            if (m == 0ULL) return false;
            const int sd = config::single_digit_from_mask(m);
            if (sd == 0) continue;
            if (!st.place(idx, sd)) return false;
            changed = true;
        }
        if (!changed) break;
    }
    return true;
}

inline bool probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int digit,
    int max_steps,
    ExactPatternScratchpad& sp) {
    snapshot_state(st, sp);

    bool contradiction = false;
    if (!st.place(idx, digit)) {
        contradiction = true;
    } else if (!propagate_singles(st, max_steps)) {
        contradiction = true;
    }

    restore_state(st, sp);
    return contradiction;
}

} // namespace sudoku_hpc::logic::shared
