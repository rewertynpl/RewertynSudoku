// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: naked_hidden_single.h (Poziom 1)
// Opis: Rozwiązywanie na zasadzie gołych (Naked) lub ukrytych (Hidden) Singli.
//       Absolutnie pierwsza linia obrony przy certyfikacji. Zero-allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p1_easy {

inline ApplyResult apply_naked_single(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Szukamy komórki, która ma dostępnego tylko jednego kandydata
    for (int idx = 0; idx < st.topo->nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        
        const uint64_t m = st.cands[idx];
        if (m == 0ULL) { 
            s.elapsed_ns += st.now_ns() - t0; 
            return ApplyResult::Contradiction; 
        }
        
        const int d = config::single_digit(m);
        if (d == 0) continue; // Posiada wielu kandydatów
        
        // Czas na postawienie jedynego dostępnego kandydata
        if (!st.place(idx, d)) { 
            s.elapsed_ns += st.now_ns() - t0; 
            return ApplyResult::Contradiction; 
        }
        
        ++s.hit_count; 
        ++s.placements; 
        ++r.steps; 
        r.used_naked_single = true;
        
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_hidden_single(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    
    // Szukamy w każdym rzędzie, kolumnie i bloku, czy jakakolwiek cyfra 
    // występuje tylko w jednej dostępnej komórce dla danego domku
    for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
        const int p0 = st.topo->house_offsets[h];
        const int p1 = st.topo->house_offsets[h + 1];
        
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int pos = -1;
            int cnt = 0;
            
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                
                pos = idx; 
                ++cnt; 
                if (cnt > 1) break; // Szybkie zakończenie poszukiwań - występuje min 2 razy
            }
            
            if (cnt != 1) continue;
            
            // Postawienie cyfry
            if (!st.place(pos, d)) { 
                s.elapsed_ns += st.now_ns() - t0; 
                return ApplyResult::Contradiction; 
            }
            
            ++s.hit_count; 
            ++s.placements; 
            ++r.steps; 
            r.used_hidden_single = true;
            
            s.elapsed_ns += st.now_ns() - t0;
            return ApplyResult::Progress;
        }
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p1_easy