// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: exocet_family.h (Level 8 - Theoretical)
// Description: Exocet and Senior Exocet with direct structural probing
// on detected base/target layouts, zero-allocation.
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../p7_nightmare/aic_grouped_aic.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool exocet_propagate_singles(CandidateState& st, int max_steps) {
    const int nn = st.topo->nn;
    for (int iter = 0; iter < max_steps; ++iter) {
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

inline bool exocet_probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int digit,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    std::copy_n(st.cands, nn, sp.dyn_cands_backup);
    std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
    sp.dyn_empty_backup = st.board->empty_cells;

    bool contradiction = false;
    if (!st.place(idx, digit)) {
        contradiction = true;
    } else if (!exocet_propagate_singles(st, max_steps)) {
        contradiction = true;
    }

    std::copy_n(sp.dyn_cands_backup, nn, st.cands);
    std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
    st.board->empty_cells = sp.dyn_empty_backup;
    return contradiction;
}

inline ApplyResult exocet_structural_probe(
    CandidateState& st,
    int pattern_cap,
    int max_steps,
    bool senior_mode) {
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        return ApplyResult::NoProgress;
    }

    const int n = st.topo->n;
    auto& sp = shared::exact_pattern_scratchpad();
    int patterns = 0;
    int box_cells[64]{};

    for (int b = 0; b < n; ++b) {
        const int h = 2 * n + b;
        const int p0 = st.topo->house_offsets[h];
        const int p1 = st.topo->house_offsets[h + 1];
        int bc = 0;

        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            if (st.board->values[idx] != 0) continue;
            const int pc = std::popcount(st.cands[idx]);
            if (pc < 2 || pc > 6) continue;
            if (bc < 64) box_cells[bc++] = idx;
        }
        if (bc < 2) continue;

        for (int i = 0; i < bc; ++i) {
            const int base1 = box_cells[i];
            const int r1 = st.topo->cell_row[base1];
            const int c1 = st.topo->cell_col[base1];
            const uint64_t m1 = st.cands[base1];

            for (int j = i + 1; j < bc; ++j) {
                if (patterns >= pattern_cap) return ApplyResult::NoProgress;
                const int base2 = box_cells[j];
                const int r2 = st.topo->cell_row[base2];
                const int c2 = st.topo->cell_col[base2];
                if (r1 == r2 || c1 == c2) continue;

                const uint64_t m2 = st.cands[base2];
                const uint64_t core = m1 & m2;
                if (std::popcount(core) < 2) continue;

                const int t1 = r1 * n + c2;
                const int t2 = r2 * n + c1;
                if (st.board->values[t1] != 0 || st.board->values[t2] != 0) continue;

                const uint64_t mt1 = st.cands[t1];
                const uint64_t mt2 = st.cands[t2];
                if ((mt1 & core) == 0ULL || (mt2 & core) == 0ULL) continue;

                const int extra_pc = std::popcount((mt1 | mt2) & ~core);
                if (extra_pc > (senior_mode ? 6 : 4)) continue;

                ++patterns;
                const int scope[4] = {base1, base2, t1, t2};
                for (int s = 0; s < 4; ++s) {
                    const int idx = scope[s];
                    uint64_t probe = st.cands[idx];
                    const int per_cell_budget = senior_mode ? 3 : 2;
                    int tested = 0;

                    while (probe != 0ULL && tested < per_cell_budget) {
                        const uint64_t bit = config::bit_lsb(probe);
                        probe = config::bit_clear_lsb_u64(probe);
                        ++tested;
                        const int d = config::bit_ctz_u64(bit) + 1;
                        if (!exocet_probe_candidate_contradiction(st, idx, d, max_steps, sp)) continue;

                        const ApplyResult er = st.eliminate(idx, bit);
                        if (er == ApplyResult::Contradiction) return er;
                        if (er == ApplyResult::Progress) return er;
                    }
                }
            }
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_exocet_exact(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 4 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int pattern_cap = std::clamp(4 + n / 3, 6, 16);
    const int max_steps = std::clamp(6 + n / 4, 8, 14);

    const ApplyResult ar = exocet_structural_probe(st, pattern_cap, max_steps, false);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_exocet(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 5 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    ApplyResult ar = apply_exocet_exact(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    bool used = false;
    const int depth_cap = std::clamp(8 + st.board->empty_cells / std::max(1, n), 10, 16);
    ar = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = p7_nightmare::apply_grouped_aic(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_senior_exocet(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 6 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    const int pattern_cap = std::clamp(6 + n / 2, 8, 22);
    const int max_steps = std::clamp(10 + n / 3, 12, 20);

    ApplyResult ar = exocet_structural_probe(st, pattern_cap, max_steps, true);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_senior_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    bool used = false;
    const int depth_cap = std::clamp(12 + st.board->empty_cells / std::max(1, n), 14, 24);
    ar = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_senior_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = apply_exocet(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_senior_exocet = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical
