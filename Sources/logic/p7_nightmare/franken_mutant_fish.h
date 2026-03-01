// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: franken_mutant_fish.h (Level 7 - Nightmare)
// Description: Direct mixed fish detector with both orientations:
// row+box bases vs column covers and col+box bases vs row covers.
// ============================================================================

#pragma once

#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool cell_in_mixed_base(
    const CandidateState& st,
    int idx,
    const int* base_ids,
    int base_count,
    bool line_is_row) {
    const int line = line_is_row ? st.topo->cell_row[idx] : st.topo->cell_col[idx];
    const int box = st.topo->cell_box[idx];
    for (int i = 0; i < base_count; ++i) {
        const int b = base_ids[i];
        if (b < st.topo->n) {
            if (line == b) return true;
        } else {
            if (box == (b - st.topo->n)) return true;
        }
    }
    return false;
}

inline bool build_line_box_base_masks(
    const CandidateState& st,
    int digit,
    int fish_size,
    bool line_is_row,
    int* out_ids,
    uint64_t* out_masks,
    int& out_count) {
    const int n = st.topo->n;
    const uint64_t bit = 1ULL << (digit - 1);
    out_count = 0;

    // Line houses as bases (rows or cols).
    for (int line = 0; line < n; ++line) {
        uint64_t cover_mask = 0ULL;
        for (int orth = 0; orth < n; ++orth) {
            const int idx = line_is_row ? (line * n + orth) : (orth * n + line);
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            const int cover = line_is_row ? st.topo->cell_col[idx] : st.topo->cell_row[idx];
            cover_mask |= (1ULL << cover);
        }
        const int pc = std::popcount(cover_mask);
        if (pc >= 2 && pc <= fish_size) {
            out_ids[out_count] = line;
            out_masks[out_count] = cover_mask;
            ++out_count;
        }
    }

    // Boxes as bases.
    for (int b = 0; b < n; ++b) {
        const int h = 2 * n + b;
        const int p0 = st.topo->house_offsets[h];
        const int p1 = st.topo->house_offsets[h + 1];
        uint64_t cover_mask = 0ULL;
        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            const int cover = line_is_row ? st.topo->cell_col[idx] : st.topo->cell_row[idx];
            cover_mask |= (1ULL << cover);
        }
        const int pc = std::popcount(cover_mask);
        if (pc >= 2 && pc <= fish_size) {
            out_ids[out_count] = n + b;
            out_masks[out_count] = cover_mask;
            ++out_count;
        }
    }

    return out_count >= fish_size;
}

inline ApplyResult apply_mixed_row_box_vs_col_fish(
    CandidateState& st,
    int fish_size,
    bool line_is_row,
    bool& progress,
    StrategyStats& s,
    uint64_t t0) {
    const int n = st.topo->n;
    if (n > 64 || fish_size < 2 || fish_size > 4) {
        return ApplyResult::NoProgress;
    }

    int base_ids[128]{};
    uint64_t base_masks[128]{};
    int selected[4]{};

    for (int d = 1; d <= n; ++d) {
        int base_count = 0;
        if (!build_line_box_base_masks(st, d, fish_size, line_is_row, base_ids, base_masks, base_count)) {
            continue;
        }
        const uint64_t d_bit = 1ULL << (d - 1);

        for (int i0 = 0; i0 < base_count; ++i0) {
            selected[0] = i0;
            const int i1_start = (fish_size >= 2) ? (i0 + 1) : base_count;
            const int i1_end = (fish_size >= 2) ? base_count : (i0 + 1);
            for (int i1 = i1_start; i1 < i1_end; ++i1) {
                if (fish_size >= 2) selected[1] = i1;
                const int i2_start = (fish_size >= 3) ? (i1 + 1) : i1_end;
                const int i2_end = (fish_size >= 3) ? base_count : (i1 + 1);
                for (int i2 = i2_start; i2 < i2_end; ++i2) {
                    if (fish_size >= 3) selected[2] = i2;
                    const int i3_start = (fish_size >= 4) ? (i2 + 1) : i2_end;
                    const int i3_end = (fish_size >= 4) ? base_count : (i2 + 1);
                    for (int i3 = i3_start; i3 < i3_end; ++i3) {
                        if (fish_size >= 4) selected[3] = i3;

                        uint64_t cover_cols = 0ULL;
                        for (int k = 0; k < fish_size; ++k) {
                            cover_cols |= base_masks[selected[k]];
                        }
                        if (std::popcount(cover_cols) != fish_size) {
                            continue;
                        }

                        int chosen_base_ids[4]{};
                        for (int k = 0; k < fish_size; ++k) {
                            chosen_base_ids[k] = base_ids[selected[k]];
                        }

                        for (int cover = 0; cover < n; ++cover) {
                            if ((cover_cols & (1ULL << cover)) == 0ULL) continue;
                            for (int orth = 0; orth < n; ++orth) {
                                const int idx = line_is_row ? (orth * n + cover) : (cover * n + orth);
                                if (st.board->values[idx] != 0) continue;
                                if ((st.cands[idx] & d_bit) == 0ULL) continue;
                                if (cell_in_mixed_base(st, idx, chosen_base_ids, fish_size, line_is_row)) continue;

                                const ApplyResult er = st.eliminate(idx, d_bit);
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

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_franken_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool progress = false;
    ApplyResult ar = apply_mixed_row_box_vs_col_fish(st, 3, true, progress, s, t0);
    if (ar == ApplyResult::Contradiction) return ar;
    ar = apply_mixed_row_box_vs_col_fish(st, 3, false, progress, s, t0);
    if (ar == ApplyResult::Contradiction) {
        return ar;
    }
    if (progress) {
        ++s.hit_count;
        r.used_franken_fish = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_mutant_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool progress = false;
    ApplyResult ar = apply_mixed_row_box_vs_col_fish(st, 4, true, progress, s, t0);
    if (ar == ApplyResult::Contradiction) return ar;
    ar = apply_mixed_row_box_vs_col_fish(st, 4, false, progress, s, t0);
    if (ar == ApplyResult::Contradiction) {
        return ar;
    }
    if (progress) {
        ++s.hit_count;
        r.used_mutant_fish = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
