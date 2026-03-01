// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: unique_loop_avoidable_oddagon.h (Level 6 - Diabolical)
// Description: Direct detectors for Unique Loop, Avoidable Rectangle and
// Bivalue Oddagon (zero-allocation, asymmetric geometry compatible).
// ============================================================================

#pragma once

#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline constexpr int kMaxNN = 64 * 64;

inline bool rectangle_same_two_boxes(const CandidateState& st, int i1, int i2, int i3, int i4) {
    // Proper UR/AR rectangle uses exactly two boxes (works for asym boxes too).
    int b1 = st.topo->cell_box[i1];
    int b2 = st.topo->cell_box[i2];
    int b3 = st.topo->cell_box[i3];
    int b4 = st.topo->cell_box[i4];
    int unique = 1;
    if (b2 != b1) ++unique;
    if (b3 != b1 && b3 != b2) ++unique;
    if (b4 != b1 && b4 != b2 && b4 != b3) ++unique;
    return unique == 2;
}

inline ApplyResult apply_unique_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool progress = false;
    bool contradiction = false;

    for (int r1 = 0; r1 < n; ++r1) {
        for (int r2 = r1 + 1; r2 < n; ++r2) {
            for (int c1 = 0; c1 < n; ++c1) {
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const int i1 = r1 * n + c1;
                    const int i2 = r1 * n + c2;
                    const int i3 = r2 * n + c1;
                    const int i4 = r2 * n + c2;

                    if (!rectangle_same_two_boxes(st, i1, i2, i3, i4)) continue;
                    if (st.board->values[i1] != 0 || st.board->values[i2] != 0 ||
                        st.board->values[i3] != 0 || st.board->values[i4] != 0) {
                        continue;
                    }

                    const int cells[4] = {i1, i2, i3, i4};
                    for (int p = 0; p < 4; ++p) {
                        const uint64_t pair_mask = st.cands[cells[p]];
                        if (std::popcount(pair_mask) != 2) continue;

                        int exact_pair_cnt = 0;
                        int extra_idx = -1;
                        bool valid = true;

                        for (int k = 0; k < 4; ++k) {
                            const uint64_t cm = st.cands[cells[k]];
                            if ((cm & pair_mask) != pair_mask) {
                                valid = false;
                                break;
                            }
                            if (cm == pair_mask) {
                                ++exact_pair_cnt;
                            } else {
                                if (extra_idx != -1) {
                                    valid = false;
                                    break;
                                }
                                extra_idx = cells[k];
                            }
                        }

                        if (!valid || exact_pair_cnt != 3 || extra_idx < 0) continue;

                        const ApplyResult er = st.eliminate(extra_idx, pair_mask);
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

    if (progress) {
        ++s.hit_count;
        r.used_unique_loop = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_avoidable_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool progress = false;
    bool contradiction = false;

    for (int r1 = 0; r1 < n; ++r1) {
        for (int r2 = r1 + 1; r2 < n; ++r2) {
            for (int c1 = 0; c1 < n; ++c1) {
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const int i1 = r1 * n + c1;
                    const int i2 = r1 * n + c2;
                    const int i3 = r2 * n + c1;
                    const int i4 = r2 * n + c2;

                    if (!rectangle_same_two_boxes(st, i1, i2, i3, i4)) continue;

                    auto check_diag = [&](int s1, int s2, int u1, int u2) {
                        if (contradiction) return;
                        const int v1 = st.board->values[s1];
                        const int v2 = st.board->values[s2];
                        if (v1 == 0 || v2 == 0 || v1 == v2) return;
                        if (st.board->values[u1] != 0 || st.board->values[u2] != 0) return;

                        const uint64_t pair = (1ULL << (v1 - 1)) | (1ULL << (v2 - 1));
                        const uint64_t m1 = st.cands[u1];
                        const uint64_t m2 = st.cands[u2];
                        if ((m1 & pair) != pair || (m2 & pair) != pair) return;

                        if (m1 == pair && m2 != pair) {
                            const ApplyResult er = st.eliminate(u2, pair);
                            if (er == ApplyResult::Contradiction) {
                                contradiction = true;
                                return;
                            }
                            progress = progress || (er == ApplyResult::Progress);
                        } else if (m2 == pair && m1 != pair) {
                            const ApplyResult er = st.eliminate(u1, pair);
                            if (er == ApplyResult::Contradiction) {
                                contradiction = true;
                                return;
                            }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    };

                    check_diag(i1, i4, i2, i3);
                    check_diag(i2, i3, i1, i4);
                    if (contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return ApplyResult::Contradiction;
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_avoidable_rectangle = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline bool odd_length_path_between_strict_nodes(
    const CandidateState& st,
    const int* strict_cells,
    int strict_count,
    const int* cell_to_node,
    int start_node,
    int target_node) {
    int qnode[kMaxNN]{};
    int qpar[kMaxNN]{};
    int vis0[kMaxNN]{};
    int vis1[kMaxNN]{};

    for (int i = 0; i < strict_count; ++i) {
        vis0[i] = 0;
        vis1[i] = 0;
    }

    int qh = 0;
    int qt = 0;
    qnode[qt] = start_node;
    qpar[qt] = 0;
    ++qt;
    vis0[start_node] = 1;

    while (qh < qt) {
        const int u = qnode[qh];
        const int par = qpar[qh];
        ++qh;

        const int cell = strict_cells[u];
        const int p0 = st.topo->peer_offsets[cell];
        const int p1 = st.topo->peer_offsets[cell + 1];
        for (int p = p0; p < p1; ++p) {
            const int v_cell = st.topo->peers_flat[p];
            const int v = cell_to_node[v_cell];
            if (v < 0) continue;
            const int npar = 1 - par;
            if (v == target_node && npar == 1) {
                return true; // odd number of edges
            }
            int* vis = (npar == 0) ? vis0 : vis1;
            if (vis[v] != 0) continue;
            vis[v] = 1;
            qnode[qt] = v;
            qpar[qt] = npar;
            ++qt;
            if (qt >= strict_count * 2) break;
        }
    }
    return false;
}

inline ApplyResult apply_bivalue_oddagon(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    bool progress = false;

    int strict_cells[kMaxNN]{};
    int cell_to_node[kMaxNN]{};
    int pivot_neighbors[kMaxNN]{};

    for (int pivot = 0; pivot < nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;
        const uint64_t pm = st.cands[pivot];
        if (std::popcount(pm) < 3) continue;

        uint64_t w1 = pm;
        while (w1 != 0ULL) {
            const uint64_t b1 = w1 & (~w1 + 1ULL);
            w1 &= (w1 - 1ULL);
            uint64_t w2 = w1;
            while (w2 != 0ULL) {
                const uint64_t b2 = w2 & (~w2 + 1ULL);
                w2 &= (w2 - 1ULL);
                const uint64_t pair = b1 | b2;

                for (int i = 0; i < nn; ++i) {
                    cell_to_node[i] = -1;
                }

                int strict_count = 0;
                for (int idx = 0; idx < nn; ++idx) {
                    if (st.board->values[idx] != 0) continue;
                    if (st.cands[idx] != pair) continue;
                    cell_to_node[idx] = strict_count;
                    strict_cells[strict_count++] = idx;
                }
                if (strict_count < 3) continue;

                int neigh_cnt = 0;
                const int p0 = st.topo->peer_offsets[pivot];
                const int p1 = st.topo->peer_offsets[pivot + 1];
                for (int p = p0; p < p1; ++p) {
                    const int v_cell = st.topo->peers_flat[p];
                    const int v = cell_to_node[v_cell];
                    if (v < 0) continue;
                    pivot_neighbors[neigh_cnt++] = v;
                }
                if (neigh_cnt < 2) continue;

                bool elim_pair_from_pivot = false;
                for (int a = 0; a < neigh_cnt && !elim_pair_from_pivot; ++a) {
                    for (int b = a + 1; b < neigh_cnt && !elim_pair_from_pivot; ++b) {
                        if (odd_length_path_between_strict_nodes(
                                st,
                                strict_cells,
                                strict_count,
                                cell_to_node,
                                pivot_neighbors[a],
                                pivot_neighbors[b])) {
                            elim_pair_from_pivot = true;
                        }
                    }
                }

                if (elim_pair_from_pivot) {
                    const ApplyResult er = st.eliminate(pivot, pair);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return er;
                    }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_bivalue_oddagon = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
