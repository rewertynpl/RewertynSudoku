// ============================================================================
// SUDOKU HPC - CORE ENGINES
// Moduł: quick_prefilter.h
// Opis: Błyskawiczny, "brudny" filtr odrzucający siatki przed rygorystyczną
//       weryfikacją logiczną i MCTS. Zero-allocation, pełne AVX scaling.
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>

#include "../../core/board.h"

namespace sudoku_hpc::core_engines {

struct GenericQuickPrefilter {
    // Zwraca false jeśli plansza na pewno jest nieprawidłowa lub nie ma sensu jej testować.
    bool check(const std::vector<uint16_t>& puzzle, const GenericTopology& topo, int min_clues, int max_clues) const {
        if (static_cast<int>(puzzle.size()) != topo.nn) {
            return false;
        }

        int clues = 0;
        const auto& rcb = topo.cell_rcb_packed;
        
        // Zero-allocation: wykorzystanie prealokowanego obszaru roboczego
        GenericThreadScratch& scratch = generic_tls_for(topo);
        
        // Szybkie wyzerowanie bitsetów
        std::fill(scratch.row_tmp.begin(), scratch.row_tmp.end(), 0ULL);
        std::fill(scratch.col_tmp.begin(), scratch.col_tmp.end(), 0ULL);
        std::fill(scratch.box_tmp.begin(), scratch.box_tmp.end(), 0ULL);
        
        uint64_t* const row_used = scratch.row_tmp.data();
        uint64_t* const col_used = scratch.col_tmp.data();
        uint64_t* const box_used = scratch.box_tmp.data();
        const uint32_t* const packed = rcb.data();
        
        for (int idx = 0; idx < topo.nn; ++idx) {
            const int d = static_cast<int>(puzzle[static_cast<size_t>(idx)]);
            if (d == 0) {
                // Wczesna weryfikacja czy w ogóle starczy nam miejsc by dobić do min_clues
                const int remaining = topo.nn - idx - 1;
                if (clues + remaining < min_clues) {
                    return false;
                }
                continue;
            }
            if (d < 1 || d > topo.n) {
                return false;
            }
            
            ++clues;
            
            if (clues > max_clues) {
                return false;
            }

            const uint32_t p = packed[static_cast<size_t>(idx)];
            const int r = static_cast<int>(p & 63U);
            const int c = static_cast<int>((p >> 6U) & 63U);
            const int b = static_cast<int>((p >> 12U) & 63U);
            
            const uint64_t bit = (1ULL << (d - 1));
            
            // Kolizja w obrębie struktury?
            if ((row_used[static_cast<size_t>(r)] & bit) ||
                (col_used[static_cast<size_t>(c)] & bit) ||
                (box_used[static_cast<size_t>(b)] & bit)) {
                return false;
            }
            
            row_used[static_cast<size_t>(r)] |= bit;
            col_used[static_cast<size_t>(c)] |= bit;
            box_used[static_cast<size_t>(b)] |= bit;
        }
        
        return clues >= min_clues;
    }
};

} // namespace sudoku_hpc::core_engines