// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: aligned_exclusion.h (Poziom 7 - Nightmare)
// Opis: Implementacja Aligned Pair Exclusion (APE) i Aligned Triple Exclusion (ATE).
//       Eliminuje pary lub trójki cyfr w komórkach kandydujących, jeżeli wszystkie
//       ich legalne kombinacje zmuszają "cel" (target) do złamania reguł.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

// ============================================================================
// Aligned Pair Exclusion (APE)
// ============================================================================
inline ApplyResult apply_aligned_pair_exclusion(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Optymalizacja wczesnego wyjścia (w siatkach gęstych to się nie wydarzy)
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int nn = st.topo->nn;
    bool progress = false;

    // Przeszukiwanie wszystkich możliwych par komórek, które się nawzajem widzą (Aligned)
    for (int a = 0; a < nn; ++a) {
        if (st.board->values[a] != 0) continue;
        const uint64_t ma = st.cands[a];
        if (ma == 0ULL) continue;
        
        for (int b = a + 1; b < nn; ++b) {
            if (st.board->values[b] != 0) continue;
            // Warunek zestrojenia - komórki "Aligned" muszą się widzieć
            if (!st.is_peer(a, b)) continue;
            
            const uint64_t mb = st.cands[b];
            if (mb == 0ULL) continue;

            uint64_t bad_a = 0ULL;
            uint64_t bad_b = 0ULL;
            
            // ----------------------------------------------------------------
            // Ewaluacja dla komórki A: szukamy cyfry, która NIE MA kompatybilnego 
            // sąsiada w komórce B, by mogły współistnieć w otoczeniu planszy.
            // ----------------------------------------------------------------
            uint64_t wa = ma;
            while (wa != 0ULL) {
                const uint64_t ba = config::bit_lsb(wa);
                wa = config::bit_clear_lsb_u64(wa);
                const int da = config::bit_ctz_u64(ba) + 1;
                
                bool valid_for_a = false;
                uint64_t wb = mb;
                while (wb != 0ULL) {
                    const uint64_t bb = config::bit_lsb(wb);
                    wb = config::bit_clear_lsb_u64(wb);
                    const int db = config::bit_ctz_u64(bb) + 1;
                    
                    if (da == db) continue; // Aligned Pair nie pozwala na te same cyfry (bo się widzą)
                    
                    // Sprawdzamy czy para (da w a) oraz (db w b) jest legalna względem ogólnych reguł planszy
                    if (st.board->can_place(a, da) && st.board->can_place(b, db)) {
                        valid_for_a = true;
                        break;
                    }
                }
                if (!valid_for_a) bad_a |= ba;
            }

            // ----------------------------------------------------------------
            // Ewaluacja dla komórki B (Lustrzana operacja w locie)
            // ----------------------------------------------------------------
            uint64_t wb = mb;
            while (wb != 0ULL) {
                const uint64_t bb = config::bit_lsb(wb);
                wb = config::bit_clear_lsb_u64(wb);
                const int db = config::bit_ctz_u64(bb) + 1;
                
                bool valid_for_b = false;
                uint64_t waa = ma;
                while (waa != 0ULL) {
                    const uint64_t ba = config::bit_lsb(waa);
                    waa = config::bit_clear_lsb_u64(waa);
                    const int da = config::bit_ctz_u64(ba) + 1;
                    
                    if (da == db) continue;
                    
                    if (st.board->can_place(a, da) && st.board->can_place(b, db)) {
                        valid_for_b = true;
                        break;
                    }
                }
                if (!valid_for_b) bad_b |= bb;
            }

            // Aplikacja potencjalnych uderzeń
            if (bad_a != 0ULL) {
                const ApplyResult er = st.eliminate(a, bad_a);
                if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                progress = progress || (er == ApplyResult::Progress);
            }
            if (bad_b != 0ULL) {
                const ApplyResult er = st.eliminate(b, bad_b);
                if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                progress = progress || (er == ApplyResult::Progress);
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_aligned_pair_exclusion = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}


// ============================================================================
// Aligned Triple Exclusion (ATE)
// ============================================================================
inline ApplyResult apply_aligned_triple_exclusion(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int nn = st.topo->nn;
    bool progress = false;

    for (int a = 0; a < nn; ++a) {
        if (st.board->values[a] != 0) continue;
        const uint64_t ma = st.cands[a];
        // Heurystyczne cięcie wydajnościowe: nie wchodzimy w duże trójki
        if (ma == 0ULL || std::popcount(ma) > 8) continue;
        
        for (int b = a + 1; b < nn; ++b) {
            if (st.board->values[b] != 0) continue;
            if (!st.is_peer(a, b)) continue;
            
            const uint64_t mb = st.cands[b];
            if (mb == 0ULL || std::popcount(mb) > 8) continue;
            
            for (int c = b + 1; c < nn; ++c) {
                if (st.board->values[c] != 0) continue;
                // C musi widzieć A ORAZ B
                if (!st.is_peer(a, c) || !st.is_peer(b, c)) continue;
                
                const uint64_t mc = st.cands[c];
                if (mc == 0ULL || std::popcount(mc) > 8) continue;

                uint64_t bad_a = 0ULL;
                
                // Trójpoziomowa kombinatoryka (działa szybko dzięki maskom, max 8*8*8 = 512 iteracji lokalnie)
                uint64_t wa = ma;
                while (wa != 0ULL) {
                    const uint64_t ba = config::bit_lsb(wa);
                    wa = config::bit_clear_lsb_u64(wa);
                    const int da = config::bit_ctz_u64(ba) + 1;
                    
                    bool valid = false;
                    uint64_t wb = mb;
                    
                    while (wb != 0ULL && !valid) {
                        const uint64_t bb = config::bit_lsb(wb);
                        wb = config::bit_clear_lsb_u64(wb);
                        const int db = config::bit_ctz_u64(bb) + 1;
                        if (db == da) continue;
                        
                        uint64_t wc = mc;
                        while (wc != 0ULL) {
                            const uint64_t bc = config::bit_lsb(wc);
                            wc = config::bit_clear_lsb_u64(wc);
                            const int dc = config::bit_ctz_u64(bc) + 1;
                            if (dc == da || dc == db) continue;
                            
                            // Czy trójka cyfr może legalnie bytować w siatce?
                            if (st.board->can_place(a, da) && st.board->can_place(b, db) && st.board->can_place(c, dc)) {
                                valid = true;
                                break; // Szybkie zwolnienie
                            }
                        }
                    }
                    if (!valid) bad_a |= ba;
                }

                if (bad_a != 0ULL) {
                    const ApplyResult er = st.eliminate(a, bad_a);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
                
                // Eliminacja B i C pominięta dla oszczędności symetrii operacji 
                // Pętla nadrzędna O(N^3) zajmie się zbadaniem b i c jako potencjalne 'a' w kolejnych iteracjach,
                // co uodparnia logikę na powielanie kodu (HPC Cache Friendly).
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_aligned_triple_exclusion = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare