// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: sk_loop.h (Poziom 8 - Theoretical)
// Opis: Algorytm rozwiÄ…zujÄ…cy cykle oparte na SK-Loop (Stephen Kurz Loop).
//       Sprawdza zamkniÄ™te prostokÄ…ty z dedykowanym "Core" i "Extra" masek,
//       wspierajÄ…c siÄ™ kompozycjami gĹ‚Ä™bokich pÄ™tli w razie braku dokĹ‚adnego
//       dopasowania klasycznego wzorca. Zoptymalizowany dla Zero-Allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>
#include <array>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

// ModuĹ‚y pomocnicze (kompozytu do SK Loop)
#include "../p7_nightmare/continuous_nice_loop.h"
#include "../p7_nightmare/grouped_x_cycle.h"
#include "../p7_nightmare/aic_grouped_aic.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool sk_loop_propagate_singles(CandidateState& st, int max_steps) {
    const int nn = st.topo->nn;
    for (int step = 0; step < max_steps; ++step) {
        bool changed = false;
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[idx] != 0) continue;
            const uint64_t m = st.cands[idx];
            if (m == 0ULL) return false;
            const int sd = config::single_digit_from_mask(m);
            if (sd == 0) continue;
            if (!st.place(idx, sd)) return false;
            changed = true;
        }
        if (!changed) break;
    }
    return true;
}

inline bool sk_loop_probe_contradiction(
    CandidateState& st,
    int idx,
    int d,
    int max_steps,
    shared::ExactPatternScratchpad& sp) {
    const int nn = st.topo->nn;
    std::copy_n(st.cands, nn, sp.dyn_cands_backup);
    std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
    sp.dyn_empty_backup = st.board->empty_cells;

    bool contradiction = false;
    if (!st.place(idx, d)) {
        contradiction = true;
    } else if (!sk_loop_propagate_singles(st, max_steps)) {
        contradiction = true;
    }

    std::copy_n(sp.dyn_cands_backup, nn, st.cands);
    std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
    st.board->empty_cells = sp.dyn_empty_backup;
    return contradiction;
}

// ============================================================================
// SK LOOP EXACT
// Klasyczne podejĹ›cie "twardej geometrii": prostokÄ…t, w ktĂłrym 4 komĂłrki
// dzielÄ… 2 cyfry rdzeniowe ("Core"), oraz kaĹĽda/niektĂłre majÄ… nadmiar,
// co pozwala na skrzyĹĽowanÄ… eliminacjÄ™ cyfr wyjĹ›ciowych z reszty rzÄ™du/kolumny.
// ============================================================================
inline ApplyResult apply_sk_loop_exact(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 6 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }
    const int pattern_cap = std::clamp(2 + n / 3, 4, 10);
    const int probe_steps = std::clamp(6 + n / 4, 8, 12);
    int patterns = 0;
    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    // Przeszukiwanie wierzchoĹ‚kĂłw dla SK Loop
    for (int r1 = 0; r1 < n; ++r1) {
        for (int r2 = r1 + 1; r2 < n; ++r2) {
            for (int c1 = 0; c1 < n; ++c1) {
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    if (patterns >= pattern_cap) {
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return ApplyResult::NoProgress;
                    }
                    const int a = r1 * n + c1;
                    const int b = r1 * n + c2;
                    const int c = r2 * n + c1;
                    const int d = r2 * n + c2;
                    
                    // Wszystkie punkty cyklu muszÄ… byÄ‡ nieustalone
                    if (st.board->values[a] != 0 ||
                        st.board->values[b] != 0 ||
                        st.board->values[c] != 0 ||
                        st.board->values[d] != 0) {
                        continue;
                    }

                    const uint64_t ma = st.cands[a];
                    const uint64_t mb = st.cands[b];
                    const uint64_t mc = st.cands[c];
                    const uint64_t md = st.cands[d];
                    
                    // Szukamy "Rdzenia" (Core) â€“ 2 cyfr dzielonych przez wszystkie wierzchoĹ‚ki
                    const uint64_t core = ma & mb & mc & md;
                    if (std::popcount(core) != 2) continue;

                    // Pobieramy "Nadmiary" (Extra candidates) wykraczajÄ…ce poza cykl gĹ‚Ăłwny
                    const uint64_t exa = ma & ~core;
                    const uint64_t exb = mb & ~core;
                    const uint64_t exc = mc & ~core;
                    const uint64_t exd = md & ~core;
                    
                    const uint64_t extra_union = exa | exb | exc | exd;
                    if (extra_union == 0ULL) continue; // W peĹ‚ni czysty DP (Deadly Pattern), obsĹ‚ugiwane w P6 UR
                    ++patterns;

                    // Sprawdzamy kaĹĽdÄ… wyodrÄ™bnionÄ… cyfrÄ™ "Z" i usuwamy z przeciÄ™Ä‡ wzroku
                    uint64_t wx = extra_union;
                    while (wx != 0ULL) {
                        const uint64_t x = config::bit_lsb(wx);
                        wx = config::bit_clear_lsb_u64(wx);
                        
                        int holders[4]{};
                        int hc = 0;
                        
                        if ((exa & x) != 0ULL) holders[hc++] = a;
                        if ((exb & x) != 0ULL) holders[hc++] = b;
                        if ((exc & x) != 0ULL) holders[hc++] = c;
                        if ((exd & x) != 0ULL) holders[hc++] = d;
                        
                        // Zjawisko musi wystÄ…piÄ‡ w minimum 2 komĂłrkach cyklu, by mĂłc celowaÄ‡ "poza"
                        if (hc < 2) continue;
                        
                        for (int t = 0; t < st.topo->nn; ++t) {
                            if (t == a || t == b || t == c || t == d) continue;
                            if (st.board->values[t] != 0) continue;
                            
                            // Target "T" musi widzieÄ‡ WSZYSTKIE wierzchoĹ‚ki cyklu zawierajÄ…ce "X"
                            bool sees_all = true;
                            for (int i = 0; i < hc; ++i) {
                                if (!st.is_peer(t, holders[i])) {
                                    sees_all = false;
                                    break;
                                }
                            }
                            if (!sees_all) continue;
                            
                            const ApplyResult er = st.eliminate(t, x);
                            if (er == ApplyResult::Contradiction) { 
                                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
                                return er; 
                            }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }

                    // Local probing for corner extras in SK layout.
                    const int corners[4] = {a, b, c, d};
                    const uint64_t extras[4] = {exa, exb, exc, exd};
                    for (int ci = 0; ci < 4; ++ci) {
                        uint64_t probe = extras[ci];
                        int tested = 0;
                        while (probe != 0ULL && tested < 2) {
                            const uint64_t bit = config::bit_lsb(probe);
                            probe = config::bit_clear_lsb_u64(probe);
                            ++tested;
                            const int digit = config::bit_ctz_u64(bit) + 1;
                            if (!sk_loop_probe_contradiction(st, corners[ci], digit, probe_steps, sp)) continue;
                            const ApplyResult er = st.eliminate(corners[ci], bit);
                            if (er == ApplyResult::Contradiction) {
                                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                                return er;
                            }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_sk_loop = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// SK LOOP (PeĹ‚ny silnik logiczny)
// Integruje Exact z mocÄ… gĹ‚Ä™bokiego zastÄ™pstwa silnikiem P7
// ============================================================================
inline ApplyResult apply_sk_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 5 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    StrategyStats tmp{};
    
    // 1. Twarda struktura Exact (szybka)
    const ApplyResult exact = apply_sk_loop_exact(st, tmp, r);
    if (exact == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return exact; 
    }
    if (exact == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_sk_loop = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // Adaptacyjne gĹ‚Ä™bokie skanowanie (P8) - Skokowa propagacja dla siatek
    const int depth_cap = std::clamp(8 + (st.board->empty_cells / std::max(1, st.topo->n)), 10, 14);
    bool used_dynamic = false;

    // 2. Szukanie zdeformowanych cykli przez Bounded Implication (Nishio Forcing)
    ApplyResult dyn = p7_nightmare::bounded_implication_core(st, s, r, depth_cap, used_dynamic);
    if (dyn == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn; 
    }
    if (dyn == ApplyResult::Progress && used_dynamic) {
        ++s.hit_count;
        r.used_sk_loop = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // 3. Fallback w poszukiwaniu klasycznej pÄ™tli Nice Loop
    ApplyResult ar = p7_nightmare::apply_continuous_nice_loop(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar; 
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_sk_loop = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // 4. Fallback X-Cycle dla uĹ‚oĹĽonych siatek
    ar = p7_nightmare::apply_grouped_x_cycle(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar; 
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_sk_loop = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical


