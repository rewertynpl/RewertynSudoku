// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: aic_grouped_aic.h (Level 7 - Nightmare)
// Description: Direct Alternating Inference Chains (AIC) and Grouped AIC
// on strong/weak candidate graphs per digit, zero-allocation.
// ============================================================================

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline uint64_t get_current_time_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline bool aic_is_strong_neighbor(const shared::ExactPatternScratchpad& sp, int u, int v) {
    const int p0 = sp.dyn_strong_offsets[u];
    const int p1 = sp.dyn_strong_offsets[u + 1];
    for (int p = p0; p < p1; ++p) {
        if (sp.dyn_strong_adj[p] == v) return true;
    }
    return false;
}

inline int aic_collect_neighbors(
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
        if (aic_is_strong_neighbor(sp, u, v)) continue;
        out[cnt++] = v;
    }
    return cnt;
}

inline ApplyResult aic_eliminate_common_peers(
    CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    uint64_t bit,
    int a_cell,
    int b_cell) {
    for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
        const int idx = sp.dyn_digit_cells[i];
        if (idx == a_cell || idx == b_cell) continue;
        if (!st.is_peer(idx, a_cell) || !st.is_peer(idx, b_cell)) continue;
        const ApplyResult er = st.eliminate(idx, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult alternating_chain_core(
    CandidateState& st,
    int depth_cap,
    bool allow_weak_start,
    bool& used_flag) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int n = st.topo->n;
    int neighbors[256]{};

    int* const vis_even = sp.visited;
    int* const vis_odd = sp.bfs_depth;
    int* const queue_state = sp.bfs_queue;
    int* const queue_depth = sp.bfs_parent;

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 4 || sp.dyn_strong_edge_count == 0) continue;

        for (int start = 0; start < sp.dyn_node_count; ++start) {
            const int start_cell = sp.dyn_node_to_cell[start];
            if (st.board->values[start_cell] != 0) continue;

            for (int first_type = 1; first_type >= 0; --first_type) {
                if (first_type == 0 && !allow_weak_start) continue;

                std::fill_n(vis_even, sp.dyn_node_count, 0);
                std::fill_n(vis_odd, sp.dyn_node_count, 0);

                int qh = 0;
                int qt = 0;

                const int first_cnt = aic_collect_neighbors(sp, start, first_type, neighbors);
                for (int i = 0; i < first_cnt; ++i) {
                    if (qt >= shared::ExactPatternScratchpad::MAX_BFS) break;
                    const int v = neighbors[i];
                    queue_state[qt] = (v << 1) | first_type;
                    queue_depth[qt] = 1;
                    ++qt;
                    if (first_type == 0) vis_even[v] = 1;
                    else vis_odd[v] = 1;
                }

                while (qh < qt) {
                    const int state = queue_state[qh];
                    const int dep = queue_depth[qh];
                    ++qh;

                    const int u = (state >> 1);
                    const int last_type = (state & 1);
                    const int next_type = 1 - last_type;
                    if (dep >= depth_cap) continue;

                    const int next_cnt = aic_collect_neighbors(sp, u, next_type, neighbors);
                    for (int i = 0; i < next_cnt; ++i) {
                        const int v = neighbors[i];
                        const int nd = dep + 1;

                        if (v == start) {
                            // Start-closure inferences are handled in dedicated
                            // Nice Loop strategy to keep AIC core conservative.
                            continue;
                        }

                        // Odd-length alternating chain endpoints seeing each other
                        // force elimination in the intersection of their peers.
                        if (first_type == 1 && nd >= 3 && (nd & 1) == 1) {
                            const int end_cell = sp.dyn_node_to_cell[v];
                            if (st.is_peer(start_cell, end_cell)) {
                                const ApplyResult er = aic_eliminate_common_peers(st, sp, bit, start_cell, end_cell);
                                if (er == ApplyResult::Contradiction) return er;
                                if (er == ApplyResult::Progress) {
                                    used_flag = true;
                                    return er;
                                }
                            }
                        }

                        int* const vis = (next_type == 0) ? vis_even : vis_odd;
                        if (vis[v] != 0) continue;
                        vis[v] = 1;
                        if (qt >= shared::ExactPatternScratchpad::MAX_BFS) continue;
                        queue_state[qt] = (v << 1) | next_type;
                        queue_depth[qt] = nd;
                        ++qt;
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

// Lightweight implication driver used across P7/P8.
inline ApplyResult bounded_implication_core(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    int max_iters,
    bool& used_flag) {
    (void)r;
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int depth_cap = std::clamp(max_iters, 6, 28);
    const bool allow_weak_start = false;

    const ApplyResult ar = alternating_chain_core(st, depth_cap, allow_weak_start, used_flag);
    s.elapsed_ns += get_current_time_ns() - t0;
    return ar;
}

// Backward-compatible alias for older modules.
inline ApplyResult bounded_implication_proxy(
    CandidateState& st,
    StrategyStats& s,
    GenericLogicCertifyResult& r,
    int max_iters,
    bool& used_flag) {
    return bounded_implication_core(st, s, r, max_iters, used_flag);
}

inline ApplyResult apply_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    bool used = false;
    const ApplyResult res = bounded_implication_core(st, s, r, 8, used);
    if (res == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_aic = true;
    }
    return res;
}

inline ApplyResult apply_grouped_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    bool used = false;
    const ApplyResult res = bounded_implication_core(st, s, r, 14, used);
    if (res == ApplyResult::Progress && used) {
        ++s.hit_count;
        r.used_grouped_aic = true;
    }
    return res;
}

} // namespace sudoku_hpc::logic::p7_nightmare
