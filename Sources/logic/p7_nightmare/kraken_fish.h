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
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/state_probe.h"
#include "../p6_diabolical/finned_jelly_sword.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline ApplyResult kraken_try_targets(
    CandidateState& st,
    uint64_t bit,
    bool row_based,
    const int* base_lines,
    int base_count,
    uint64_t covers_union,
    const int* fin_cells,
    int fin_count,
    int probe_steps) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;

    for (uint64_t w = covers_union; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
        const int cover = config::bit_ctz_u64(w);
        for (int orth = 0; orth < n; ++orth) {
            const int idx = row_based ? (orth * n + cover) : (cover * n + orth);
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;

            const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
            bool in_base = false;
            for (int i = 0; i < base_count; ++i) {
                if (idx_line == base_lines[i]) {
                    in_base = true;
                    break;
                }
            }
            if (in_base) continue;

            bool sees_tentacles = true;
            for (int i = 0; i < fin_count; ++i) {
                if (!st.is_peer(idx, fin_cells[i])) {
                    sees_tentacles = false;
                    break;
                }
            }
            if (!sees_tentacles) continue;

            const int digit = config::bit_ctz_u64(bit) + 1;
            if (!shared::probe_candidate_contradiction(st, idx, digit, probe_steps, sp)) continue;
            return st.eliminate(idx, bit);
        }
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult kraken_direct_scan_for_digit(
    CandidateState& st,
    int digit,
    int fish_size,
    bool row_based,
    int combo_cap,
    int probe_steps) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;
    const uint64_t bit = (1ULL << (digit - 1));

    int active_lines[64]{};
    uint64_t line_masks[64]{};
    int line_count = 0;
    for (int line = 0; line < n; ++line) {
        uint64_t mask = 0ULL;
        for (int orth = 0; orth < n; ++orth) {
            const int idx = row_based ? (line * n + orth) : (orth * n + line);
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            const int cover = row_based ? st.topo->cell_col[idx] : st.topo->cell_row[idx];
            mask |= (1ULL << cover);
        }
        const int pc = std::popcount(mask);
        if (pc >= 2 && pc <= fish_size + 1 && line_count < 64) {
            active_lines[line_count] = line;
            line_masks[line_count] = mask;
            ++line_count;
        }
    }
    if (line_count < fish_size) return ApplyResult::NoProgress;
    if (line_count > 16) return ApplyResult::NoProgress;

    int combo_checks = 0;
    auto process_combo = [&](const int* ids, int count) -> ApplyResult {
        uint64_t covers_union = 0ULL;
        int base_lines[5]{};
        uint64_t masks[5]{};
        for (int i = 0; i < count; ++i) {
            base_lines[i] = active_lines[ids[i]];
            masks[i] = line_masks[ids[i]];
            covers_union |= masks[i];
        }
        const int cover_pc = std::popcount(covers_union);
        if (cover_pc < fish_size || cover_pc > fish_size + 1) return ApplyResult::NoProgress;

        int fin_cells[4]{};
        int fin_count = 0;
        for (uint64_t w = covers_union; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
            const int cover = config::bit_ctz_u64(w);
            int owner = -1;
            int cnt = 0;
            for (int i = 0; i < count; ++i) {
                if ((masks[i] & (1ULL << cover)) == 0ULL) continue;
                owner = i;
                ++cnt;
            }
            if (cnt == 1 && fin_count < 4) {
                fin_cells[fin_count++] =
                    row_based ? (base_lines[owner] * n + cover) : (cover * n + base_lines[owner]);
            }
        }
        if (fin_count <= 0 || fin_count > 2) return ApplyResult::NoProgress;

        int common_box = st.topo->cell_box[fin_cells[0]];
        bool same_box = true;
        for (int i = 1; i < fin_count; ++i) {
            if (st.topo->cell_box[fin_cells[i]] != common_box) {
                same_box = false;
                break;
            }
        }
        if (!same_box) return ApplyResult::NoProgress;

        return kraken_try_targets(st, bit, row_based, base_lines, count, covers_union, fin_cells, fin_count, probe_steps);
    };

    if (fish_size == 3) {
        for (int i0 = 0; i0 < line_count; ++i0) {
            for (int i1 = i0 + 1; i1 < line_count; ++i1) {
                for (int i2 = i1 + 1; i2 < line_count; ++i2) {
                    if (++combo_checks > combo_cap) return ApplyResult::NoProgress;
                    const int ids[3] = {i0, i1, i2};
                    const ApplyResult ar = process_combo(ids, 3);
                    if (ar != ApplyResult::NoProgress) return ar;
                }
            }
        }
    } else {
        for (int i0 = 0; i0 < line_count; ++i0) {
            for (int i1 = i0 + 1; i1 < line_count; ++i1) {
                for (int i2 = i1 + 1; i2 < line_count; ++i2) {
                    for (int i3 = i2 + 1; i3 < line_count; ++i3) {
                        if (++combo_checks > combo_cap) return ApplyResult::NoProgress;
                        const int ids[4] = {i0, i1, i2, i3};
                        const ApplyResult ar = process_combo(ids, 4);
                        if (ar != ApplyResult::NoProgress) return ar;
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_kraken_fish_direct(CandidateState& st) {
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n < 6 || n > 25) return ApplyResult::NoProgress;
    if (st.board->empty_cells > (nn - 2 * n)) return ApplyResult::NoProgress;

    const int probe_steps = std::clamp(6 + n / 4, 8, 14);
    const int combo_cap = (n <= 12) ? 4000 : 1800;
    for (int d = 1; d <= n; ++d) {
        ApplyResult ar = kraken_direct_scan_for_digit(st, d, 3, true, combo_cap, probe_steps);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_direct_scan_for_digit(st, d, 3, false, combo_cap, probe_steps);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_direct_scan_for_digit(st, d, 4, true, combo_cap / 2, probe_steps);
        if (ar != ApplyResult::NoProgress) return ar;
        ar = kraken_direct_scan_for_digit(st, d, 4, false, combo_cap / 2, probe_steps);
        if (ar != ApplyResult::NoProgress) return ar;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_kraken_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    // Kraken patterns become useful in late game where fish body + tentacles
    // are constrained enough to produce deterministic eliminations.
    if (st.board->empty_cells > (st.topo->nn - st.topo->n / 2)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    ApplyResult ar = apply_kraken_fish_direct(st);
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

    // Step 1: fallback fish body
    StrategyStats tmp{};
    ar = p6_diabolical::apply_finned_swordfish_jellyfish(st, tmp, r);
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
