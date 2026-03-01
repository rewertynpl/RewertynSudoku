// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: borescoper_qiu.h (Poziom 6 - Diabolical)
// Opis: Implementacja strategii z rodziny "Deadly Patterns" - ze szczegĂłlnym 
//       uwzglÄ™dnieniem Borescoper's Deadly Pattern oraz Qiu's Deadly Pattern.
//       SÄ… to zĹ‚oĹĽone ukĹ‚ady 4 wierzchoĹ‚kĂłw na rĂłĹĽnych blokach, ktĂłrych 
//       doprowadzenie do 2 cyfr rujnuje unikalnoĹ›Ä‡ planszy.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>
#include <array>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

// DoĹ‚Ä…czamy narzÄ™dzia niezbÄ™dne do analizy kompozytowej
#include "ur_extended.h"
#include "bug_variants.h"
#include "../p7_nightmare/aic_grouped_aic.h"

namespace sudoku_hpc::logic::p6_diabolical {

// ============================================================================
// Borescoper's Deadly Pattern / Qiu's Deadly Pattern
// Kombinacja twardej heurystyki oraz zestawu zagnieĹĽdĹĽonych wywoĹ‚aĹ„
// z wariantĂłw UR/BUG (Composite).
// ============================================================================
inline ApplyResult apply_borescoper_qiu_deadly_pattern(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    // Heurystyka wczesnego wyjĹ›cia - brak blokĂłw (brak geometrii UR) lub plansza jest zbyt pusta
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};

    // Krok 1: Wiele z tych struktur matematycznie objawia siÄ™ jako rozszerzone 
    // Unique Rectangles (Type 2-6) - z tÄ… rĂłĹĽnicÄ…, ĹĽe sÄ… mocniej rozrzucone.
    ApplyResult ar = apply_ur_extended(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar; 
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_borescoper_qiu_deadly_pattern = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // Krok 2: Odrzucenie ukrytych skrzyĹĽowaĹ„ (Hidden UR), co matematycznie 
    // pokrywa czÄ™Ĺ›Ä‡ Qiu's Deadly Pattern.
    ar = apply_hidden_ur(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar; 
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_borescoper_qiu_deadly_pattern = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // Krok 3: Analiza BUG (Bivalue Universal Grave)
    ar = apply_bug_type2(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; return ar; }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_borescoper_qiu_deadly_pattern = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    ar = apply_bug_type3(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; return ar; }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_borescoper_qiu_deadly_pattern = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    ar = apply_bug_type4(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; return ar; }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_borescoper_qiu_deadly_pattern = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // ------------------------------------------------------------------------
    // Krok 4: Extra Deadly check (Twarda detekcja Borescoper's/Qiu's DP)
    //         JeĹĽeli ZEWNÄTRZNA komĂłrka (spoza struktury) potrafi wygenerowaÄ‡ 
    //         sprzecznoĹ›Ä‡ dla 4 komĂłrek UR i widzi je wszystkie jednoczeĹ›nie
    //         z parÄ… cyfr - purguje zaporÄ™.
    // ------------------------------------------------------------------------
    const int n = st.topo->n;
    bool progress = false;

    for (int r1 = 0; r1 < n; ++r1) {
        for (int r2 = r1 + 1; r2 < n; ++r2) {
            for (int c1 = 0; c1 < n; ++c1) {
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const int a = r1 * n + c1;
                    const int b = r1 * n + c2;
                    const int c = r2 * n + c1;
                    const int d = r2 * n + c2;
                    
                    if (st.board->values[a] != 0 ||
                        st.board->values[b] != 0 ||
                        st.board->values[c] != 0 ||
                        st.board->values[d] != 0) {
                        continue;
                    }
                    
                    // Badamy przeciÄ™cie czterech komĂłrek by znaleĹşÄ‡ parÄ™ Ĺ›miercionoĹ›nÄ…
                    const uint64_t pair = st.cands[a] & st.cands[b] & st.cands[c] & st.cands[d];
                    if (std::popcount(pair) != 2) continue; // DP wymaga pary cyfr
                    
                    // Cel ataku (target) to inna komĂłrka na planszy widzÄ…ca w peĹ‚ni kwartet
                    for (int t = 0; t < st.topo->nn; ++t) {
                        if (t == a || t == b || t == c || t == d) continue;
                        if (st.board->values[t] != 0) continue;
                        
                        // Czy target widzi wszystkie komĂłrki struktury Borescoper?
                        if (!st.is_peer(t, a) || !st.is_peer(t, b) || 
                            !st.is_peer(t, c) || !st.is_peer(t, d)) {
                            continue;
                        }
                        
                        // Wyeliminowanie 'pary' by nie zamknÄ…Ä‡ pÄ™tli
                        const ApplyResult er = st.eliminate(t, pair);
                        if (er == ApplyResult::Contradiction) { 
                            s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
                            return er; 
                        }
                        if (er == ApplyResult::Progress) {
                            progress = true;
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_borescoper_qiu_deadly_pattern = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
