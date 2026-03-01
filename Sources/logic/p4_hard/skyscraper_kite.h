// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: skyscraper_kite.h (Poziom 4)
// Opis: Algorytmy Skyscraper oraz 2-String Kite korzystające z połamanych 
//       powiązań x-wing. Oba wykorzystują Scratchpada do zero-allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p4_hard {

inline ApplyResult apply_skyscraper(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
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

        // Row Skyscraper
        for (int r1 = 0; r1 < n; ++r1) {
            const uint64_t m1 = sp.fish_row_masks[r1];
            if (std::popcount(m1) != 2) continue;
            
            for (int r2 = r1 + 1; r2 < n; ++r2) {
                const uint64_t m2 = sp.fish_row_masks[r2];
                if (std::popcount(m2) != 2) continue;
                
                const uint64_t common = m1 & m2;
                if (std::popcount(common) != 1) continue; // Wymaga dokładnie jednego wspólnego "dachu"
                
                const uint64_t e1 = m1 & ~common;
                const uint64_t e2 = m2 & ~common;
                if (e1 == 0ULL || e2 == 0ULL) continue;
                
                const int c1 = config::bit_ctz_u64(e1);
                const int c2 = config::bit_ctz_u64(e2);
                const int a = r1 * n + c1;
                const int b = r2 * n + c2;
                
                const int ap0 = st.topo->peer_offsets[a];
                const int ap1 = st.topo->peer_offsets[a + 1];
                
                for (int p = ap0; p < ap1; ++p) {
                    const int t = st.topo->peers_flat[p];
                    if (t == a || t == b) continue;
                    // Skyscraper uderza tam gdzie A widzi cel oraz B widzi cel
                    if (!st.is_peer(t, b)) continue;
                    
                    const ApplyResult er = st.eliminate(t, bit);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }

        // Col Skyscraper
        for (int c1 = 0; c1 < n; ++c1) {
            const uint64_t m1 = sp.fish_col_masks[c1];
            if (std::popcount(m1) != 2) continue;
            
            for (int c2 = c1 + 1; c2 < n; ++c2) {
                const uint64_t m2 = sp.fish_col_masks[c2];
                if (std::popcount(m2) != 2) continue;
                
                const uint64_t common = m1 & m2;
                if (std::popcount(common) != 1) continue;
                
                const uint64_t e1 = m1 & ~common;
                const uint64_t e2 = m2 & ~common;
                if (e1 == 0ULL || e2 == 0ULL) continue;
                
                const int r1 = config::bit_ctz_u64(e1);
                const int r2 = config::bit_ctz_u64(e2);
                const int a = r1 * n + c1;
                const int b = r2 * n + c2;
                
                const int ap0 = st.topo->peer_offsets[a];
                const int ap1 = st.topo->peer_offsets[a + 1];
                
                for (int p = ap0; p < ap1; ++p) {
                    const int t = st.topo->peers_flat[p];
                    if (t == a || t == b) continue;
                    if (!st.is_peer(t, b)) continue;
                    
                    const ApplyResult er = st.eliminate(t, bit);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_skyscraper = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_two_string_kite(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
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

        for (int row = 0; row < n; ++row) {
            const uint64_t rm = sp.fish_row_masks[row];
            if (std::popcount(rm) != 2) continue; // Wymaga silnego powiązania
            
            uint64_t ra = rm & (~rm + 1ULL);
            uint64_t rb = rm ^ ra;
            const int c1 = config::bit_ctz_u64(ra);
            const int c2 = config::bit_ctz_u64(rb);
            const int row_a = row * n + c1;
            const int row_b = row * n + c2;

            for (int col = 0; col < n; ++col) {
                const uint64_t cm = sp.fish_col_masks[col];
                if (std::popcount(cm) != 2) continue; // Wymaga drugiego silnego powiązania
                
                uint64_t ca = cm & (~cm + 1ULL);
                uint64_t cb = cm ^ ca;
                const int r1 = config::bit_ctz_u64(ca);
                const int r2 = config::bit_ctz_u64(cb);
                const int col_a = r1 * n + col;
                const int col_b = r2 * n + col;

                struct Choice {
                    int row_pivot;
                    int row_end;
                    int col_pivot;
                    int col_end;
                };
                
                // Sprawdzamy wszystkie warianty połączeń między silnym rzędem i kolumną
                const std::array<Choice, 4> choices = {{
                    {row_a, row_b, col_a, col_b},
                    {row_a, row_b, col_b, col_a},
                    {row_b, row_a, col_a, col_b},
                    {row_b, row_a, col_b, col_a},
                }};
                
                for (const auto& ch : choices) {
                    if (ch.row_pivot == ch.col_pivot) continue;
                    // Oba złączenia muszą być w tym samym bloku by utworzyć 2-String Kite
                    if (st.topo->cell_box[ch.row_pivot] != st.topo->cell_box[ch.col_pivot]) continue;
                    
                    // Uderzenie eliminacji tam, gdzie końcówki widzą wspólną komórkę
                    const int p0 = st.topo->peer_offsets[ch.row_end];
                    const int p1 = st.topo->peer_offsets[ch.row_end + 1];
                    for (int p = p0; p < p1; ++p) {
                        const int t = st.topo->peers_flat[p];
                        if (t == ch.row_end || t == ch.col_end || t == ch.row_pivot || t == ch.col_pivot) continue;
                        if (!st.is_peer(t, ch.col_end)) continue;
                        
                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_two_string_kite = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p4_hard