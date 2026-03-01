// ============================================================================
// SUDOKU HPC - CORE
// Moduł: candidate_state.h
// Opis: Reprezentacja stanu poszukiwań. Zmieniona z wektora na wskaźnik do
//       pamięci Thread Local Storage. Zapewnia Zero-Allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <chrono>

#include "board.h"
#include "geometry.h"
#include "../logic/logic_result.h"

namespace sudoku_hpc {

using logic::ApplyResult;

struct CandidateState {
    GenericBoard* board = nullptr;
    const GenericTopology* topo = nullptr;
    
    // Wskaźnik na płaski bufor thread_local, zapobiega alokacjom na stercie w trakcie rozwiązywania
    uint64_t* cands = nullptr;

    bool init(GenericBoard& b, const GenericTopology& t, uint64_t* tls_buffer) {
        board = &b;
        topo = &t;
        cands = tls_buffer;
        
        for (int idx = 0; idx < t.nn; ++idx) {
            if (b.values[idx] != 0) {
                cands[idx] = 0ULL;
                continue;
            }
            const uint64_t m = b.candidate_mask_for_idx(idx);
            if (m == 0ULL) return false;
            cands[idx] = m;
        }
        return true;
    }

    uint64_t now_ns() const {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    bool is_peer(int a, int b) const {
        if (a == b) return false;
        return topo->cell_row[a] == topo->cell_row[b] ||
               topo->cell_col[a] == topo->cell_col[b] ||
               topo->cell_box[a] == topo->cell_box[b];
    }

    bool place(int idx, int d) {
        if (board->values[idx] != 0) {
            return board->values[idx] == static_cast<uint16_t>(d);
        }
        const uint64_t bit = (1ULL << (d - 1));
        if ((cands[idx] & bit) == 0ULL) return false;
        if (!board->can_place(idx, d)) return false;
        
        board->place(idx, d);
        cands[idx] = 0ULL;
        
        const int p0 = topo->peer_offsets[idx];
        const int p1 = topo->peer_offsets[idx + 1];
        
        for (int p = p0; p < p1; ++p) {
            const int peer = topo->peers_flat[p];
            if (board->values[peer] != 0) continue;
            
            uint64_t& pm = cands[peer];
            if ((pm & bit) == 0ULL) continue;
            
            pm &= ~bit;
            if (pm == 0ULL) return false; // Sprzeczność - wyczerpano kandydatów
        }
        return true;
    }

    ApplyResult eliminate(int idx, uint64_t rm) {
        if (rm == 0ULL || board->values[idx] != 0) return ApplyResult::NoProgress;
        
        uint64_t& m = cands[idx];
        if ((m & rm) == 0ULL) return ApplyResult::NoProgress;
        
        m &= ~rm;
        if (m == 0ULL) return ApplyResult::Contradiction;
        
        return ApplyResult::Progress;
    }

    ApplyResult keep_only(int idx, uint64_t allowed) {
        if (board->values[idx] != 0) return ApplyResult::NoProgress;
        
        uint64_t& m = cands[idx];
        const uint64_t nm = m & allowed;
        
        if (nm == m) return ApplyResult::NoProgress;
        if (nm == 0ULL) return ApplyResult::Contradiction;
        
        m = nm;
        return ApplyResult::Progress;
    }
};

} // namespace sudoku_hpc