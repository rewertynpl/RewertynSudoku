// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: squirmbag.h (Poziom 7 - Nightmare)
// Opis: Implementacja strategii Squirmbag (Starfish). Jest to "ryba" rzędu 5x5.
//       Bardzo obciążająca matematycznie kombinatoryka, oparta o rygorystyczne 
//       bitboardowanie i sprzężone skanowanie (Zero-Allocation).
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline ApplyResult apply_squirmbag(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    
    // Optymalizacja wczesnego wyjścia:
    // Starfish na planszach poniżej 5x5 matematycznie nie ma racji bytu
    if (n < 5) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    auto& sp = shared::exact_pattern_scratchpad();

    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
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

        int row_count = 0;
        int col_count = 0;
        
        // Zbieramy użyteczne linie (takie gdzie ryba w ogóle ma sens)
        for (int rr = 0; rr < n; ++rr) {
            const int cnt = std::popcount(sp.fish_row_masks[rr]);
            if (cnt >= 2 && cnt <= 5) sp.active_rows[row_count++] = rr;
        }
        for (int cc = 0; cc < n; ++cc) {
            const int cnt = std::popcount(sp.fish_col_masks[cc]);
            if (cnt >= 2 && cnt <= 5) sp.active_cols[col_count++] = cc;
        }

        // ====================================================================
        // Faza 1: Szukanie po rzędach (Row-based Squirmbag / Starfish)
        // ====================================================================
        for (int i = 0; i + 4 < row_count; ++i) {
            const int r1 = sp.active_rows[i];
            const uint64_t u1 = sp.fish_row_masks[r1];
            
            for (int j = i + 1; j + 3 < row_count; ++j) {
                const int r2 = sp.active_rows[j];
                const uint64_t u2 = u1 | sp.fish_row_masks[r2];
                if (std::popcount(u2) > 5) continue;
                
                for (int k = j + 1; k + 2 < row_count; ++k) {
                    const int r3 = sp.active_rows[k];
                    const uint64_t u3 = u2 | sp.fish_row_masks[r3];
                    if (std::popcount(u3) > 5) continue;
                    
                    for (int l = k + 1; l + 1 < row_count; ++l) {
                        const int r4 = sp.active_rows[l];
                        const uint64_t u4 = u3 | sp.fish_row_masks[r4];
                        if (std::popcount(u4) > 5) continue;
                        
                        for (int m = l + 1; m < row_count; ++m) {
                            const int r5 = sp.active_rows[m];
                            const uint64_t cols_union = u4 | sp.fish_row_masks[r5];
                            
                            // Musi idealnie nakładać się na maksymalnie 5 kolumn
                            if (std::popcount(cols_union) != 5) continue;
                            
                            // Uderzenie eliminujące po kolumnach
                            for (uint64_t w = cols_union; w != 0ULL; w &= (w - 1ULL)) {
                                const int cc = config::bit_ctz_u64(w);
                                for (int rr = 0; rr < n; ++rr) {
                                    if (rr == r1 || rr == r2 || rr == r3 || rr == r4 || rr == r5) continue;
                                    
                                    const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                    if (er == ApplyResult::Contradiction) { 
                                        s.elapsed_ns += st.now_ns() - t0; 
                                        return er; 
                                    }
                                    if (er == ApplyResult::Progress) {
                                        ++s.hit_count;
                                        r.used_squirmbag = true;
                                        s.elapsed_ns += st.now_ns() - t0;
                                        return ApplyResult::Progress;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ====================================================================
        // Faza 2: Szukanie po kolumnach (Col-based Squirmbag / Starfish)
        // ====================================================================
        for (int i = 0; i + 4 < col_count; ++i) {
            const int c1 = sp.active_cols[i];
            const uint64_t u1 = sp.fish_col_masks[c1];
            
            for (int j = i + 1; j + 3 < col_count; ++j) {
                const int c2 = sp.active_cols[j];
                const uint64_t u2 = u1 | sp.fish_col_masks[c2];
                if (std::popcount(u2) > 5) continue;
                
                for (int k = j + 1; k + 2 < col_count; ++k) {
                    const int c3 = sp.active_cols[k];
                    const uint64_t u3 = u2 | sp.fish_col_masks[c3];
                    if (std::popcount(u3) > 5) continue;
                    
                    for (int l = k + 1; l + 1 < col_count; ++l) {
                        const int c4 = sp.active_cols[l];
                        const uint64_t u4 = u3 | sp.fish_col_masks[c4];
                        if (std::popcount(u4) > 5) continue;
                        
                        for (int m = l + 1; m < col_count; ++m) {
                            const int c5 = sp.active_cols[m];
                            const uint64_t rows_union = u4 | sp.fish_col_masks[c5];
                            
                            if (std::popcount(rows_union) != 5) continue;
                            
                            for (uint64_t w = rows_union; w != 0ULL; w &= (w - 1ULL)) {
                                const int rr = config::bit_ctz_u64(w);
                                for (int cc = 0; cc < n; ++cc) {
                                    if (cc == c1 || cc == c2 || cc == c3 || cc == c4 || cc == c5) continue;
                                    
                                    const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                    if (er == ApplyResult::Contradiction) { 
                                        s.elapsed_ns += st.now_ns() - t0; 
                                        return er; 
                                    }
                                    if (er == ApplyResult::Progress) {
                                        ++s.hit_count;
                                        r.used_squirmbag = true;
                                        s.elapsed_ns += st.now_ns() - t0;
                                        return ApplyResult::Progress;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare