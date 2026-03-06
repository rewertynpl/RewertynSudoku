// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: medusa_3d.h (Level 7 - Nightmare)
// Description: 3D Medusa-oriented chain coloring pass focused on boards with
// rich bivalue structure (zero-allocation).
// ============================================================================

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../../config/bit_utils.h"
#include "aic_grouped_aic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline int medusa_find_node_for_digit(
    const shared::ExactPatternScratchpad& sp,
    int base_node,
    uint64_t bit) {
    if (base_node < 0) return -1;
    if (sp.medusa_node_bit[base_node] == bit) return base_node;
    if (sp.medusa_node_bit[base_node + 1] == bit) return base_node + 1;
    return -1;
}

inline void medusa_add_edge(shared::ExactPatternScratchpad& sp, int u, int v) {
    if (u < 0 || v < 0 || u == v) return;
    for (int e = 0; e < sp.medusa_edge_count; ++e) {
        const int a = sp.medusa_edge_u[e];
        const int b = sp.medusa_edge_v[e];
        if ((a == u && b == v) || (a == v && b == u)) return;
    }
    if (sp.medusa_edge_count >= shared::ExactPatternScratchpad::MAX_MEDUSA_EDGES) return;
    sp.medusa_edge_u[sp.medusa_edge_count] = u;
    sp.medusa_edge_v[sp.medusa_edge_count] = v;
    ++sp.medusa_edge_count;
}

inline bool build_medusa_bivalue_graph(CandidateState& st, shared::ExactPatternScratchpad& sp, int& bivalue_count) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    sp.medusa_node_count = 0;
    sp.medusa_edge_count = 0;
    std::fill_n(sp.cell_to_node, nn, -1);
    bivalue_count = 0;

    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        uint64_t mask = st.cands[idx];
        if (std::popcount(mask) != 2) continue;
        if (sp.medusa_node_count + 1 >= shared::ExactPatternScratchpad::MAX_MEDUSA_NODES) break;

        const int base = sp.medusa_node_count;
        sp.cell_to_node[idx] = base;
        sp.medusa_node_cell[base] = idx;
        sp.medusa_node_bit[base] = config::bit_lsb(mask);
        mask &= ~sp.medusa_node_bit[base];
        sp.medusa_node_cell[base + 1] = idx;
        sp.medusa_node_bit[base + 1] = mask;
        sp.medusa_node_count += 2;
        ++bivalue_count;
        medusa_add_edge(sp, base, base + 1);
    }
    if (bivalue_count < 3 || bivalue_count > 256) return false;

    int house_nodes[64]{};
    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
        for (int h = 0; h < house_count; ++h) {
            const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
            const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
            int cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                const int base = sp.cell_to_node[idx];
                if (base < 0) continue;
                const int node = medusa_find_node_for_digit(sp, base, bit);
                if (node < 0) continue;
                if (cnt < 64) house_nodes[cnt++] = node;
                if (cnt > 2) break;
            }
            if (cnt == 2) medusa_add_edge(sp, house_nodes[0], house_nodes[1]);
        }
    }

    if (sp.medusa_edge_count == 0) return false;

    std::fill_n(sp.medusa_degree, sp.medusa_node_count, 0);
    for (int e = 0; e < sp.medusa_edge_count; ++e) {
        ++sp.medusa_degree[sp.medusa_edge_u[e]];
        ++sp.medusa_degree[sp.medusa_edge_v[e]];
    }

    int total = 0;
    for (int i = 0; i < sp.medusa_node_count; ++i) {
        sp.medusa_offsets[i] = total;
        total += sp.medusa_degree[i];
    }
    sp.medusa_offsets[sp.medusa_node_count] = total;
    if (total > shared::ExactPatternScratchpad::MAX_MEDUSA_ADJ) return false;
    for (int i = 0; i < sp.medusa_node_count; ++i) {
        sp.medusa_cursor[i] = sp.medusa_offsets[i];
        sp.medusa_color[i] = 0;
    }
    for (int e = 0; e < sp.medusa_edge_count; ++e) {
        const int u = sp.medusa_edge_u[e];
        const int v = sp.medusa_edge_v[e];
        sp.medusa_adj[sp.medusa_cursor[u]++] = v;
        sp.medusa_adj[sp.medusa_cursor[v]++] = u;
    }
    return true;
}

inline ApplyResult medusa_eliminate_color(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    int color) {
    for (int node = 0; node < sp.medusa_node_count; ++node) {
        if (sp.medusa_color[node] != color) continue;
        const ApplyResult er = st.eliminate(sp.medusa_node_cell[node], sp.medusa_node_bit[node]);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult medusa_component_pass(
    CandidateState& st,
    shared::ExactPatternScratchpad& sp,
    int start_node) {
    int qh = 0;
    int qt = 0;
    int comp_size = 0;
    sp.bfs_queue[qt++] = start_node;
    sp.medusa_color[start_node] = 1;

    while (qh < qt) {
        const int u = sp.bfs_queue[qh++];
        sp.bfs_parent[comp_size++] = u;
        const int p0 = sp.medusa_offsets[u];
        const int p1 = sp.medusa_offsets[u + 1];
        for (int p = p0; p < p1; ++p) {
            const int v = sp.medusa_adj[p];
            if (sp.medusa_color[v] == 0) {
                sp.medusa_color[v] = -sp.medusa_color[u];
                sp.bfs_queue[qt++] = v;
            }
        }
    }

    for (int i = 0; i < comp_size; ++i) {
        const int u = sp.bfs_parent[i];
        const int cell_u = sp.medusa_node_cell[u];
        for (int j = i + 1; j < comp_size; ++j) {
            const int v = sp.bfs_parent[j];
            if (sp.medusa_color[u] != sp.medusa_color[v]) continue;
            if (sp.medusa_node_bit[u] != sp.medusa_node_bit[v] &&
                cell_u == sp.medusa_node_cell[v]) {
                return medusa_eliminate_color(st, sp, sp.medusa_color[u]);
            }
            if (sp.medusa_node_bit[u] == sp.medusa_node_bit[v] &&
                st.is_peer(cell_u, sp.medusa_node_cell[v])) {
                return medusa_eliminate_color(st, sp, sp.medusa_color[u]);
            }
        }
    }

    const int nn = st.topo->nn;
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        uint64_t m = st.cands[idx];
        while (m != 0ULL) {
            const uint64_t bit = config::bit_lsb(m);
            m = config::bit_clear_lsb_u64(m);
            bool sees_pos = false;
            bool sees_neg = false;
            for (int i = 0; i < comp_size; ++i) {
                const int node = sp.bfs_parent[i];
                if (sp.medusa_node_bit[node] != bit) continue;
                const int src = sp.medusa_node_cell[node];
                if (src == idx || !st.is_peer(idx, src)) continue;
                sees_pos = sees_pos || (sp.medusa_color[node] > 0);
                sees_neg = sees_neg || (sp.medusa_color[node] < 0);
                if (sees_pos && sees_neg) {
                    return st.eliminate(idx, bit);
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_medusa_3d(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = get_current_time_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (n > 32 || st.board->empty_cells > (nn - 2 * n)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    auto& sp = shared::exact_pattern_scratchpad();
    int bivalue_count = 0;
    if (!build_medusa_bivalue_graph(st, sp, bivalue_count)) {
        s.elapsed_ns += get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    for (int node = 0; node < sp.medusa_node_count; ++node) {
        if (sp.medusa_color[node] != 0) continue;
        const ApplyResult ar = medusa_component_pass(st, sp, node);
        if (ar == ApplyResult::Contradiction) {
            s.elapsed_ns += get_current_time_ns() - t0;
            return ar;
        }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_medusa_3d = true;
            s.elapsed_ns += get_current_time_ns() - t0;
            return ar;
        }
    }

    s.elapsed_ns += get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
