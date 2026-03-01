// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: simple_coloring.h (Poziom 5 - Expert)
// Opis: Algorytm Simple Coloring korzystający z węzłów w powiązaniach silnych.
//       Rozwiązuje metodą przydzielania 2 kolorów z weryfikacją dwudzielności 
//       grafu i dedukcją (Rules: Color Trap & Color Wrap). Zero-allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"

namespace sudoku_hpc::logic::p5_expert {

inline ApplyResult apply_simple_coloring(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    bool any_progress = false;

    auto& sp = shared::exact_pattern_scratchpad();

    // Re-use tablic z exact_pattern_scratchpad jako zmienne kolorowania i kolejki
    int* color = sp.visited;        // Wielkość: MAX_NN (odwiedzono = pomalowano)
    int* queue = sp.bfs_queue;      // Wielkość: MAX_BFS
    int* comp_nodes = sp.chain_cell; // Wielkość: MAX_CHAIN
    
    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        
        // Zbudowanie grafu dla bieżącej cyfry. Moduł tworzy sp.dyn_strong_adj.
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        
        // Brak silnych powiązań -> brak możliwości użycia Simple Coloring
        if (sp.dyn_strong_edge_count == 0) continue;

        std::fill_n(color, sp.dyn_node_count, -1);

        for (int start_node = 0; start_node < sp.dyn_node_count; ++start_node) {
            // Pomijaj node'y bez powiązań silnych lub już pokolorowane
            if (sp.dyn_strong_degree[start_node] == 0) continue;
            if (color[start_node] != -1) continue;

            int qh = 0;
            int qt = 0;
            int comp_size = 0;
            
            bool conflict0 = false;
            bool conflict1 = false;

            color[start_node] = 0;
            queue[qt++] = start_node;
            
            // Faza 1: BFS - Pokolorowanie komponentu (Sieć silnych powiązań)
            while (qh < qt) {
                const int u = queue[qh++];
                if (comp_size < ExactPatternScratchpad::MAX_CHAIN) {
                    comp_nodes[comp_size++] = u;
                }
                
                const int my_color = color[u];
                const int opp_color = 1 - my_color;

                const int off_begin = sp.dyn_strong_offsets[u];
                const int off_end = sp.dyn_strong_offsets[u + 1];
                
                for (int e = off_begin; e < off_end; ++e) {
                    const int v = sp.dyn_strong_adj[e];
                    
                    if (color[v] == -1) {
                        color[v] = opp_color;
                        if (qt < ExactPatternScratchpad::MAX_BFS) {
                            queue[qt++] = v;
                        }
                    } else if (color[v] == my_color) {
                        // Color Trap: znaleziono dwa węzły tego samego koloru powiązane relacją silną.
                        // Graf przestaje być dwudzielny. Zatem 'my_color' na 100% wskazuje na fałszywą cyfrę.
                        if (my_color == 0) conflict0 = true;
                        else conflict1 = true;
                    }
                }
            }

            // Faza 2: Obsługa Color Trap (Eliminacja wewnętrzna w grupie)
            if (conflict0 || conflict1) {
                const int bad_color = conflict0 ? 0 : 1;
                
                for (int i = 0; i < comp_size; ++i) {
                    const int u = comp_nodes[i];
                    if (color[u] != bad_color) continue;
                    
                    const int cell = sp.dyn_node_to_cell[u];
                    const ApplyResult er = st.eliminate(cell, bit);
                    if (er == ApplyResult::Contradiction) { 
                        s.elapsed_ns += st.now_ns() - t0; 
                        return er; 
                    }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_simple_coloring = true;
                        any_progress = true;
                    }
                }
                // Jeśli usunęliśmy bad_color, to reszta planszy została zdestabilizowana,
                // więc kontynuacja analizy małej sieci może wywołać fałszywe redukcje.
                continue; 
            }

            // Faza 3: Obsługa Color Wrap (Rule 2 - Twice in a House)
            // Zbieramy komórki przypisane do kolorów 0 oraz 1
            int c0_count = 0;
            int c1_count = 0;
            int* const c0_cells = sp.bfs_depth;  // Używamy buforów BFS jako tempów
            int* const c1_cells = sp.bfs_parent;

            for (int i = 0; i < comp_size; ++i) {
                const int u = comp_nodes[i];
                if (color[u] == 0) c0_cells[c0_count++] = sp.dyn_node_to_cell[u];
                else c1_cells[c1_count++] = sp.dyn_node_to_cell[u];
            }

            // Szukamy niezwiązanych z naszą siecią komórek kandydujących, które widzą
            // przynajmniej jednego reprezentanta koloru 0 ORAZ koloru 1.
            // Skoro kolor 0 xor kolor 1 muszą stanowić rozwiązanie, to t-cell jest fałszywy.
            for (int i = 0; i < sp.dyn_digit_cell_count; ++i) {
                const int t_cell = sp.dyn_digit_cells[i];
                const int t_node = sp.dyn_cell_to_node[t_cell];
                
                // Jeśli ten node jest pomalowany w naszej bieżącej iteracji - omiń (zajęliśmy się nim wyżej)
                if (t_node >= 0 && color[t_node] != -1) continue;
                
                bool sees_0 = false;
                for (int k = 0; k < c0_count; ++k) {
                    if (st.is_peer(t_cell, c0_cells[k])) {
                        sees_0 = true;
                        break;
                    }
                }
                if (!sees_0) continue;

                bool sees_1 = false;
                for (int k = 0; k < c1_count; ++k) {
                    if (st.is_peer(t_cell, c1_cells[k])) {
                        sees_1 = true;
                        break;
                    }
                }
                if (!sees_1) continue;

                // Znaleziono komórkę spiętą (wraps). Posiada ona wspólną widoczność dla c0 i c1
                const ApplyResult er = st.eliminate(t_cell, bit);
                if (er == ApplyResult::Contradiction) { 
                    s.elapsed_ns += st.now_ns() - t0; 
                    return er; 
                }
                if (er == ApplyResult::Progress) {
                    ++s.hit_count;
                    r.used_simple_coloring = true;
                    any_progress = true;
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return any_progress ? ApplyResult::Progress : ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p5_expert