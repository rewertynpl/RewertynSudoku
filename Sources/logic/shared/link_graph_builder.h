// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: link_graph_builder.h
// Opis: System wyznaczania i utrzymywania sieci powiązań pomiędzy
//       komórkami w relacjach typu "Strong" oraz "Weak" per konkretna cyfra.
//       Kluczowe dla metod X-Cycles i Forcing Chains.
// ============================================================================

#pragma once

#include <cstdint>

#include "exact_pattern_scratchpad.h"
#include "../../core/candidate_state.h"

namespace sudoku_hpc::logic::shared {

inline bool dyn_has_strong_edge(const ExactPatternScratchpad& sp, int u, int v) {
    for (int i = 0; i < sp.dyn_strong_edge_count; ++i) {
        const int a = sp.dyn_strong_edge_u[i];
        const int b = sp.dyn_strong_edge_v[i];
        if ((a == u && b == v) || (a == v && b == u)) return true;
    }
    return false;
}

inline void dyn_add_strong_edge(ExactPatternScratchpad& sp, int u, int v) {
    if (u == v) return;
    if (dyn_has_strong_edge(sp, u, v)) return;
    if (sp.dyn_strong_edge_count >= (ExactPatternScratchpad::MAX_HOUSES * 2)) return;
    
    sp.dyn_strong_edge_u[sp.dyn_strong_edge_count] = u;
    sp.dyn_strong_edge_v[sp.dyn_strong_edge_count] = v;
    ++sp.dyn_strong_edge_count;
}

inline void dyn_add_weak_edge(ExactPatternScratchpad& sp, int u, int v) {
    if (u == v) return;
    // Słabe powiązania uzupełniają silne, ale silne jest już automatycznie "słabym".
    if (dyn_has_strong_edge(sp, u, v)) return;
    if (sp.dyn_weak_edge_count >= ExactPatternScratchpad::MAX_LINK_EDGES) return;
    
    sp.dyn_weak_edge_u[sp.dyn_weak_edge_count] = u;
    sp.dyn_weak_edge_v[sp.dyn_weak_edge_count] = v;
    ++sp.dyn_weak_edge_count;
}

// Konstruuje cały graf wzajemnych oddziaływań dla konkretnej cyfry w jednym przebiegu.
inline bool build_grouped_link_graph_for_digit(const CandidateState& st, int digit, ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    const uint64_t bit = (1ULL << (digit - 1));
    sp.reset_dynamic_graph(nn);

    // Krok 1: Identyfikacja wszystkich węzłów (komórek), gdzie kandydat występuje
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
        if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
        
        if (sp.dyn_node_count >= nn) break;
        sp.dyn_cell_to_node[idx] = sp.dyn_node_count;
        sp.dyn_node_to_cell[sp.dyn_node_count] = idx;
        sp.dyn_digit_cells[sp.dyn_digit_cell_count++] = idx;
        ++sp.dyn_node_count;
    }
    if (sp.dyn_node_count < 2) return false;

    // Krok 2: Poszukiwanie "Strong Links" - występuje tylko w 2 pozycjach w rzędzie/kolumnie/boxie
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    for (int h = 0; h < house_count; ++h) {
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
        int a = -1;
        int b = -1;
        int cnt = 0;
        
        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
            
            if (cnt == 0) a = idx;
            else if (cnt == 1) b = idx;
            ++cnt;
            if (cnt > 2) break;
        }
        // Jeśli cyfra występuje dokładnie w 2 miejscach - to jest Powiązanie Silne
        if (cnt == 2 && a >= 0 && b >= 0) {
            const int na = sp.dyn_cell_to_node[a];
            const int nb = sp.dyn_cell_to_node[b];
            if (na >= 0 && nb >= 0) {
                dyn_add_strong_edge(sp, na, nb);
            }
        }
    }

    // Krok 3: Dodanie "Weak Links" dla pozostałych komórek widzących się wzajemnie
    for (int ni = 0; ni < sp.dyn_node_count; ++ni) {
        const int cell = sp.dyn_node_to_cell[ni];
        const int p0 = st.topo->peer_offsets[static_cast<size_t>(cell)];
        const int p1 = st.topo->peer_offsets[static_cast<size_t>(cell + 1)];
        
        for (int p = p0; p < p1; ++p) {
            const int peer = st.topo->peers_flat[static_cast<size_t>(p)];
            if (peer <= cell) continue; // Uniknięcie podwójnego skanowania tego samego rzędu
            
            const int nj = sp.dyn_cell_to_node[peer];
            if (nj < 0) continue;
            dyn_add_weak_edge(sp, ni, nj);
        }
    }
    
    if (sp.dyn_strong_edge_count == 0 && sp.dyn_weak_edge_count == 0) return false;

    // Krok 4: Transformacja z listy wskaźników na zoptymalizowaną strukturę CSR (Compressed Sparse Row) dla szybkiego BFS.
    sp.reset_dynamic_adjacency(sp.dyn_node_count);
    
    for (int e = 0; e < sp.dyn_strong_edge_count; ++e) {
        const int u = sp.dyn_strong_edge_u[e];
        const int v = sp.dyn_strong_edge_v[e];
        ++sp.dyn_strong_degree[u];
        ++sp.dyn_strong_degree[v];
        ++sp.dyn_weak_degree[u];
        ++sp.dyn_weak_degree[v];
    }
    for (int e = 0; e < sp.dyn_weak_edge_count; ++e) {
        const int u = sp.dyn_weak_edge_u[e];
        const int v = sp.dyn_weak_edge_v[e];
        ++sp.dyn_weak_degree[u];
        ++sp.dyn_weak_degree[v];
    }

    int strong_total = 0;
    int weak_total = 0;
    for (int i = 0; i < sp.dyn_node_count; ++i) {
        sp.dyn_strong_offsets[i] = strong_total;
        sp.dyn_weak_offsets[i] = weak_total;
        strong_total += sp.dyn_strong_degree[i];
        weak_total += sp.dyn_weak_degree[i];
    }
    sp.dyn_strong_offsets[sp.dyn_node_count] = strong_total;
    sp.dyn_weak_offsets[sp.dyn_node_count] = weak_total;
    
    if (strong_total > ExactPatternScratchpad::MAX_ADJ || weak_total > ExactPatternScratchpad::MAX_ADJ) {
        return false;
    }

    for (int i = 0; i < sp.dyn_node_count; ++i) {
        sp.dyn_strong_cursor[i] = sp.dyn_strong_offsets[i];
        sp.dyn_weak_cursor[i] = sp.dyn_weak_offsets[i];
    }

    for (int e = 0; e < sp.dyn_strong_edge_count; ++e) {
        const int u = sp.dyn_strong_edge_u[e];
        const int v = sp.dyn_strong_edge_v[e];
        sp.dyn_strong_adj[sp.dyn_strong_cursor[u]++] = v;
        sp.dyn_strong_adj[sp.dyn_strong_cursor[v]++] = u;
        sp.dyn_weak_adj[sp.dyn_weak_cursor[u]++] = v;
        sp.dyn_weak_adj[sp.dyn_weak_cursor[v]++] = u;
    }
    for (int e = 0; e < sp.dyn_weak_edge_count; ++e) {
        const int u = sp.dyn_weak_edge_u[e];
        const int v = sp.dyn_weak_edge_v[e];
        sp.dyn_weak_adj[sp.dyn_weak_cursor[u]++] = v;
        sp.dyn_weak_adj[sp.dyn_weak_cursor[v]++] = u;
    }
    return true;
}

} // namespace sudoku_hpc::logic::shared