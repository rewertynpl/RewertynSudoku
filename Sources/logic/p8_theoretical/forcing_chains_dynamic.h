// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: forcing_chains_dynamic.h (Poziom 8 - Theoretical)
// Opis: Algorytmy Forcing Chains oraz Dynamic Forcing Chains.
//       Eksploracja zaĹ‚oĹĽeĹ„ z wymuszaniem i zagnieĹĽdĹĽonym Ĺ›rodowiskiem symulacji.
//       RozwiÄ…zanie Zero-Allocation operujÄ…ce na tablicach "zrzutĂłw" 
//       znajdujÄ…cych siÄ™ we wspĂłĹ‚dzielonym Scratchpadzie (do gĹ‚Ä™bokoĹ›ci N).
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

// DoĹ‚Ä…czamy komponent wykorzystywany do silnej weryfikacji powiÄ…zaĹ„ w DFC
#include "../p7_nightmare/aic_grouped_aic.h"

namespace sudoku_hpc::logic::p8_theoretical {

// ============================================================================
// Wymuszenia Dynamiczne (Dynamic Forcing Assumption)
// NajciÄ™ĹĽsza metoda (P8). Dokonuje "Nishio Probing" na wÄ™zĹ‚ach bivalue/trivalue.
// Tworzy gaĹ‚Ä™zie i weryfikuje ich stabilnoĹ›Ä‡ (bez peĹ‚nego drzewa DLX).
// ============================================================================
inline ApplyResult apply_dynamic_forcing_assumption(CandidateState& st, bool& used_flag) {
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    auto& sp = shared::exact_pattern_scratchpad();

    // Ograniczamy budĹĽet hipotez: najpierw bivalue, potem trivalue.
    const int pivot_budget = std::clamp(8 + (n / 2), 8, 40);
    const int max_steps = std::clamp(6 + (n / 3), 8, 20);
    int tested_pivots = 0;

    for (int pass_pc = 2; pass_pc <= 3; ++pass_pc) {
        for (int pivot = 0; pivot < nn; ++pivot) {
            if (tested_pivots >= pivot_budget) return ApplyResult::NoProgress;
            if (st.board->values[pivot] != 0) continue;
            const uint64_t mask = st.cands[pivot];
            const int pc = std::popcount(mask);
            if (pc != pass_pc) continue;
            ++tested_pivots;

            const int digit_budget = (pc == 2) ? 2 : 2;
            int tested_digits = 0;

            for (uint64_t w = mask; w != 0ULL; w &= (w - 1ULL)) {
                if (tested_digits >= digit_budget) break;
                ++tested_digits;

                const uint64_t test_bit = config::bit_lsb(w);
                const int test_digit = config::bit_ctz_u64(test_bit) + 1;

                std::copy_n(st.cands, nn, sp.dyn_cands_backup);
                std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
                sp.dyn_empty_backup = st.board->empty_cells;

                bool contradiction = false;
                if (!st.place(pivot, test_digit)) {
                    contradiction = true;
                } else {
                    int iters = 0;
                    bool changed = true;
                    while (changed && iters < max_steps) {
                        changed = false;
                        ++iters;
                        for (int idx = 0; idx < nn; ++idx) {
                            if (st.board->values[idx] != 0) continue;
                            const uint64_t m = st.cands[idx];
                            if (m == 0ULL) {
                                contradiction = true;
                                break;
                            }
                            const int sd = config::single_digit_from_mask(m);
                            if (sd == 0) continue;
                            if (!st.place(idx, sd)) {
                                contradiction = true;
                                break;
                            }
                            changed = true;
                        }
                        if (contradiction) break;
                    }
                }

                std::copy_n(sp.dyn_cands_backup, nn, st.cands);
                std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
                st.board->empty_cells = sp.dyn_empty_backup;

                if (!contradiction) continue;

                const ApplyResult er = st.eliminate(pivot, test_bit);
                if (er == ApplyResult::Contradiction) return er;
                if (er == ApplyResult::Progress) {
                    used_flag = true;
                    return ApplyResult::Progress;
                }
            }
        }
    }
    return ApplyResult::NoProgress;
}

// ============================================================================
// GĹĂ“WNY INTERFEJS Forcing Chains
// ============================================================================
inline ApplyResult apply_forcing_chains(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    // Heurystyka odcinajÄ…ca (Tylko w pĂłĹşnych fazach ma to sens)
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    bool used_dynamic = false;

    // Faza 1: Ograniczone hipotezy wymuszeĹ„ (Nishio-style).
    const ApplyResult ar_assumption = apply_dynamic_forcing_assumption(st, used_dynamic);
    
    if (ar_assumption == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar_assumption; 
    }
    if (ar_assumption == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // Faza 2: GĹ‚Ä™bokie zejĹ›cia AIC na grafie implikacji.
    const int depth_cap = std::clamp(14 + (st.board->empty_cells / std::max(1, st.topo->n)), 16, 28);
    
    const ApplyResult dyn = p7_nightmare::bounded_implication_core(st, s, r, depth_cap, used_dynamic);
    if (dyn == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn; 
    }
    if (dyn == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// DYNAMIC FORCING CHAINS
// Ewolucja zagnieĹĽdĹĽenia - wywoĹ‚anie jeszcze wiÄ™kszej iloĹ›ci wirtualnych BFS'Ăłw.
// ============================================================================
inline ApplyResult apply_dynamic_forcing_chains(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    // UĹĽywamy tego samego jÄ…dra co w klasycznym Forcing Chains, poniewaĹĽ
    // bounded_implication_core i dynamic_forcing_assumption operujÄ… na
    // grafach budowanych dynamicznie per-cyfra.
    StrategyStats tmp{};
    const ApplyResult dyn_exact = apply_forcing_chains(st, tmp, r);
    
    if (dyn_exact == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn_exact; 
    }
    if (dyn_exact == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_dynamic_forcing_chains = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical
