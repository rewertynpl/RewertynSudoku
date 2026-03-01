// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: medusa_3d.h (Level 7 - Nightmare)
// Description: 3D Medusa-oriented chain coloring pass focused on boards with
// rich bivalue structure (zero-allocation).
// ============================================================================

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline ApplyResult apply_medusa_3d(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;

    int bivalue_count = 0;
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 2) ++bivalue_count;
    }
    if (bivalue_count < 3) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int depth_cap = std::clamp(8 + (bivalue_count / std::max(1, n / 2)), 10, 18);
    bool used = false;
    const ApplyResult ar = alternating_chain_core(st, depth_cap, false, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_medusa_3d = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
