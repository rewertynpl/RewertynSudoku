// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: bug_plus_one.h (Poziom 5 - Expert)
// Opis: Wykrywa Bivalue Universal Grave + 1 (BUG+1). 
//       Jeżeli wszystkie nieodkryte komórki prócz jednej mają 2 kandydatów 
//       (i ta jedna posiada ich 3), oznacza to śmiertelny wzorzec, w którym
//       tylko jedna dedukcja "ratuje" planszę przed wielokrotnym rozwiązaniem.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p5_expert {

inline ApplyResult apply_bug_plus_one(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Strategie oparte o Unikalność i Prostokąty wymagają siatek posiadających konwencjonalne bloki 
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    int bug_idx = -1;
    int tri_count = 0;
    
    // Skan wszystkich pustych komórek, by sprawdzić warunek BUG (wszystko bivalue + 1 trivalue)
    for (int idx = 0; idx < st.topo->nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        
        const int cnt = std::popcount(st.cands[idx]);
        if (cnt < 2 || cnt > 3) {
            // Natychmiastowe wyjście: Mamy komórkę o popcount >= 4 lub <= 1
            s.elapsed_ns += st.now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        
        if (cnt == 3) {
            bug_idx = idx;
            ++tri_count;
            if (tri_count > 1) {
                // BUG wymaga dokładnie JEDNEJ komórki tri-value (pozostałe muszą być bivalue)
                s.elapsed_ns += st.now_ns() - t0;
                return ApplyResult::NoProgress;
            }
        }
    }
    
    // BUG+1 nie wystąpił
    if (tri_count != 1 || bug_idx < 0) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    // Znaleziono siatkę w stanie BUG+1. Analizujemy węzeł (Pivot), w którym są 3 cyfry.
    const int bug_row = st.topo->cell_row[bug_idx];
    const int bug_col = st.topo->cell_col[bug_idx];
    const int bug_box = st.topo->cell_box[bug_idx];
    const uint64_t m = st.cands[bug_idx];
    
    // Weryfikujemy każdą z trzech cyfr tego węzła. Tylko jedna z nich pozwala
    // na złamanie pętli BUG'a (taka, która występuje nieparzyście w rejonach p-ta)
    uint64_t w = m;
    while (w != 0ULL) {
        const uint64_t bit = config::bit_ctz_u64(w);
        w = config::bit_clear_lsb_u64(w);
        
        const int d = static_cast<int>(bit) + 1;
        int cnt_row = 0;
        int cnt_col = 0;
        int cnt_box = 0;

        for (int c = 0; c < st.topo->n; ++c) {
            const int idx = bug_row * st.topo->n + c;
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & (1ULL << bit)) != 0ULL) ++cnt_row;
        }
        for (int rr = 0; rr < st.topo->n; ++rr) {
            const int idx = rr * st.topo->n + bug_col;
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & (1ULL << bit)) != 0ULL) ++cnt_col;
        }
        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.topo->cell_box[idx] != bug_box) continue;
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & (1ULL << bit)) != 0ULL) ++cnt_box;
        }
        
        // Ratunkiem dla BUG jest ta cyfra, która zaburza parzystość wystąpień we 
        // wszystkich trzech domkach (rzędzie, kolumnie i bloku)
        if ((cnt_row % 2 == 1) && (cnt_col % 2 == 1) && (cnt_box % 2 == 1)) {
            if (!st.place(bug_idx, d)) { 
                s.elapsed_ns += st.now_ns() - t0; 
                return ApplyResult::Contradiction; 
            }
            
            ++s.hit_count;
            ++s.placements;
            ++r.steps;
            r.used_bug_plus_one = true;
            
            s.elapsed_ns += st.now_ns() - t0;
            return ApplyResult::Progress;
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p5_expert