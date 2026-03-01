// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: als_builder.h
// Opis: Algorytm wyszukujący zbiory Almost Locked Sets (ALS).
//       Kompletnie Zero-Allocation, bazuje na płaskich tablicach Scratchpada.
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>

#include "exact_pattern_scratchpad.h"
#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"

namespace sudoku_hpc::logic::shared {

inline void als_clear_cell_mask(ALS& als, int words) {
    for (int w = 0; w < words; ++w) {
        als.cell_mask[w] = 0ULL;
    }
}

inline bool als_cell_in(const ALS& als, int idx) {
    const int w = (idx >> 6);
    const int b = (idx & 63);
    return (als.cell_mask[w] & (1ULL << b)) != 0ULL;
}

inline void als_add_cell(ALS& als, int idx) {
    const int w = (idx >> 6);
    const int b = (idx & 63);
    als.cell_mask[w] |= (1ULL << b);
}

inline void als_push_record(
    ExactPatternScratchpad& sp,
    int words,
    int house,
    const int* cells,
    int cell_count,
    uint64_t digit_mask) {
    
    if (sp.als_count >= ExactPatternScratchpad::MAX_NN) return;
    
    ALS& rec = sp.als_list[sp.als_count++];
    als_clear_cell_mask(rec, words);
    
    for (int i = 0; i < cell_count; ++i) {
        als_add_cell(rec, cells[i]);
    }
    
    rec.digit_mask = digit_mask;
    rec.size = static_cast<uint8_t>(cell_count);
    // Stopień swobody (z definicji ALS wynosi 1, gdyż N komórek ma N+1 cyfr)
    rec.degree = static_cast<uint8_t>(std::popcount(digit_mask) - cell_count); 
    rec.house = static_cast<uint16_t>(std::max(0, house));
}

// Zwraca ilość znalezionych struktur ALS
inline int build_als_list(const CandidateState& st, int min_size = 2, int max_size = 4) {
    auto& sp = exact_pattern_scratchpad();
    sp.als_count = 0;
    
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    const int words = (nn + 63) >> 6;
    const int house_count = static_cast<int>(st.topo->house_offsets.size()) - 1;
    
    const int lo = std::clamp(min_size, 2, 6);
    const int hi = std::clamp(max_size, lo, 6);

    int house_cells[64]{};
    uint64_t house_masks[64]{};

    // Przeszukiwanie każdego z "domków" (Houses - wiersz, kolumna lub blok)
    for (int h = 0; h < house_count; ++h) {
        const int p0 = st.topo->house_offsets[static_cast<size_t>(h)];
        const int p1 = st.topo->house_offsets[static_cast<size_t>(h + 1)];
        int hc = 0;
        
        for (int p = p0; p < p1; ++p) {
            const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            
            const uint64_t m = st.cands[static_cast<size_t>(idx)];
            const int pc = std::popcount(m);
            // Wykluczenie gołych jedynek i nadmiarowych list
            if (pc < 2 || pc > n) continue;
            
            house_cells[hc] = idx;
            house_masks[hc] = m;
            ++hc;
            if (hc >= 64) break;
        }
        if (hc < lo) continue;

        // Budowanie kombinacji w obrębie znalezionego domku
        for (int a = 0; a < hc; ++a) {
            if (lo <= 1 && hi >= 1) {
                const uint64_t dm = house_masks[a];
                if (std::popcount(dm) == 2) {
                    const int cells[1] = {house_cells[a]};
                    als_push_record(sp, words, h, cells, 1, dm);
                }
            }
            for (int b = a + 1; b < hc; ++b) {
                const uint64_t dm2 = house_masks[a] | house_masks[b];
                if (lo <= 2 && hi >= 2 && std::popcount(dm2) == 3) {
                    const int cells2[2] = {house_cells[a], house_cells[b]};
                    als_push_record(sp, words, h, cells2, 2, dm2);
                }
                if (hi < 3) continue;
                
                for (int c = b + 1; c < hc; ++c) {
                    const uint64_t dm3 = dm2 | house_masks[c];
                    if (lo <= 3 && hi >= 3 && std::popcount(dm3) == 4) {
                        const int cells3[3] = {house_cells[a], house_cells[b], house_cells[c]};
                        als_push_record(sp, words, h, cells3, 3, dm3);
                    }
                    if (hi < 4) continue;
                    
                    for (int d = c + 1; d < hc; ++d) {
                        const uint64_t dm4 = dm3 | house_masks[d];
                        if (lo <= 4 && hi >= 4 && std::popcount(dm4) == 5) {
                            const int cells4[4] = {house_cells[a], house_cells[b], house_cells[c], house_cells[d]};
                            als_push_record(sp, words, h, cells4, 4, dm4);
                        }
                    }
                }
            }
        }
    }

    return sp.als_count;
}

} // namespace sudoku_hpc::logic::shared