// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: intersections.h (Poziom 2)
// Opis: Usuwanie zablokowanych intersekcji z rzędów do bloków (Pointing) 
//       oraz z bloków do rzędów (Box-Line). 
//       Kompletnie odporne na prostokątne obszary (Asymetryczna geometria).
// ============================================================================

#pragma once

#include <cstdint>
#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p2_intersections {

// Obie te techniki szukają bardzo podobnych wzorców (przecięcie Box-Line), dlatego
// w celu optymalizacji przebiegów na pamięci, są zaimplementowane w jednej funkcji z 
// dwiema statystykami.
inline ApplyResult apply_pointing_and_boxline(
    CandidateState& st, 
    StrategyStats& sp, 
    StrategyStats& sb, 
    GenericLogicCertifyResult& r) {
    
    const uint64_t t0p = st.now_ns();
    ++sp.use_count;
    const int n = st.topo->n;
    bool p_progress = false;
    
    // ------------------------------------------------------------------------
    // FAZA 1: Pointing Pairs/Triples (Z Box'a do Row/Col)
    // ------------------------------------------------------------------------
    for (int brg = 0; brg < st.topo->box_rows_count; ++brg) {
        for (int bcg = 0; bcg < st.topo->box_cols_count; ++bcg) {
            const int r0 = brg * st.topo->box_rows;
            const int c0 = bcg * st.topo->box_cols;
            
            for (int d = 1; d <= n; ++d) {
                const uint64_t bit = (1ULL << (d - 1));
                int fr = -1, fc = -1, cnt = 0;
                bool same_row = true, same_col = true;
                
                for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                    for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                        const int rr = r0 + dr;
                        const int cc = c0 + dc;
                        const int idx = rr * n + cc;
                        
                        if (st.board->values[idx] != 0) continue;
                        if ((st.cands[idx] & bit) == 0ULL) continue;
                        
                        if (cnt == 0) { 
                            fr = rr; 
                            fc = cc; 
                        } else { 
                            same_row = same_row && (rr == fr); 
                            same_col = same_col && (cc == fc); 
                        }
                        ++cnt;
                    }
                }
                
                // Wymaga min. 2 komórek do eliminacji na podstawie rzutowania
                if (cnt < 2) continue;
                
                if (same_row) {
                    for (int c = 0; c < n; ++c) {
                        // Omijanie wewnątrz bloku
                        if (c >= c0 && c < c0 + st.topo->box_cols) continue;
                        
                        const ApplyResult er = st.eliminate(fr * n + c, bit);
                        if (er == ApplyResult::Contradiction) { 
                            sp.elapsed_ns += st.now_ns() - t0p; 
                            return er; 
                        }
                        p_progress = p_progress || (er == ApplyResult::Progress);
                    }
                }
                
                if (same_col) {
                    for (int rr = 0; rr < n; ++rr) {
                        // Omijanie wewnątrz bloku
                        if (rr >= r0 && rr < r0 + st.topo->box_rows) continue;
                        
                        const ApplyResult er = st.eliminate(rr * n + fc, bit);
                        if (er == ApplyResult::Contradiction) { 
                            sp.elapsed_ns += st.now_ns() - t0p; 
                            return er; 
                        }
                        p_progress = p_progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }
    }
    
    sp.elapsed_ns += st.now_ns() - t0p;
    if (p_progress) { 
        ++sp.hit_count; 
        r.used_pointing_pairs = true; 
        return ApplyResult::Progress; 
    }

    // ------------------------------------------------------------------------
    // FAZA 2: Box/Line Reduction (Z Row/Col do Box'a)
    // ------------------------------------------------------------------------
    const uint64_t t0b = st.now_ns();
    ++sb.use_count;
    bool b_progress = false;
    
    // Skan rzędów
    for (int r0 = 0; r0 < n; ++r0) {
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int first_box = -1, cnt = 0; 
            bool same_box = true;
            
            for (int c = 0; c < n; ++c) {
                const int idx = r0 * n + c;
                if (st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                
                const int box = st.topo->cell_box[idx];
                if (cnt == 0) first_box = box; 
                else same_box = same_box && (box == first_box);
                ++cnt;
            }
            
            if (!same_box || cnt < 2 || first_box < 0) continue;
            
            // Asymetryczna matematyka: Box zdekodowany
            const int brg = first_box / st.topo->box_cols_count;
            const int bcg = first_box % st.topo->box_cols_count;
            
            for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                    const int rr = brg * st.topo->box_rows + dr;
                    const int cc = bcg * st.topo->box_cols + dc;
                    // Omijanie źródłowego rzędu
                    if (rr == r0) continue;
                    
                    const ApplyResult er = st.eliminate(rr * n + cc, bit);
                    if (er == ApplyResult::Contradiction) { 
                        sb.elapsed_ns += st.now_ns() - t0b; 
                        return er; 
                    }
                    b_progress = b_progress || (er == ApplyResult::Progress);
                }
            }
        }
    }
    
    // Skan kolumn
    for (int c0 = 0; c0 < n; ++c0) {
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int first_box = -1, cnt = 0; 
            bool same_box = true;
            
            for (int r0 = 0; r0 < n; ++r0) {
                const int idx = r0 * n + c0;
                if (st.board->values[idx] != 0 || (st.cands[idx] & bit) == 0ULL) continue;
                
                const int box = st.topo->cell_box[idx];
                if (cnt == 0) first_box = box; 
                else same_box = same_box && (box == first_box);
                ++cnt;
            }
            
            if (!same_box || cnt < 2 || first_box < 0) continue;
            
            const int brg = first_box / st.topo->box_cols_count;
            const int bcg = first_box % st.topo->box_cols_count;
            
            for (int dr = 0; dr < st.topo->box_rows; ++dr) {
                for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                    const int rr = brg * st.topo->box_rows + dr;
                    const int cc = bcg * st.topo->box_cols + dc;
                    // Omijanie źródłowej kolumny
                    if (cc == c0) continue;
                    
                    const ApplyResult er = st.eliminate(rr * n + cc, bit);
                    if (er == ApplyResult::Contradiction) { 
                        sb.elapsed_ns += st.now_ns() - t0b; 
                        return er; 
                    }
                    b_progress = b_progress || (er == ApplyResult::Progress);
                }
            }
        }
    }
    
    sb.elapsed_ns += st.now_ns() - t0b;
    if (b_progress) { 
        ++sb.hit_count; 
        r.used_box_line = true; 
        return ApplyResult::Progress; 
    }
    
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p2_intersections