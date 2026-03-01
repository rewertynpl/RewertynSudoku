// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: finned_jelly_sword.h (Level 6 - Diabolical)
// Description: Jellyfish and direct finned Swordfish/Jellyfish passes,
// zero-allocation.
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

#include "../p5_expert/finned_fish.h"
#include "../p4_hard/fish_basic.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline ApplyResult apply_jellyfish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    if (n < 5) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    auto& sp = shared::exact_pattern_scratchpad();
    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        std::fill_n(sp.fish_row_masks, n, 0ULL);
        std::fill_n(sp.fish_col_masks, n, 0ULL);

        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            const int rr = st.topo->cell_row[idx];
            const int cc = st.topo->cell_col[idx];
            sp.fish_row_masks[rr] |= (1ULL << cc);
            sp.fish_col_masks[cc] |= (1ULL << rr);
        }

        int row_count = 0;
        int col_count = 0;
        for (int rr = 0; rr < n; ++rr) {
            const int cnt = std::popcount(sp.fish_row_masks[rr]);
            if (cnt >= 2 && cnt <= 4) sp.active_rows[row_count++] = rr;
        }
        for (int cc = 0; cc < n; ++cc) {
            const int cnt = std::popcount(sp.fish_col_masks[cc]);
            if (cnt >= 2 && cnt <= 4) sp.active_cols[col_count++] = cc;
        }

        for (int i = 0; i + 3 < row_count; ++i) {
            const int r1 = sp.active_rows[i];
            const uint64_t u1 = sp.fish_row_masks[r1];
            for (int j = i + 1; j + 2 < row_count; ++j) {
                const int r2 = sp.active_rows[j];
                const uint64_t u2 = u1 | sp.fish_row_masks[r2];
                if (std::popcount(u2) > 4) continue;
                for (int k = j + 1; k + 1 < row_count; ++k) {
                    const int r3 = sp.active_rows[k];
                    const uint64_t u3 = u2 | sp.fish_row_masks[r3];
                    if (std::popcount(u3) > 4) continue;
                    for (int l = k + 1; l < row_count; ++l) {
                        const int r4 = sp.active_rows[l];
                        const uint64_t cols_union = u3 | sp.fish_row_masks[r4];
                        if (std::popcount(cols_union) != 4) continue;
                        for (uint64_t w = cols_union; w != 0ULL; w &= (w - 1ULL)) {
                            const int cc = config::bit_ctz_u64(w);
                            for (int rr = 0; rr < n; ++rr) {
                                if (rr == r1 || rr == r2 || rr == r3 || rr == r4) continue;
                                const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                if (er == ApplyResult::Contradiction) {
                                    s.elapsed_ns += st.now_ns() - t0;
                                    return er;
                                }
                                if (er == ApplyResult::Progress) {
                                    ++s.hit_count;
                                    r.used_jellyfish = true;
                                    s.elapsed_ns += st.now_ns() - t0;
                                    return ApplyResult::Progress;
                                }
                            }
                        }
                    }
                }
            }
        }

        for (int i = 0; i + 3 < col_count; ++i) {
            const int c1 = sp.active_cols[i];
            const uint64_t u1 = sp.fish_col_masks[c1];
            for (int j = i + 1; j + 2 < col_count; ++j) {
                const int c2 = sp.active_cols[j];
                const uint64_t u2 = u1 | sp.fish_col_masks[c2];
                if (std::popcount(u2) > 4) continue;
                for (int k = j + 1; k + 1 < col_count; ++k) {
                    const int c3 = sp.active_cols[k];
                    const uint64_t u3 = u2 | sp.fish_col_masks[c3];
                    if (std::popcount(u3) > 4) continue;
                    for (int l = k + 1; l < col_count; ++l) {
                        const int c4 = sp.active_cols[l];
                        const uint64_t rows_union = u3 | sp.fish_col_masks[c4];
                        if (std::popcount(rows_union) != 4) continue;
                        for (uint64_t w = rows_union; w != 0ULL; w &= (w - 1ULL)) {
                            const int rr = config::bit_ctz_u64(w);
                            for (int cc = 0; cc < n; ++cc) {
                                if (cc == c1 || cc == c2 || cc == c3 || cc == c4) continue;
                                const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                if (er == ApplyResult::Contradiction) {
                                    s.elapsed_ns += st.now_ns() - t0;
                                    return er;
                                }
                                if (er == ApplyResult::Progress) {
                                    ++s.hit_count;
                                    r.used_jellyfish = true;
                                    s.elapsed_ns += st.now_ns() - t0;
                                    return ApplyResult::Progress;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_finned_fish_direct(
    CandidateState& st,
    int fish_size,
    bool row_based,
    bool& progress,
    StrategyStats& s,
    uint64_t t0) {
    const int n = st.topo->n;
    if (fish_size < 3 || fish_size > 4 || n < fish_size + 1 || n > 64) {
        return ApplyResult::NoProgress;
    }

    auto& sp = shared::exact_pattern_scratchpad();
    int active_lines[64]{};
    uint64_t line_masks[64]{};
    int line_count = 0;
    int combo_checks = 0;
    const int combo_cap = (fish_size == 3) ? 10000 : 7000;

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        line_count = 0;

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
            if (pc < 2 || pc > fish_size + 1) continue;
            if (line_count < 64) {
                active_lines[line_count] = line;
                line_masks[line_count] = mask;
                ++line_count;
            }
        }

        if (line_count < fish_size) continue;
        if (line_count > 20) continue;

        for (int i0 = 0; i0 < line_count; ++i0) {
            for (int i1 = i0 + 1; i1 < line_count; ++i1) {
                if (fish_size == 3) {
                    for (int i2 = i1 + 1; i2 < line_count; ++i2) {
                        if (++combo_checks > combo_cap) return ApplyResult::NoProgress;
                        const uint64_t u = line_masks[i0] | line_masks[i1] | line_masks[i2];
                        if (std::popcount(u) != fish_size + 1) continue;

                        int lines[3] = {active_lines[i0], active_lines[i1], active_lines[i2]};
                        uint64_t masks[3] = {line_masks[i0], line_masks[i1], line_masks[i2]};

                        for (uint64_t w = u; w != 0ULL; w &= (w - 1ULL)) {
                            const int fin_cover = config::bit_ctz_u64(w);
                            int fin_line = -1;
                            int fin_cnt = 0;
                            for (int k = 0; k < 3; ++k) {
                                if ((masks[k] & (1ULL << fin_cover)) == 0ULL) continue;
                                ++fin_cnt;
                                fin_line = lines[k];
                            }
                            if (fin_cnt != 1 || fin_line < 0) continue;

                            const int fin_idx = row_based ? (fin_line * n + fin_cover) : (fin_cover * n + fin_line);
                            const int fin_box = st.topo->cell_box[fin_idx];

                            for (uint64_t wc = u; wc != 0ULL; wc &= (wc - 1ULL)) {
                                const int cover = config::bit_ctz_u64(wc);
                                if (cover == fin_cover) continue;
                                for (int orth = 0; orth < n; ++orth) {
                                    const int idx = row_based ? (orth * n + cover) : (cover * n + orth);
                                    if (st.board->values[idx] != 0) continue;
                                    if ((st.cands[idx] & bit) == 0ULL) continue;
                                    if (st.topo->cell_box[idx] != fin_box) continue;

                                    const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
                                    bool in_base = false;
                                    for (int k = 0; k < 3; ++k) {
                                        if (idx_line == lines[k]) {
                                            in_base = true;
                                            break;
                                        }
                                    }
                                    if (in_base) continue;

                                    const ApplyResult er = st.eliminate(idx, bit);
                                    if (er == ApplyResult::Contradiction) {
                                        s.elapsed_ns += st.now_ns() - t0;
                                        return er;
                                    }
                                    progress = progress || (er == ApplyResult::Progress);
                                }
                            }
                        }
                    }
                } else {
                    for (int i2 = i1 + 1; i2 < line_count; ++i2) {
                        for (int i3 = i2 + 1; i3 < line_count; ++i3) {
                            if (++combo_checks > combo_cap) return ApplyResult::NoProgress;
                            const uint64_t u = line_masks[i0] | line_masks[i1] | line_masks[i2] | line_masks[i3];
                            if (std::popcount(u) != fish_size + 1) continue;

                            int lines[4] = {active_lines[i0], active_lines[i1], active_lines[i2], active_lines[i3]};
                            uint64_t masks[4] = {line_masks[i0], line_masks[i1], line_masks[i2], line_masks[i3]};

                            for (uint64_t w = u; w != 0ULL; w &= (w - 1ULL)) {
                                const int fin_cover = config::bit_ctz_u64(w);
                                int fin_line = -1;
                                int fin_cnt = 0;
                                for (int k = 0; k < 4; ++k) {
                                    if ((masks[k] & (1ULL << fin_cover)) == 0ULL) continue;
                                    ++fin_cnt;
                                    fin_line = lines[k];
                                }
                                if (fin_cnt != 1 || fin_line < 0) continue;

                                const int fin_idx = row_based ? (fin_line * n + fin_cover) : (fin_cover * n + fin_line);
                                const int fin_box = st.topo->cell_box[fin_idx];

                                for (uint64_t wc = u; wc != 0ULL; wc &= (wc - 1ULL)) {
                                    const int cover = config::bit_ctz_u64(wc);
                                    if (cover == fin_cover) continue;
                                    for (int orth = 0; orth < n; ++orth) {
                                        const int idx = row_based ? (orth * n + cover) : (cover * n + orth);
                                        if (st.board->values[idx] != 0) continue;
                                        if ((st.cands[idx] & bit) == 0ULL) continue;
                                        if (st.topo->cell_box[idx] != fin_box) continue;

                                        const int idx_line = row_based ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
                                        bool in_base = false;
                                        for (int k = 0; k < 4; ++k) {
                                            if (idx_line == lines[k]) {
                                                in_base = true;
                                                break;
                                            }
                                        }
                                        if (in_base) continue;

                                        const ApplyResult er = st.eliminate(idx, bit);
                                        if (er == ApplyResult::Contradiction) {
                                            s.elapsed_ns += st.now_ns() - t0;
                                            return er;
                                        }
                                        progress = progress || (er == ApplyResult::Progress);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_finned_swordfish_jellyfish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    StrategyStats tmp{};
    bool progress = false;
    const int n = st.topo->n;
    const int nn = st.topo->nn;

    // Heavy direct finned-fish scan only in later board phase.
    if (st.board->empty_cells <= (nn - 4 * n)) {
        ApplyResult ar = apply_finned_fish_direct(st, 3, true, progress, s, t0);
        if (ar == ApplyResult::Contradiction) return ar;
        ar = apply_finned_fish_direct(st, 3, false, progress, s, t0);
        if (ar == ApplyResult::Contradiction) return ar;
        ar = apply_finned_fish_direct(st, 4, true, progress, s, t0);
        if (ar == ApplyResult::Contradiction) return ar;
        ar = apply_finned_fish_direct(st, 4, false, progress, s, t0);
        if (ar == ApplyResult::Contradiction) return ar;
        if (progress) {
            ++s.hit_count;
            r.used_finned_swordfish_jellyfish = true;
            s.elapsed_ns += st.now_ns() - t0;
            return ApplyResult::Progress;
        }
    }

    ApplyResult ar = logic::p5_expert::apply_finned_x_wing_sashimi(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_finned_swordfish_jellyfish = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    ar = logic::p4_hard::apply_swordfish(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_finned_swordfish_jellyfish = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    ar = apply_jellyfish(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_finned_swordfish_jellyfish = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
