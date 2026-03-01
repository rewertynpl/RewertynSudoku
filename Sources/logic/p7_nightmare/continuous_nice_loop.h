// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: continuous_nice_loop.h (Level 7 - Nightmare)
// Description: Direct alternating strong/weak loop search with discontinuity
// inference (strong-strong => true, weak-weak => false).
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool cnl_is_strong_neighbor(const shared::ExactPatternScratchpad& sp, int u, int v) {
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

inline int cnl_collect_neighbors(
    const shared::ExactPatternScratchpad& sp,
    int u,
    int edge_type, // 1=strong, 0=weak-only
    int* out) {
    int cnt = 0;
    if (edge_type == 1) {
        const int p0 = sp.dyn_strong_offsets[u];
        const int p1 = sp.dyn_strong_offsets[u + 1];
        for (int p = p0; p < p1; ++p) {
            out[cnt++] = sp.dyn_strong_adj[p];
        }
        return cnt;
    }

    const int p0 = sp.dyn_weak_offsets[u];
    const int p1 = sp.dyn_weak_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        const int v = sp.dyn_weak_adj[p];
        if (cnl_is_strong_neighbor(sp, u, v)) continue;
        out[cnt++] = v;
    }
    return cnt;
}

inline ApplyResult apply_continuous_nice_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int n = st.topo->n;
    bool any_progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    int* const vis_even = sp.visited;    // visited by (node, parity 0)
    int* const vis_odd = sp.bfs_depth;   // visited by (node, parity 1)
    int* const queue = sp.bfs_queue;     // state = node * 2 + last_edge_type
    int* const depth = sp.bfs_parent;    // state depth in edges
    int neighbors[256]{};

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 3 || sp.dyn_strong_edge_count == 0) continue;

        const int max_depth = std::clamp(8 + st.board->empty_cells / std::max(1, n), 10, 22);

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            const int start_cell = sp.dyn_node_to_cell[start];
            if (st.board->values[start_cell] != 0) continue;

            for (int first_type = 0; first_type <= 1; ++first_type) {
                // first_type: 1=strong, 0=weak-only
                std::fill_n(vis_even, sp.dyn_node_count, 0);
                std::fill_n(vis_odd, sp.dyn_node_count, 0);

                int qh = 0;
                int qt = 0;

                const int first_cnt = cnl_collect_neighbors(sp, start, first_type, neighbors);
                for (int i = 0; i < first_cnt; ++i) {
                    const int v = neighbors[i];
                    const int state = (v << 1) | first_type; // last edge type used
                    queue[qt] = state;
                    depth[qt] = 1;
                    ++qt;
                    if (first_type == 0) vis_even[v] = 1;
                    else vis_odd[v] = 1;
                }

                bool inferred = false;
                while (qh < qt && !inferred) {
                    const int state = queue[qh];
                    const int dep = depth[qh];
                    ++qh;

                    const int u = (state >> 1);
                    const int last_type = (state & 1);
                    const int next_type = 1 - last_type;
                    if (dep >= max_depth) continue;

                    const int next_cnt = cnl_collect_neighbors(sp, u, next_type, neighbors);
                    for (int i = 0; i < next_cnt; ++i) {
                        const int v = neighbors[i];
                        if (v == start) {
                            // Discontinuous loop at start:
                            // same type at both ends of start => inference on start candidate.
                            if (next_type == first_type && dep >= 2) {
                                if (first_type == 1) {
                                    if (!st.place(start_cell, d)) {
                                        s.elapsed_ns += get_current_time_ns() - t0;
                                        return ApplyResult::Contradiction;
                                    }
                                } else {
                                    const ApplyResult er = st.eliminate(start_cell, bit);
                                    if (er == ApplyResult::Contradiction) {
                                        s.elapsed_ns += get_current_time_ns() - t0;
                                        return er;
                                    }
                                    if (er == ApplyResult::NoProgress) {
                                        // no inference from this loop
                                        continue;
                                    }
                                }
                                any_progress = true;
                                inferred = true;
                                break;
                            }
                            continue;
                        }

                        int* vis = (next_type == 0) ? vis_even : vis_odd;
                        if (vis[v] != 0) continue;
                        vis[v] = 1;
                        queue[qt] = (v << 1) | next_type;
                        depth[qt] = dep + 1;
                        ++qt;
                        if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
                    }
                }
            }
        }
    }

    if (any_progress) {
        ++s.hit_count;
        r.used_continuous_nice_loop = true;
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare

