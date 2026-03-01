// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: chains_basic.h (Poziom 6 - Diabolical)
// Opis: Algorytmy oparte na łańcuchach logicznych: X-Chain i XY-Chain.
//       Wszystkie analizy działają na spłaszczonych drzewach (BFS) w
//       buforze exact_pattern_scratchpad by uniknąć the memory overhead.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/link_graph_builder.h"

namespace sudoku_hpc::logic::p6_diabolical {

// ============================================================================
// X-Chain (Single Digit Alternating Inference Chain)
// Szuka łańcuchów Strong-Weak-Strong... gdzie dwa końce prowadzą do eliminacji 
// danej cyfry w punkcie "zbiegu" wzroku obu krańców łańcucha.
// ============================================================================
inline ApplyResult apply_x_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    bool progress = false;
    
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        
        // Zbieramy komórki dla tej konkretnej cyfry w tablicy als_cells (jako temp)
        int digit_cell_count = 0;
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) != 0ULL) {
                sp.als_cells[digit_cell_count++] = idx;
            }
        }
        
        // Łańcuch wymaga przynajmniej 4 elementów (Start -> W1 -> S1 -> End)
        if (digit_cell_count < 4) continue;

        // Tworzymy graf silnych/słabych powiązań tylko dla tej cyfry 'd'
        if (!shared::build_grouped_link_graph_for_digit(st, d, sp)) continue;
        if (sp.dyn_node_count < 4 || sp.dyn_strong_edge_count == 0) continue;

        // BFS poszukujący naprzemiennych łańcuchów
        for (int start = 0; start < sp.dyn_node_count; ++start) {
            
            std::fill_n(sp.visited, sp.dyn_node_count, -1);
            int qh = 0;
            int qt = 0;
            
            // Inicjalizujemy węzeł startowy
            sp.bfs_queue[qt++] = start;
            sp.visited[start] = 0; // Odległość 0

            // Rozwijanie łańcucha wszerz (BFS) - przeplatane z wykorzystaniem sp.visited jako znacznika głębokości
            while (qh < qt) {
                const int u = sp.bfs_queue[qh++];
                const int du = sp.visited[u];
                
                // Silne/Słabe krawędzie mamy w tablicach dyn_strong_adj i dyn_weak_adj, 
                // ale X-Chain zadowoli się po prostu silnymi powiązaniami w węzłach nieparzystych.
                // Aby ułatwić i przyspieszyć, idziemy wprost po wybudowanym mapowaniu.
                
                // UWAGA: X-Chain wykorzystuje przeplatanie (Alternating Inference). Zatem:
                // Krok od 0 (Start) wymaga powiązania SILNEGO (->1).
                // Krok z 1 może być powiązaniem SŁABYM lub SILNYM (->2).
                // Generalnie dla prostego X-Chain iterujemy poprzez silne łącza traktując je jako nośniki.
                const int off_begin = sp.dyn_strong_offsets[u];
                const int off_end = sp.dyn_strong_offsets[u + 1];
                
                for (int ei = off_begin; ei < off_end; ++ei) {
                    const int v = sp.dyn_strong_adj[ei];
                    if (sp.visited[v] != -1) continue;
                    
                    sp.visited[v] = du + 1;
                    if (qt < ExactPatternScratchpad::MAX_BFS) {
                        sp.bfs_queue[qt++] = v;
                    }
                }
            }

            const int start_cell = sp.dyn_node_to_cell[start];
            
            // Szukamy końcówki łańcucha (End), który dzieli nieparzystą liczbę skoków i widzi Start
            for (int end = 0; end < sp.dyn_node_count; ++end) {
                const int de = sp.visited[end];
                // Wymagamy minimum 3 skoków i długości nieparzystej dla klasycznego powiązania (S-W-S)
                // W tej mocno zoptymalizowanej wersji przeszukujemy po silnych komponentach 
                // więc skok o 3 oznacza (Start -> Node1 -> Node2 -> End).
                if (de < 3 || (de & 1) == 0) continue;
                
                const int end_cell = sp.dyn_node_to_cell[end];
                
                // Start i End muszą się widzieć (tzw. "X-Chain loop" albo zbieżność) lub my
                // uderzamy w komórkę, która widzi oba.
                if (!st.is_peer(start_cell, end_cell)) continue;
                
                // Znajdujemy cel `idx`, z którego można usunąć naszą cyfrę (bit)
                for (int di = 0; di < digit_cell_count; ++di) {
                    const int idx = sp.als_cells[di];
                    if (idx == start_cell || idx == end_cell) continue;
                    
                    if (!st.is_peer(idx, start_cell) || !st.is_peer(idx, end_cell)) continue;
                    
                    const ApplyResult er = st.eliminate(idx, bit);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_x_chain = true;
                        progress = true;
                    }
                }
            }
        }
    }

    if (progress) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}


// ============================================================================
// XY-Chain 
// Łańcuch oparty o powiązania węzłów typu "Bivalue" (2 kandydatów).
// Szukamy ścieżki: xy -> yz -> zw -> wx. 
// Eliminujemy cyfrę wspólną dla końców w miejscu ich skrzyżowania.
// ============================================================================
inline ApplyResult apply_xy_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int nn = st.topo->nn;
    
    // Zapobiegamy eksplozji przeszukiwań (szukamy do ustalonej głębokości)
    const int max_depth = (st.topo->n <= 16) ? 8 : 6;
    
    auto& sp = shared::exact_pattern_scratchpad();
    int bivalue_count = 0;
    
    // Zbieramy komórki dwukandydatowe
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 2) {
            sp.als_cells[bivalue_count++] = idx;
        }
    }
    
    if (bivalue_count < 3) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    // Prosta funkcja pomocnicza: weryfikuje czy węzeł jest już w bieżącym łańcuchu
    auto path_contains_cell = [&](int node_idx, int cell) -> bool {
        int cur = node_idx;
        while (cur >= 0) {
            if (sp.chain_cell[cur] == cell) return true;
            cur = sp.chain_parent[cur];
        }
        return false;
    };

    bool any_progress = false;

    for (int bi = 0; bi < bivalue_count; ++bi) {
        const int start = sp.als_cells[bi];
        const uint64_t start_mask = st.cands[start];
        
        // Rozgałęzienie startowe - w którą stronę uderzamy po łańcuchu?
        uint64_t wz = start_mask;
        while (wz != 0ULL) {
            const uint64_t zbit = config::bit_lsb(wz);
            wz = config::bit_clear_lsb_u64(wz);
            
            if ((start_mask ^ zbit) == 0ULL) continue;

            sp.chain_count = 1;
            sp.chain_cell[0] = start;
            sp.chain_enter_bit[0] = zbit; // Cyfra użyta do wyjścia z pierwszej komórki
            sp.chain_parent[0] = -1;
            sp.chain_depth[0] = 0;

            // Przeszukiwanie łańcucha (zmodyfikowany BFS dla tras bivalue)
            for (int ni = 0; ni < sp.chain_count; ++ni) {
                const int cur_cell = sp.chain_cell[ni];
                const uint64_t cur_enter = sp.chain_enter_bit[ni];
                const uint64_t cur_mask = st.cands[cur_cell];
                
                // Awaryjne weryfikacje (np. z powodu symulacji wyższego rzędu)
                if (std::popcount(cur_mask) != 2 || (cur_mask & cur_enter) == 0ULL) continue;
                
                const uint64_t exit_bit = cur_mask ^ cur_enter;
                if (exit_bit == 0ULL) continue;
                if (sp.chain_depth[ni] >= max_depth) continue;

                // Sprawdzamy sąsiadów bieżącego węzła
                const int p0 = st.topo->peer_offsets[cur_cell];
                const int p1 = st.topo->peer_offsets[cur_cell + 1];
                for (int p = p0; p < p1; ++p) {
                    const int nxt = st.topo->peers_flat[p];
                    if (st.board->values[nxt] != 0) continue;
                    
                    const uint64_t nxt_mask = st.cands[nxt];
                    if (std::popcount(nxt_mask) != 2) continue;
                    
                    // Kolejne "ogniwo" musi akceptować wyjście naszego węzła
                    if ((nxt_mask & exit_bit) == 0ULL) continue;
                    if (path_contains_cell(ni, nxt)) continue; // Zapobiega pętlom wstecznym

                    const uint64_t nxt_other = nxt_mask ^ exit_bit;
                    if (nxt_other == 0ULL) continue;

                    const int next_depth = static_cast<int>(sp.chain_depth[ni]) + 1;
                    
                    // Czy łańcuch zamknął pełną sieć logiki i "wyszliśmy" tą samą cyfrą z którą weszliśmy do startu?
                    if (next_depth >= 2 && nxt_other == zbit) {
                        for (int t = 0; t < nn; ++t) {
                            if (t == start || t == nxt) continue;
                            if (st.board->values[t] != 0) continue;
                            if ((st.cands[t] & zbit) == 0ULL) continue;
                            
                            // Cel ataku musi widzieć OBA krańce łańcucha
                            if (!st.is_peer(t, start) || !st.is_peer(t, nxt)) continue;
                            
                            const ApplyResult er = st.eliminate(t, zbit);
                            if (er == ApplyResult::Contradiction) { 
                                s.elapsed_ns += st.now_ns() - t0; 
                                return er; 
                            }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_xy_chain = true;
                                any_progress = true;
                            }
                        }
                    }

                    // Jeśli mamy wciąż miejsce, dokładamy łańcuch do dalszej propagacji
                    if (sp.chain_count >= ExactPatternScratchpad::MAX_CHAIN) continue;
                    sp.chain_cell[sp.chain_count] = nxt;
                    sp.chain_enter_bit[sp.chain_count] = exit_bit;
                    sp.chain_parent[sp.chain_count] = ni;
                    sp.chain_depth[sp.chain_count] = static_cast<uint8_t>(next_depth);
                    ++sp.chain_count;
                }
            }
        }
    }

    if (any_progress) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical