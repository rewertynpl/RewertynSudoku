// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: pattern_overlay.h (Poziom 8 - Theoretical)
// Opis: Algorytm Pattern Overlay Method (POM). Przeszukuje czÄ™Ĺ›ciowe, 
//       wolne od sprzecznoĹ›ci nakĹ‚adki dla pojedynczej cyfry i eliminuje 
//       kandydatĂłw, ktĂłrzy nigdy nie wejdÄ… w skĹ‚ad prawidĹ‚owego rozwiÄ…zania.
//       RozwiÄ…zanie Zero-Allocation operujÄ…ce na pĹ‚ytkim wirtualnym stosie.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

// DoĹ‚Ä…czamy wymagane moduły dla POM 
#include "msls.h"

namespace sudoku_hpc::logic::p8_theoretical {

inline bool pom_propagate_singles(CandidateState& st, int max_steps) {
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

inline bool pom_probe_candidate_contradiction(
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
    } else if (!pom_propagate_singles(st, max_steps)) {
        contradiction = true;
    }

    std::copy_n(sp.dyn_cands_backup, nn, st.cands);
    std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
    st.board->empty_cells = sp.dyn_empty_backup;
    return contradiction;
}

// ============================================================================
// WĹ‚aĹ›ciwy silnik sprawdzania nakĹ‚adek (Bounded Pattern Overlay Method)
// Operuje na pĹ‚ytkim drzewie DFS dla wybranej, najbardziej ograniczonej komĂłrki.
// ============================================================================
inline ApplyResult apply_pom_exact(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    auto& sp = shared::exact_pattern_scratchpad();
    const int nn = st.topo->nn;
    const int n = st.topo->n;
    if (st.board->empty_cells > (nn - 6 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int pivot_budget = std::clamp(4 + n / 3, 6, 12);
    int pivots[64]{};
    int pivot_cnt = 0;
    for (int pc = 2; pc <= 4; ++pc) {
        for (int idx = 0; idx < nn; ++idx) {
            if (pivot_cnt >= pivot_budget) break;
            if (st.board->values[idx] != 0) continue;
            if (std::popcount(st.cands[idx]) != pc) continue;
            pivots[pivot_cnt++] = idx;
        }
        if (pivot_cnt >= pivot_budget) break;
    }
    if (pivot_cnt == 0) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int hyp_steps = std::clamp(6 + n / 4, 8, 12);
    const int probe_steps = std::clamp(6 + n / 3, 8, 14);
    for (int pi = 0; pi < pivot_cnt; ++pi) {
        const int pivot = pivots[pi];
        const uint64_t pm = st.cands[pivot];
        if (pm == 0ULL) continue;

        std::copy_n(st.cands, nn, sp.dyn_cands_backup);
        std::copy_n(st.board->values.data(), nn, sp.dyn_values_backup);
        sp.dyn_empty_backup = st.board->empty_cells;

        uint64_t inter_cands[shared::ExactPatternScratchpad::MAX_NN]{};
        for (int i = 0; i < nn; ++i) inter_cands[i] = ~0ULL;
        uint64_t contradiction_mask = 0ULL;
        int valid_hyp = 0;

        int digit_budget = 0;
        for (uint64_t w = pm; w != 0ULL; w = config::bit_clear_lsb_u64(w)) {
            if (digit_budget >= 2) break;
            ++digit_budget;
            const uint64_t bit = config::bit_lsb(w);
            const int d = config::bit_ctz_u64(bit) + 1;

            std::copy_n(sp.dyn_cands_backup, nn, st.cands);
            std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
            st.board->empty_cells = sp.dyn_empty_backup;

            bool contradiction = false;
            if (!st.place(pivot, d)) {
                contradiction = true;
            } else if (!pom_propagate_singles(st, hyp_steps)) {
                contradiction = true;
            }

            if (contradiction) {
                contradiction_mask |= bit;
                continue;
            }

            ++valid_hyp;
            for (int i = 0; i < nn; ++i) {
                uint64_t cur = st.cands[i];
                if (st.board->values[i] != 0) {
                    cur = (1ULL << (st.board->values[i] - 1));
                }
                inter_cands[i] &= cur;
            }
        }

        std::copy_n(sp.dyn_cands_backup, nn, st.cands);
        std::copy_n(sp.dyn_values_backup, nn, st.board->values.data());
        st.board->empty_cells = sp.dyn_empty_backup;

        if (contradiction_mask != 0ULL) {
            const ApplyResult er = st.eliminate(pivot, contradiction_mask);
            if (er == ApplyResult::Contradiction) {
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return er;
            }
            if (er == ApplyResult::Progress) {
                ++s.hit_count;
                r.used_pattern_overlay_method = true;
                s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                return ApplyResult::Progress;
            }
        }

        // Intersection-driven candidates are additionally validated by
        // contradiction probing before elimination.
        if (valid_hyp >= 2) {
            int probe_budget = std::clamp(4 + n / 3, 6, 12);
            for (int idx = 0; idx < nn && probe_budget > 0; ++idx) {
                if (st.board->values[idx] != 0) continue;
                const uint64_t base = st.cands[idx];
                const uint64_t keep = inter_cands[idx] & base;
                if (keep == 0ULL) continue;
                uint64_t rm = base & ~keep;
                while (rm != 0ULL && probe_budget > 0) {
                    const uint64_t bit = config::bit_lsb(rm);
                    rm = config::bit_clear_lsb_u64(rm);
                    --probe_budget;
                    const int d = config::bit_ctz_u64(bit) + 1;
                    if (!pom_probe_candidate_contradiction(st, idx, d, probe_steps, sp)) continue;
                    const ApplyResult er = st.eliminate(idx, bit);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return er;
                    }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_pattern_overlay_method = true;
                        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
                        return ApplyResult::Progress;
                    }
                }
            }
        }
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

// ============================================================================
// GĹ‚Ăłwny kontroler Ĺ‚Ä…czÄ…cy nakĹ‚adanie masek z kompozycjÄ… MSLS i Ryb (P8)
// ============================================================================
inline ApplyResult apply_pattern_overlay_method(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = p7_nightmare::get_current_time_ns();
    ++s.use_count;
    
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (st.board->empty_cells > (nn - 7 * n)) {
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::NoProgress;
    }

    StrategyStats tmp{};
    
    // Krok 1: Aplikacja czystego POM (szybki overlay)
    const ApplyResult exact = apply_pom_exact(st, tmp, r);
    if (exact == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return exact; 
    }
    if (exact == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_pattern_overlay_method = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }
    
    // Krok 2: Adaptacyjne dynamiczne Ĺ›ledzenie zatorĂłw za pomocÄ… Ĺ‚aĹ„cuchĂłw (GĹ‚Ä™bokoĹ›Ä‡ P8)
    const int depth_cap = std::clamp(8 + (st.board->empty_cells / std::max(1, st.topo->n)), 10, 14);
    bool used_dynamic = false;

    ApplyResult dyn = p7_nightmare::bounded_implication_core(st, s, r, depth_cap, used_dynamic);
    if (dyn == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return dyn; 
    }
    if (dyn == ApplyResult::Progress && used_dynamic) {
        ++s.hit_count;
        r.used_pattern_overlay_method = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    // Krok 3: Kaskada z P8 - MSLS generuje wielkie nakĹ‚adki matryc
    ApplyResult ar = apply_msls(st, tmp, r);
    if (ar == ApplyResult::Contradiction) { 
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0; 
        return ar; 
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_pattern_overlay_method = true;
        s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += p7_nightmare::get_current_time_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p8_theoretical

