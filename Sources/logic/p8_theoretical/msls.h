// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: msls.h (Level 8 - Theoretical)
// Description: MSLS (Multi-Sector Locked Sets) with direct sector-cluster
// probing and bounded composite fallback, zero-allocation.
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

#include "../p7_nightmare/aic_grouped_aic.h"
#include "../p7_nightmare/grouped_x_cycle.h"
#include "../p7_nightmare/als_xy_wing_chain.h"
#include "../p7_nightmare/kraken_fish.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool msls_propagate_singles(CandidateState& st, int max_steps) {
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

inline bool msls_probe_candidate_contradiction(
    CandidateState& st,
    int idx,
    int d,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    std::copy_n(st.cands, nn, sp.dyn_cands_backup);
    std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
    sp.dyn_empty_backup = st.board->empty_cells;

    bool contradiction = false;
    if (!st.place(idx, d)) {
        contradiction = true;
    } else if (!msls_propagate_singles(st, max_steps)) {
        contradiction = true;
    }

    std::copy_n(sp.dyn_cands_backup, nn, st.cands);
    std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
    st.board->empty_cells = sp.dyn_empty_backup;
    return contradiction;
}

inline int msls_relation_mask(const CandidateState& st, int a, int b) {
    int mask = 0;
    if (st.topo->cell_row[a] == st.topo->cell_row[b]) mask |= 1;
    if (st.topo->cell_col[a] == st.topo->cell_col[b]) mask |= 2;
    if (st.topo->cell_box[a] == st.topo->cell_box[b]) mask |= 4;
    return mask;
}

inline ApplyResult apply_msls_direct(CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n < 6 || n > 40) return ApplyResult::NoProgress;
    if (st.board->empty_cells > (nn - 6 * n)) return ApplyResult::NoProgress;

    auto& sp = shared::exact_pattern_scratchpad();
    int cand_cells[512]{};
    int cluster[64]{};
    int in_cluster[4096]{};

    const int anchor_cap = std::clamp(6 + n / 3, 8, 18);
    const int pattern_cap = std::clamp(8 + n / 2, 10, 28);
    const int probe_steps = std::clamp(6 + n / 4, 8, 14);
    int pattern_count = 0;
    int probe_budget = std::clamp(8 + n / 2, 10, 26);

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        int cc = 0;
        for (int idx = 0; idx < nn && cc < 512; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            cand_cells[cc++] = idx;
        }
        if (cc < 8) continue;

        const int anchors = std::min(cc, anchor_cap);
        for (int ai = 0; ai < anchors; ++ai) {
            if (pattern_count >= pattern_cap || probe_budget <= 0) return ApplyResult::NoProgress;
            const int anchor = cand_cells[ai];
            const int ar = st.topo->cell_row[anchor];
            const int ac = st.topo->cell_col[anchor];
            const int ab = st.topo->cell_box[anchor];

            std::fill_n(in_cluster, nn, 0);
            int cl = 0;
            for (int i = 0; i < cc && cl < 64; ++i) {
                const int idx = cand_cells[i];
                if (st.topo->cell_row[idx] == ar ||
                    st.topo->cell_col[idx] == ac ||
                    st.topo->cell_box[idx] == ab) {
                    in_cluster[idx] = 1;
                    cluster[cl++] = idx;
                }
            }
            if (cl < 5 || cl > 16) continue;

            int row_seen[64]{}, col_seen[64]{}, box_seen[64]{};
            int ru = 0, cu = 0, bu = 0;
            for (int i = 0; i < cl; ++i) {
                const int idx = cluster[i];
                const int rr = st.topo->cell_row[idx];
                const int ccv = st.topo->cell_col[idx];
                const int bb = st.topo->cell_box[idx];
                if (!row_seen[rr]) {
                    row_seen[rr] = 1;
                    ++ru;
                }
                if (!col_seen[ccv]) {
                    col_seen[ccv] = 1;
                    ++cu;
                }
                if (!box_seen[bb]) {
                    box_seen[bb] = 1;
                    ++bu;
                }
            }
            if (ru < 2 || cu < 2 || bu < 2) continue;
            ++pattern_count;

            for (int ti = 0; ti < cc && probe_budget > 0; ++ti) {
                const int t = cand_cells[ti];
                if (in_cluster[t]) continue;

                int seen_cells = 0;
                int rel_mask = 0;
                for (int ci = 0; ci < cl; ++ci) {
                    const int cidx = cluster[ci];
                    if (!st.is_peer(t, cidx)) continue;
                    ++seen_cells;
                    rel_mask |= msls_relation_mask(st, t, cidx);
                    if (seen_cells >= 3 && std::popcount(static_cast<unsigned int>(rel_mask)) >= 2) break;
                }
                if (seen_cells < 2) continue;
                if (std::popcount(static_cast<unsigned int>(rel_mask)) < 2) continue;

                --probe_budget;
                if (!msls_probe_candidate_contradiction(st, t, d, probe_steps, sp)) continue;
                const ApplyResult er = st.eliminate(t, bit);
                if (er != ApplyResult::NoProgress) return er;
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_msls(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 5 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};

    ApplyResult ar = apply_msls_direct(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_msls = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    const int depth_cap = std::clamp(8 + (st.board->empty_cells / std::max(1, n)), 10, 16);
    bool used_dynamic = false;
    ar = p7_nightmare::bounded_implication_core(st, tmp, r, depth_cap, used_dynamic);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_msls = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = p7_nightmare::apply_grouped_x_cycle(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_msls = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    ar = p7_nightmare::apply_als_chain(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_msls = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ar;
    }

    // Kraken fallback only for very late boards.
    if (st.board->empty_cells <= (nn - 7 * n)) {
        ar = p7_nightmare::apply_kraken_fish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_msls = true;
            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
            return ar;
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical
