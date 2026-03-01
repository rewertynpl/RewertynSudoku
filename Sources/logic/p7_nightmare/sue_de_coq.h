// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: sue_de_coq.h (Level 7 - Nightmare)
// Description: Full Sue de Coq implementation for row-box and col-box
// intersections. Zero-allocation, fixed-size local buffers only.
// ============================================================================

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

template <class Fn>
inline void for_each_combo_up_to3(int count, int choose, const Fn& fn) {
    if (choose == 1) {
        for (int a = 0; a < count; ++a) {
            fn(a, -1, -1);
        }
        return;
    }
    if (choose == 2) {
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                fn(a, b, -1);
            }
        }
        return;
    }
    if (choose == 3) {
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                for (int c = b + 1; c < count; ++c) {
                    fn(a, b, c);
                }
            }
        }
    }
}

inline uint64_t pool_mask_from_selection(
    const CandidateState& st,
    const int* pool,
    int i1,
    int i2,
    int i3) {
    uint64_t m = 0ULL;
    if (i1 >= 0) m |= st.cands[pool[i1]];
    if (i2 >= 0) m |= st.cands[pool[i2]];
    if (i3 >= 0) m |= st.cands[pool[i3]];
    return m;
}

inline bool idx_equals_any_selected(int idx, const int* pool, int i1, int i2, int i3) {
    return (i1 >= 0 && idx == pool[i1]) || (i2 >= 0 && idx == pool[i2]) || (i3 >= 0 && idx == pool[i3]);
}

inline ApplyResult scan_row_box_intersections(CandidateState& st, bool& progress, StrategyStats& s, uint64_t t0) {
    const int n = st.topo->n;
    bool contradiction = false;

    for (int brg = 0; brg < st.topo->box_rows_count; ++brg) {
        for (int bcg = 0; bcg < st.topo->box_cols_count; ++bcg) {
            const int r0 = brg * st.topo->box_rows;
            const int c0 = bcg * st.topo->box_cols;

            for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                const int intersect_row = r0 + dr;

                int a_cells[64]{};
                int a_cnt = 0;
                uint64_t ma = 0ULL;
                for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                    const int idx = intersect_row * n + (c0 + dc);
                    if (st.board->values[idx] == 0) {
                        a_cells[a_cnt++] = idx;
                        ma |= st.cands[idx];
                    }
                }
                if (a_cnt < 2) continue;

                int b_pool[64]{};
                int b_pool_cnt = 0;
                for (int c = 0; c < n; ++c) {
                    if (c >= c0 && c < c0 + st.topo->box_cols) continue;
                    const int idx = intersect_row * n + c;
                    if (st.board->values[idx] == 0) {
                        b_pool[b_pool_cnt++] = idx;
                    }
                }

                int c_pool[64]{};
                int c_pool_cnt = 0;
                for (int b_dr = 0; b_dr < st.topo->box_rows; ++b_dr) {
                    if (b_dr == dr) continue;
                    for (int b_dc = 0; b_dc < st.topo->box_cols; ++b_dc) {
                        const int idx = (r0 + b_dr) * n + (c0 + b_dc);
                        if (st.board->values[idx] == 0) {
                            c_pool[c_pool_cnt++] = idx;
                        }
                    }
                }

                const int b_limit = std::min(b_pool_cnt, 3);
                const int c_limit = std::min(c_pool_cnt, 3);
                if (b_limit <= 0 || c_limit <= 0) continue;

                for (int b_choose = 1; b_choose <= b_limit; ++b_choose) {
                    for (int c_choose = 1; c_choose <= c_limit; ++c_choose) {
                        const int target_cands = a_cnt + b_choose + c_choose;
                        if (std::popcount(ma) > target_cands) continue;

                        for_each_combo_up_to3(b_pool_cnt, b_choose, [&](int bi1, int bi2, int bi3) {
                            if (contradiction) return;
                            const uint64_t mb = pool_mask_from_selection(st, b_pool, bi1, bi2, bi3);
                            if ((mb & ~ma) != 0ULL) return;

                            for_each_combo_up_to3(c_pool_cnt, c_choose, [&](int ci1, int ci2, int ci3) {
                                if (contradiction) return;
                                const uint64_t mc = pool_mask_from_selection(st, c_pool, ci1, ci2, ci3);
                                if ((mc & ~ma) != 0ULL) return;

                                const uint64_t union_mask = ma | mb | mc;
                                if (std::popcount(union_mask) != target_cands) return;

                                if (mb != 0ULL) {
                                    for (int c = 0; c < n; ++c) {
                                        if (c >= c0 && c < c0 + st.topo->box_cols) continue;
                                        const int idx = intersect_row * n + c;
                                        if (st.board->values[idx] != 0) continue;
                                        if (idx_equals_any_selected(idx, b_pool, bi1, bi2, bi3)) continue;
                                        const ApplyResult er = st.eliminate(idx, mb);
                                        if (er == ApplyResult::Contradiction) {
                                            contradiction = true;
                                            return;
                                        }
                                        progress = progress || (er == ApplyResult::Progress);
                                    }
                                }

                                if (mc != 0ULL) {
                                    for (int b_dr = 0; b_dr < st.topo->box_rows; ++b_dr) {
                                        if (b_dr == dr) continue;
                                        for (int b_dc = 0; b_dc < st.topo->box_cols; ++b_dc) {
                                            const int idx = (r0 + b_dr) * n + (c0 + b_dc);
                                            if (st.board->values[idx] != 0) continue;
                                            if (idx_equals_any_selected(idx, c_pool, ci1, ci2, ci3)) continue;
                                            const ApplyResult er = st.eliminate(idx, mc);
                                            if (er == ApplyResult::Contradiction) {
                                                contradiction = true;
                                                return;
                                            }
                                            progress = progress || (er == ApplyResult::Progress);
                                        }
                                    }
                                }
                            });
                        });
                        if (contradiction) return ApplyResult::Contradiction;
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult scan_col_box_intersections(CandidateState& st, bool& progress, StrategyStats& s, uint64_t t0) {
    const int n = st.topo->n;
    bool contradiction = false;

    for (int brg = 0; brg < st.topo->box_rows_count; ++brg) {
        for (int bcg = 0; bcg < st.topo->box_cols_count; ++bcg) {
            const int r0 = brg * st.topo->box_rows;
            const int c0 = bcg * st.topo->box_cols;

            for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                const int intersect_col = c0 + dc;

                int a_cells[64]{};
                int a_cnt = 0;
                uint64_t ma = 0ULL;
                for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                    const int idx = (r0 + dr) * n + intersect_col;
                    if (st.board->values[idx] == 0) {
                        a_cells[a_cnt++] = idx;
                        ma |= st.cands[idx];
                    }
                }
                if (a_cnt < 2) continue;

                int b_pool[64]{};
                int b_pool_cnt = 0;
                for (int r = 0; r < n; ++r) {
                    if (r >= r0 && r < r0 + st.topo->box_rows) continue;
                    const int idx = r * n + intersect_col;
                    if (st.board->values[idx] == 0) {
                        b_pool[b_pool_cnt++] = idx;
                    }
                }

                int c_pool[64]{};
                int c_pool_cnt = 0;
                for (int b_dr = 0; b_dr < st.topo->box_rows; ++b_dr) {
                    for (int b_dc = 0; b_dc < st.topo->box_cols; ++b_dc) {
                        if (b_dc == dc) continue;
                        const int idx = (r0 + b_dr) * n + (c0 + b_dc);
                        if (st.board->values[idx] == 0) {
                            c_pool[c_pool_cnt++] = idx;
                        }
                    }
                }

                const int b_limit = std::min(b_pool_cnt, 3);
                const int c_limit = std::min(c_pool_cnt, 3);
                if (b_limit <= 0 || c_limit <= 0) continue;

                for (int b_choose = 1; b_choose <= b_limit; ++b_choose) {
                    for (int c_choose = 1; c_choose <= c_limit; ++c_choose) {
                        const int target_cands = a_cnt + b_choose + c_choose;
                        if (std::popcount(ma) > target_cands) continue;

                        for_each_combo_up_to3(b_pool_cnt, b_choose, [&](int bi1, int bi2, int bi3) {
                            if (contradiction) return;
                            const uint64_t mb = pool_mask_from_selection(st, b_pool, bi1, bi2, bi3);
                            if ((mb & ~ma) != 0ULL) return;

                            for_each_combo_up_to3(c_pool_cnt, c_choose, [&](int ci1, int ci2, int ci3) {
                                if (contradiction) return;
                                const uint64_t mc = pool_mask_from_selection(st, c_pool, ci1, ci2, ci3);
                                if ((mc & ~ma) != 0ULL) return;

                                const uint64_t union_mask = ma | mb | mc;
                                if (std::popcount(union_mask) != target_cands) return;

                                if (mb != 0ULL) {
                                    for (int r = 0; r < n; ++r) {
                                        if (r >= r0 && r < r0 + st.topo->box_rows) continue;
                                        const int idx = r * n + intersect_col;
                                        if (st.board->values[idx] != 0) continue;
                                        if (idx_equals_any_selected(idx, b_pool, bi1, bi2, bi3)) continue;
                                        const ApplyResult er = st.eliminate(idx, mb);
                                        if (er == ApplyResult::Contradiction) {
                                            contradiction = true;
                                            return;
                                        }
                                        progress = progress || (er == ApplyResult::Progress);
                                    }
                                }

                                if (mc != 0ULL) {
                                    for (int b_dr = 0; b_dr < st.topo->box_rows; ++b_dr) {
                                        for (int b_dc = 0; b_dc < st.topo->box_cols; ++b_dc) {
                                            if (b_dc == dc) continue;
                                            const int idx = (r0 + b_dr) * n + (c0 + b_dc);
                                            if (st.board->values[idx] != 0) continue;
                                            if (idx_equals_any_selected(idx, c_pool, ci1, ci2, ci3)) continue;
                                            const ApplyResult er = st.eliminate(idx, mc);
                                            if (er == ApplyResult::Contradiction) {
                                                contradiction = true;
                                                return;
                                            }
                                            progress = progress || (er == ApplyResult::Progress);
                                        }
                                    }
                                }
                            });
                        });
                        if (contradiction) return ApplyResult::Contradiction;
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_sue_de_coq(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool progress = false;
    {
        const ApplyResult rr = scan_row_box_intersections(st, progress, s, t0);
        if (rr == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return rr;
        }
        const ApplyResult rc = scan_col_box_intersections(st, progress, s, t0);
        if (rc == ApplyResult::Contradiction) {
            s.elapsed_ns += st.now_ns() - t0;
            return rc;
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_sue_de_coq = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
