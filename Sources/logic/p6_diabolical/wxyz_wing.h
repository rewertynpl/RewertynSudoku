// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: wxyz_wing.h (Poziom 6 - Diabolical)
// Opis: Algorytm wyszukujący 4-kandydatowy WXYZ-Wing.
//       Potężne skrzydło łączące węzeł centralny (Pivot) z aż trzeba skrzydłami
//       Bivalue rekonstruującymi całą binarową strukturę pivota. Zero-allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline ApplyResult apply_wxyz_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    const int nn = st.topo->nn;
    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    // Szukamy potencjalnego pivota: Posiada dokładnie 4 różne możliwości cyfr.
    for (int pivot = 0; pivot < nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;
        const uint64_t mp = st.cands[pivot];
        if (std::popcount(mp) != 4) continue;

        sp.wing_count = 0;
        
        // Zbieramy komórki (skrzydła) widoczne z pivota
        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        for (int p = p0; p < p1; ++p) {
            const int w = st.topo->peers_flat[p];
            if (st.board->values[w] != 0) continue;
            
            const uint64_t mw = st.cands[w];
            // Skrzydło to komórka bivalue posiadająca cyfry należące do masek Pivota
            if (std::popcount(mw) != 2) continue;
            if ((mw & ~mp) != 0ULL) continue;
            
            if (sp.wing_count < nn) {
                sp.wing_cells[sp.wing_count++] = w;
            }
        }
        
        // Wymagamy dokładnie 3 takich bivalue-"skrzydeł"
        if (sp.wing_count < 3) continue;

        // Szukamy kombinacji 3 skrzydeł do stworzenia zatoru WXYZ
        for (int i = 0; i + 2 < sp.wing_count; ++i) {
            const int a = sp.wing_cells[i];
            const uint64_t ma = st.cands[a];
            
            for (int j = i + 1; j + 1 < sp.wing_count; ++j) {
                const int b = sp.wing_cells[j];
                const uint64_t mb = st.cands[b];
                
                for (int k = j + 1; k < sp.wing_count; ++k) {
                    const int c = sp.wing_cells[k];
                    const uint64_t mc = st.cands[c];
                    
                    // Skrzydła muszą ODTWARZAĆ dokładnie taką samą powłokę co pivot
                    // Np. (1,2) + (2,3) + (3,4) + pivot(1,2,3,4)
                    if ((ma | mb | mc | mp) != mp) continue;
                    
                    // Cyfry "targetu" (te do wyeliminowania) to te, które występują 
                    // we wszystkich 3 skrzydłach tworzących węzeł zaporowy
                    const uint64_t zmask = ma & mb & mc;
                    if (zmask == 0ULL) continue;

                    uint64_t wz = zmask;
                    while (wz != 0ULL) {
                        const uint64_t z = config::bit_lsb(wz);
                        wz = config::bit_clear_lsb_u64(wz);
                        
                        // Eliminacja! 
                        // Target(y) musi widzieć pivot ORAZ wszystkie 3 skrzydła 
                        for (int t = 0; t < nn; ++t) {
                            if (t == pivot || t == a || t == b || t == c) continue;
                            if (st.board->values[t] != 0) continue;
                            if ((st.cands[t] & z) == 0ULL) continue;
                            
                            if (!st.is_peer(t, pivot) || !st.is_peer(t, a) || !st.is_peer(t, b) || !st.is_peer(t, c)) continue;
                            
                            const ApplyResult er = st.eliminate(t, z);
                            if (er == ApplyResult::Contradiction) { 
                                s.elapsed_ns += st.now_ns() - t0; 
                                return er; 
                            }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_wxyz_wing = true;
                                progress = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical