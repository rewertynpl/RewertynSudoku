// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: fish_basic.h (Poziom 4)
// Opis: Algorytmy szukające X-Wing oraz Swordfish. 
//       Wykorzystanie szybkiego bitboardu w celu optymalizacji O(N^3).
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p4_hard {

// Zunifikowany algorytm X-Wing (szuka 2x2 kwadratów dla rzędów i kolumn)
inline ApplyResult apply_x_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    bool progress = false;
    
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

        // Faza X-Wing na rzędach
        for (int r1 = 0; r1 < n; ++r1) {
            const uint64_t m1 = sp.fish_row_masks[r1];
            if (std::popcount(m1) != 2) continue; // Wymagamy dokładnie 2 kandydatów
            
            for (int r2 = r1 + 1; r2 < n; ++r2) {
                // Czy drugi rząd ma takie same kandydatury na kolumnach?
                if (sp.fish_row_masks[r2] != m1) continue;
                
                // Znalazlismy X-Wing. Eliminujemy w tych 2 kolumnach dla wszystkich innych rzędów.
                uint64_t w = m1;
                while (w != 0ULL) {
                    const int c = config::bit_ctz_u64(w);
                    w = config::bit_clear_lsb_u64(w);
                    
                    for (int rr = 0; rr < n; ++rr) {
                        if (rr == r1 || rr == r2) continue;
                        
                        const ApplyResult er = st.eliminate(rr * n + c, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }

        // Faza X-Wing na kolumnach
        for (int c1 = 0; c1 < n; ++c1) {
            const uint64_t m1 = sp.fish_col_masks[c1];
            if (std::popcount(m1) != 2) continue;
            
            for (int c2 = c1 + 1; c2 < n; ++c2) {
                if (sp.fish_col_masks[c2] != m1) continue;
                
                uint64_t w = m1;
                while (w != 0ULL) {
                    const int rr = config::bit_ctz_u64(w);
                    w = config::bit_clear_lsb_u64(w);
                    
                    for (int cc = 0; cc < n; ++cc) {
                        if (cc == c1 || cc == c2) continue;
                        
                        const ApplyResult er = st.eliminate(rr * n + cc, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_x_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

// Zunifikowany algorytm Swordfish (Ryba rozmiaru 3x3)
inline ApplyResult apply_swordfish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    bool progress = false;
    
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
        
        // Zbieramy tylko te linie, w których kandydat pojawia się 2 do 3 razy
        for (int rr = 0; rr < n; ++rr) {
            const int cnt = std::popcount(sp.fish_row_masks[rr]);
            if (cnt >= 2 && cnt <= 3) sp.active_rows[row_count++] = rr;
        }
        for (int cc = 0; cc < n; ++cc) {
            const int cnt = std::popcount(sp.fish_col_masks[cc]);
            if (cnt >= 2 && cnt <= 3) sp.active_cols[col_count++] = cc;
        }

        // Swordfish dla Rzędów (szukamy 3 rzędów, których złączone wystąpienia obejmują max 3 kolumny)
        for (int i = 0; i < row_count; ++i) {
            const int r1 = sp.active_rows[i];
            const uint64_t m1 = sp.fish_row_masks[r1];
            for (int j = i + 1; j < row_count; ++j) {
                const int r2 = sp.active_rows[j];
                const uint64_t m12 = m1 | sp.fish_row_masks[r2];
                if (std::popcount(m12) > 3) continue;
                
                for (int k = j + 1; k < row_count; ++k) {
                    const int r3 = sp.active_rows[k];
                    const uint64_t cols_union = m12 | sp.fish_row_masks[r3];
                    if (std::popcount(cols_union) != 3) continue;
                    
                    uint64_t w = cols_union;
                    while (w != 0ULL) {
                        const int cc = config::bit_ctz_u64(w);
                        w = config::bit_clear_lsb_u64(w);
                        for (int rr = 0; rr < n; ++rr) {
                            if (rr == r1 || rr == r2 || rr == r3) continue;
                            
                            const ApplyResult er = st.eliminate(rr * n + cc, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }

        // Swordfish dla Kolumn
        for (int i = 0; i < col_count; ++i) {
            const int c1 = sp.active_cols[i];
            const uint64_t m1 = sp.fish_col_masks[c1];
            for (int j = i + 1; j < col_count; ++j) {
                const int c2 = sp.active_cols[j];
                const uint64_t m12 = m1 | sp.fish_col_masks[c2];
                if (std::popcount(m12) > 3) continue;
                
                for (int k = j + 1; k < col_count; ++k) {
                    const int c3 = sp.active_cols[k];
                    const uint64_t rows_union = m12 | sp.fish_col_masks[c3];
                    if (std::popcount(rows_union) != 3) continue;
                    
                    uint64_t w = rows_union;
                    while (w != 0ULL) {
                        const int rr = config::bit_ctz_u64(w);
                        w = config::bit_clear_lsb_u64(w);
                        for (int cc = 0; cc < n; ++cc) {
                            if (cc == c1 || cc == c2 || cc == c3) continue;
                            
                            const ApplyResult er = st.eliminate(rr * n + cc, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_swordfish = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p4_hard