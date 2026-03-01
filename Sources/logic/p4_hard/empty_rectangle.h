// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: empty_rectangle.h (Poziom 4)
// Opis: Algorytm wyszukujący tzw. Puste Prostokąty (Empty Rectangle).
//       Sprawdza bloki, w których dana cyfra występuje tylko w jednej 
//       kolumnie i jednym rzędzie (kształt litery 'L' na bitboardzie).
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>
#include <array>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p4_hard {

// Zoptymalizowany dla siatek wielkoformatowych algorytm ER.
inline ApplyResult apply_empty_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    const int n = st.topo->n;
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
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

        for (int b = 0; b < n; ++b) {
            sp.als_cell_count = 0; // reużywamy wolny bufor w P4 na cele box_cells
            
            // Znajdź wszystkie wystąpienia cyfry 'd' w aktualnym Box'ie
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.topo->cell_box[idx] != b) continue;
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                
                sp.als_cells[sp.als_cell_count++] = idx;
                // Jeśli w boxie są więcej niż 2 wystąpienia, nadal może być to ER o ile tworzą +
                // ale dla celów zoptymalizowanej heurystyki HPC ograniczamy do kształtu L
                if (sp.als_cell_count > 2) break;
            }
            
            if (sp.als_cell_count != 2) continue;
            
            const int p = sp.als_cells[0];
            const int q = sp.als_cells[1];
            
            const int pr = st.topo->cell_row[p];
            const int pc = st.topo->cell_col[p];
            const int qr = st.topo->cell_row[q];
            const int qc = st.topo->cell_col[q];
            
            // Komórki z Box'a muszą znajdować się w innych rzędach i innych kolumnach, by utworzyć kształt L
            if (pr == qr || pc == qc) continue;

            struct OrientedPair { 
                int row_cell; 
                int col_cell; 
            };
            const std::array<OrientedPair, 2> orientations = {{
                {p, q}, {q, p}
            }};
            
            for (const auto& orient : orientations) {
                const int row_cell = orient.row_cell;
                const int col_cell = orient.col_cell;
                
                const int rr = st.topo->cell_row[row_cell];
                const int cc = st.topo->cell_col[col_cell];

                // Badamy wierzchołek ER od strony rzędu
                const uint64_t row_m = sp.fish_row_masks[rr];
                if (std::popcount(row_m) != 2 || (row_m & (1ULL << st.topo->cell_col[row_cell])) == 0ULL) continue;
                
                const uint64_t row_other_mask = row_m & ~(1ULL << st.topo->cell_col[row_cell]);
                if (row_other_mask == 0ULL) continue;
                const int row_other_col = config::bit_ctz_u64(row_other_mask);
                const int row_other = rr * n + row_other_col;

                // Badamy wierzchołek ER od strony kolumny
                const uint64_t col_m = sp.fish_col_masks[cc];
                if (std::popcount(col_m) != 2 || (col_m & (1ULL << st.topo->cell_row[col_cell])) == 0ULL) continue;
                
                const uint64_t col_other_mask = col_m & ~(1ULL << st.topo->cell_row[col_cell]);
                if (col_other_mask == 0ULL) continue;
                const int col_other_row = config::bit_ctz_u64(col_other_mask);
                const int col_other = col_other_row * n + cc;
                
                if (row_other == col_other) continue;

                // Sprawdzamy wzajemne powiązanie w miejscu krzyżowania zewnętrznych wypustek (Z)
                const int p0 = st.topo->peer_offsets[row_other];
                const int p1 = st.topo->peer_offsets[row_other + 1];
                for (int pi = p0; pi < p1; ++pi) {
                    const int t = st.topo->peers_flat[pi];
                    if (t == row_other || t == col_other) continue;
                    
                    if (!st.is_peer(t, col_other)) continue;
                    
                    const ApplyResult er = st.eliminate(t, bit);
                    if (er == ApplyResult::Contradiction) { 
                        s.elapsed_ns += st.now_ns() - t0; 
                        return er; 
                    }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_empty_rectangle = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p4_hard