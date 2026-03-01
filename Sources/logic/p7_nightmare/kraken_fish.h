// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: kraken_fish.h (Level 7 - Nightmare)
// Description: Kraken Fish as a combination of finned fish body detection
// and deep alternating tentacle chains (zero-allocation).
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../p6_diabolical/finned_jelly_sword.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline ApplyResult apply_kraken_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    // Kraken patterns become useful in late game where fish body + tentacles
    // are constrained enough to produce deterministic eliminations.
    if (st.board->empty_cells > (st.topo->nn - st.topo->n / 2)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    // Step 1: fish body
    StrategyStats tmp{};
    ApplyResult ar = p6_diabolical::apply_finned_swordfish_jellyfish(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_kraken_fish = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }

    // Step 2: deep tentacles
    bool used = false;
    const int depth_cap = std::clamp(14 + (st.board->empty_cells / std::max(1, st.topo->n)), 16, 26);
    ar = alternating_chain_core(st, depth_cap, false, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_kraken_fish = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
