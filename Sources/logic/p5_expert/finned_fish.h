// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: finned_fish.h (Poziom 5 - Expert)
// Opis: Implementacja Finned X-Wing oraz Sashimi X-Wing.
//       Strategie te szukają struktury X-Wing, w której jeden z węzłów 
//       rozlał się na dodatkowe komórki (płetwy/fins), ale wszystkie te 
//       dodatkowe komórki znajdują się w obrębie jednego bloku (Box).
//       Zero-allocation z użyciem ExactPatternScratchpad.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p5_expert {

inline ApplyResult apply_finned_x_wing_sashimi(
    CandidateState& st, 
    StrategyStats& s, 
    GenericLogicCertifyResult& r) {
    
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Strategia ryb z płetwami wymaga podziału na bloki
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int n = st.topo->n;
    bool progress = false;
    
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        
        // Reset bitboardów dla rzędów i kolumn
        std::fill_n(sp.fish_row_masks, n, 0ULL);
        std::fill_n(sp.fish_col_masks, n, 0ULL);
        
        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            if ((st.cands[idx] & bit) == 0ULL) continue;
            
            const int rr = st.topo->cell_row[idx];
            const int cc = st.topo->cell_col[idx];
            sp.fish_row_masks[rr] |= (1ULL << cc);
            sp.fish_col_masks[cc] |= (1ULL << rr);
        }

        // --------------------------------------------------------------------
        // Szukanie w ujęciu Rzędów (Row-based Finned/Sashimi X-Wing)
        // --------------------------------------------------------------------
        for (int r1 = 0; r1 < n; ++r1) {
            const uint64_t m1 = sp.fish_row_masks[r1];
            const int c1 = std::popcount(m1);
            if (c1 < 2 || c1 > 4) continue; // max 2 bazowe + 2 płetwy (bo box max width/height w standardzie to zwykle mało, dla N=64 akceptujemy małe płetwy heurystycznie)
            
            for (int r2 = r1 + 1; r2 < n; ++r2) {
                const uint64_t m2 = sp.fish_row_masks[r2];
                const int c2 = std::popcount(m2);
                if (c2 < 2 || c2 > 4) continue;
                
                const uint64_t common = m1 & m2;
                if (std::popcount(common) != 2) continue; // Muszą współdzielić dokładnie 2 kolumny docelowe
                
                const uint64_t e1 = m1 & ~common; // Płetwy w r1
                const uint64_t e2 = m2 & ~common; // Płetwy w r2
                
                // Tylko jeden z rzędów może mieć płetwy (XOR)
                if ((e1 == 0ULL) == (e2 == 0ULL)) continue;

                const int fin_row = (e1 != 0ULL) ? r1 : r2;
                const uint64_t fin_mask = (e1 != 0ULL) ? e1 : e2;
                
                // Iterujemy po wspólnych kolumnach (Target columns)
                uint64_t wc = common;
                while (wc != 0ULL) {
                    const int base_col = config::bit_ctz_u64(wc);
                    wc = config::bit_clear_lsb_u64(wc);
                    
                    const int base_box = st.topo->cell_box[fin_row * n + base_col];
                    
                    // Sprawdzamy czy wszystkie płetwy leżą w tym samym bloku co base_col
                    bool has_fin_in_box = false;
                    bool invalid_fin = false;
                    
                    uint64_t wf = fin_mask;
                    while (wf != 0ULL) {
                        const int fin_col = config::bit_ctz_u64(wf);
                        wf = config::bit_clear_lsb_u64(wf);
                        
                        if (st.topo->cell_box[fin_row * n + fin_col] == base_box) {
                            has_fin_in_box = true;
                        } else {
                            invalid_fin = true;
                            break;
                        }
                    }
                    
                    if (invalid_fin || !has_fin_in_box) continue;

                    // Eliminacja: Kandydat 'd' usuwamy z komórek, które są w tym samym bloku co płetwy
                    // ORAZ są w kolumnie bazowej (ale nie należą do rzędów ryby).
                    for (int rr = 0; rr < n; ++rr) {
                        if (rr == r1 || rr == r2) continue;
                        
                        const int t = rr * n + base_col;
                        if (st.topo->cell_box[t] != base_box) continue;
                        
                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) { 
                            s.elapsed_ns += st.now_ns() - t0; 
                            return er; 
                        }
                        if (er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_finned_x_wing_sashimi = true;
                            progress = true;
                        }
                    }
                }
            }
        }

        // --------------------------------------------------------------------
        // Szukanie w ujęciu Kolumn (Col-based Finned/Sashimi X-Wing)
        // --------------------------------------------------------------------
        for (int c1 = 0; c1 < n; ++c1) {
            const uint64_t m1 = sp.fish_col_masks[c1];
            const int r1_cnt = std::popcount(m1);
            if (r1_cnt < 2 || r1_cnt > 4) continue;
            
            for (int c2 = c1 + 1; c2 < n; ++c2) {
                const uint64_t m2 = sp.fish_col_masks[c2];
                const int r2_cnt = std::popcount(m2);
                if (r2_cnt < 2 || r2_cnt > 4) continue;
                
                const uint64_t common = m1 & m2;
                if (std::popcount(common) != 2) continue;
                
                const uint64_t e1 = m1 & ~common; // Płetwy w c1
                const uint64_t e2 = m2 & ~common; // Płetwy w c2
                
                // Tylko jedna kolumna posiada płetwy
                if ((e1 == 0ULL) == (e2 == 0ULL)) continue;

                const int fin_col = (e1 != 0ULL) ? c1 : c2;
                const uint64_t fin_mask = (e1 != 0ULL) ? e1 : e2;
                
                uint64_t wr = common;
                while (wr != 0ULL) {
                    const int base_row = config::bit_ctz_u64(wr);
                    wr = config::bit_clear_lsb_u64(wr);
                    
                    const int base_box = st.topo->cell_box[base_row * n + fin_col];
                    
                    bool has_fin_in_box = false;
                    bool invalid_fin = false;
                    
                    uint64_t wf = fin_mask;
                    while (wf != 0ULL) {
                        const int f_row = config::bit_ctz_u64(wf);
                        wf = config::bit_clear_lsb_u64(wf);
                        
                        if (st.topo->cell_box[f_row * n + fin_col] == base_box) {
                            has_fin_in_box = true;
                        } else {
                            invalid_fin = true;
                            break;
                        }
                    }
                    
                    if (invalid_fin || !has_fin_in_box) continue;

                    // Eliminacja: w rzędzie bazowym, w bloku płetw, z wykluczeniem kolumn ryby
                    for (int cc = 0; cc < n; ++cc) {
                        if (cc == c1 || cc == c2) continue;
                        
                        const int t = base_row * n + cc;
                        if (st.topo->cell_box[t] != base_box) continue;
                        
                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) { 
                            s.elapsed_ns += st.now_ns() - t0; 
                            return er; 
                        }
                        if (er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_finned_x_wing_sashimi = true;
                            progress = true;
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return progress ? ApplyResult::Progress : ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p5_expert