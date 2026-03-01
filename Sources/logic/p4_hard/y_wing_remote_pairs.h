// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: y_wing_remote_pairs.h (Poziom 4)
// Opis: Algorytmy bazujące na dwukandydatowych komórkach (bivalue). 
//       - Y-Wing szuka pivotu z dwoma "skrzydłami".
//       - Remote Pairs przechodzi BFS-em po wszystkich spiętych parach.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p4_hard {

inline ApplyResult apply_y_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    bool progress = false;

    // Iteracja po potencjalnych "Pivotach"
    for (int pivot = 0; pivot < st.topo->nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;
        
        const uint64_t mp = st.cands[pivot];
        if (std::popcount(mp) != 2) continue; // Pivot musi być bivalue np. AB

        // Iteracja po "widocznych" rówieśnikach pivota dla pierwszego skrzydła
        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        
        for (int i = p0; i < p1; ++i) {
            const int a = st.topo->peers_flat[i];
            if (st.board->values[a] != 0) continue;
            
            const uint64_t ma = st.cands[a];
            if (std::popcount(ma) != 2) continue;
            
            const uint64_t shared_a = ma & mp;
            if (std::popcount(shared_a) != 1) continue; // Posiadają dokładnie 1 wspólną cyfrę (np. A)
            
            const uint64_t z = ma & ~mp;
            if (std::popcount(z) != 1) continue; // Pozostała cyfra z skrzydła to nasze 'Z' (Z -> eliminacja)

            // Szukamy drugiego skrzydła na pozostałych widocznych węzłach pivota
            for (int j = i + 1; j < p1; ++j) {
                const int b = st.topo->peers_flat[j];
                if (st.board->values[b] != 0) continue;
                
                const uint64_t mb = st.cands[b];
                if (std::popcount(mb) != 2) continue;
                
                const uint64_t shared_b = mb & mp;
                // Drugie skrzydło musi dzielić z pivotem pozostałą cyfrę (np. B)
                if (std::popcount(shared_b) != 1 || shared_b == shared_a) continue;
                
                const uint64_t z2 = mb & ~mp;
                // Drugie skrzydło musi wycelować w to samo z (np. BZ)
                if (z2 != z || std::popcount(z2) != 1) continue;

                // Eliminacja 'Z' u wszystkich co widzą oba skrzydła A i B
                const int ap0 = st.topo->peer_offsets[a];
                const int ap1 = st.topo->peer_offsets[a + 1];
                for (int p = ap0; p < ap1; ++p) {
                    const int t = st.topo->peers_flat[p];
                    if (t == pivot || t == a || t == b) continue;
                    if (!st.is_peer(t, b)) continue; // t widzi już A przez zewnętrzną pętlę, musi też widzieć B
                    
                    const ApplyResult er = st.eliminate(t, z);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_y_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_remote_pairs(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int nn = st.topo->nn;
    bool progress = false;
    
    auto& sp = shared::exact_pattern_scratchpad();

    // Wykorzystujemy aliasy pamięci z ExactPatternScratchpad jako tablice lokalne dla BFS
    // Zapobiega to alokacji vectora i fałszywemu dzieleniu cache.
    int* component = sp.cell_to_node;      // wielkosc NN
    int* parity = sp.node_to_cell;         // wielkosc NN, re-used jako uint8_t parity indicator
    int* in_component = sp.node_degree;    // wielkosc NN
    int* seen_parity0 = sp.adj_cursor;     // wielkosc NN
    int* seen_parity1 = sp.visited;        // wielkosc NN

    std::fill_n(component, nn, -1);
    
    // Szukamy dostępnych masek typu Pair (np. komórki zawierające wył. "1 i 2")
    // Ponieważ potrzebujemy unikalnych wartości pair_mask, zapiszemy je tymczasowo na końcu bufora
    int pair_mask_count = 0;
    uint64_t* pair_masks = reinterpret_cast<uint64_t*>(sp.adj_flat);
    
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        const uint64_t m = st.cands[idx];
        if (std::popcount(m) == 2) {
            pair_masks[pair_mask_count++] = m;
        }
    }
    
    // Unikalne maski do zbadania BFS-em
    std::sort(pair_masks, pair_masks + pair_mask_count);
    uint64_t* new_end = std::unique(pair_masks, pair_masks + pair_mask_count);
    pair_mask_count = static_cast<int>(new_end - pair_masks);

    for (int p_idx = 0; p_idx < pair_mask_count; ++p_idx) {
        const uint64_t current_pair_mask = pair_masks[p_idx];
        
        std::fill_n(component, nn, -1);
        std::fill_n(parity, nn, 0);
        int comp_id = 0;

        for (int start = 0; start < nn; ++start) {
            if (st.board->values[start] != 0) continue;
            if (st.cands[start] != current_pair_mask) continue;
            if (component[start] != -1) continue; // Już odwiedzone w innej sieci
            
            int qh = 0;
            int qt = 0;
            
            sp.bfs_queue[qt++] = start;
            component[start] = comp_id;
            parity[start] = 0;

            // Przeszukiwanie wszerz sieci powiązanych par
            while (qh < qt) {
                const int cur = sp.bfs_queue[qh++];
                const int p_off0 = st.topo->peer_offsets[cur];
                const int p_off1 = st.topo->peer_offsets[cur + 1];
                
                for (int p = p_off0; p < p_off1; ++p) {
                    const int nxt = st.topo->peers_flat[p];
                    if (st.board->values[nxt] != 0) continue;
                    if (st.cands[nxt] != current_pair_mask) continue;
                    
                    if (component[nxt] == -1) {
                        component[nxt] = comp_id;
                        parity[nxt] = parity[cur] ^ 1;
                        if (qt < ExactPatternScratchpad::MAX_BFS) {
                            sp.bfs_queue[qt++] = nxt;
                        }
                    }
                }
            }
            ++comp_id;
        }

        // Faza ewaluacji eliminacji z poszczególnych komponentów (niezależnych sieci)
        for (int cid = 0; cid < comp_id; ++cid) {
            int node_count_in_comp = 0;
            for (int idx = 0; idx < nn; ++idx) {
                if (component[idx] == cid) {
                    sp.als_cells[node_count_in_comp++] = idx; // używamy wolnego bufora ALS
                }
            }
            
            // Remote Pairs potrzebuje co najmniej 4 węzłów w cyklu, by mogło rzucać cień
            if (node_count_in_comp < 4) continue;

            std::fill_n(in_component, nn, 0);
            std::fill_n(seen_parity0, nn, 0);
            std::fill_n(seen_parity1, nn, 0);
            
            for (int i = 0; i < node_count_in_comp; ++i) {
                const int idx = sp.als_cells[i];
                in_component[idx] = 1;
                
                const int p_off0 = st.topo->peer_offsets[idx];
                const int p_off1 = st.topo->peer_offsets[idx + 1];
                
                int* const target_seen = (parity[idx] == 0) ? seen_parity0 : seen_parity1;
                for (int p = p_off0; p < p_off1; ++p) {
                    target_seen[st.topo->peers_flat[p]] = 1;
                }
            }

            // Eliminacja tam, gdzie cel widzi na raz node o obu parytetach z naszej sieci par.
            for (int t = 0; t < nn; ++t) {
                if (in_component[t] != 0) continue;
                if (st.board->values[t] != 0) continue;
                if (seen_parity0[t] == 0 || seen_parity1[t] == 0) continue;
                
                const ApplyResult er = st.eliminate(t, current_pair_mask);
                if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                progress = progress || (er == ApplyResult::Progress);
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_remote_pairs = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p4_hard