// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: als_xz.h (Poziom 6 - Diabolical)
// Opis: Algorytm ALS-XZ. Oparty o zbiory Almost Locked Sets.
//       Rozwiązuje dylematy węzłów ograniczonych silnymi powiązaniami, 
//       eliminując cyfry z rejonów przecinających widoczność.
//       Gwarancja zero-allocation dzięki wykorzystaniu ExactPatternScratchpad.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"
#include "../shared/als_builder.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline ApplyResult apply_als_xz(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    // Wyczyszczenie silnych powiązań na potrzeby Restriced Common (X)
    for (int d = 0; d <= n; ++d) {
        sp.strong_count[d] = 0;
    }
    
    // Budujemy listę powiązań silnych dla każdej cyfry.
    // Jeśli w którymkolwiek domku (house) dana cyfra występuje DOKŁADNIE 2 razy, 
    // mówimy, że te 2 komórki są spięte "Strong Linkiem".
    for (int d = 1; d <= n; ++d) {
        const uint64_t bit = (1ULL << (d - 1));
        
        for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
            const int p0 = st.topo->house_offsets[h];
            const int p1 = st.topo->house_offsets[h + 1];
            int a = -1;
            int b = -1;
            int cnt = 0;
            
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                
                if (cnt == 0) a = idx;
                else if (cnt == 1) b = idx;
                
                ++cnt;
                if (cnt > 2) break;
            }
            if (cnt == 2 && a >= 0 && b >= 0) {
                const int at = sp.strong_count[d];
                if (at < ExactPatternScratchpad::MAX_STRONG_LINKS_PER_DIGIT) {
                    sp.strong_a[d][at] = a;
                    sp.strong_b[d][at] = b;
                    ++sp.strong_count[d];
                }
            }
        }
    }

    // Budujemy globalną tablicę ALS dostępną dla aktualnej iteracji.
    // Ustawiamy limit 2-4 komórek, by nie utonąć w nieskończonych permutacji
    // gigantycznych plansz (co niszczyłoby Performance w 64x64).
    const int als_cnt = shared::build_als_list(st, 2, 4);
    if (als_cnt < 2) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    // Przeszukiwanie par ALS-ów (A i B)
    for (int i = 0; i < als_cnt; ++i) {
        const shared::ALS& a = sp.als_list[i];
        
        for (int j = i + 1; j < als_cnt; ++j) {
            const shared::ALS& b = sp.als_list[j];
            
            // Dwa ALS nie mogą mieć nakładających się komórek.
            bool overlapping = false;
            for (int w = 0; w < ((nn + 63) >> 6); ++w) {
                if ((a.cell_mask[w] & b.cell_mask[w]) != 0ULL) {
                    overlapping = true;
                    break;
                }
            }
            if (overlapping) continue;

            const uint64_t common = a.digit_mask & b.digit_mask;
            // Muszą współdzielić co najmniej 2 cyfry: "X" (Restricted Common) i "Z" (cel eliminacji)
            if (std::popcount(common) < 2) continue;

            // Iterujemy, zakładając każdą wspólną cyfrę jako "X"
            uint64_t wx = common;
            while (wx != 0ULL) {
                const uint64_t x = config::bit_lsb(wx);
                wx = config::bit_clear_lsb_u64(wx);
                
                const int xd = config::bit_ctz_u64(x) + 1;
                if (xd < 1 || xd > n) continue;

                // Aby "X" był Restricted Common, komórki ALS A i ALS B zawierające "X" muszą się "widzieć".
                // Najsilniejszym dowodem na to jest występowanie Strong Linka między nimi, lub po prostu
                // wzajemna widoczność w tym samym domku (house).
                bool x_is_restricted = false;
                
                // Sprawdzanie widoczności cyfry "X" między ALS A i B
                int a_x_cell = -1;
                int b_x_cell = -1;
                
                // Zlokalizuj komórkę z ALS 'A' zawierającą kandydata X
                for (int w = 0; w < ((nn + 63) >> 6) && a_x_cell == -1; ++w) {
                    uint64_t mask_w = a.cell_mask[w];
                    while (mask_w != 0ULL) {
                        const int cell_idx = (w << 6) + config::bit_ctz_u64(mask_w);
                        if ((st.cands[cell_idx] & x) != 0ULL) {
                            a_x_cell = cell_idx;
                            break;
                        }
                        mask_w = config::bit_clear_lsb_u64(mask_w);
                    }
                }
                
                // Zlokalizuj komórkę z ALS 'B' zawierającą kandydata X
                for (int w = 0; w < ((nn + 63) >> 6) && b_x_cell == -1; ++w) {
                    uint64_t mask_w = b.cell_mask[w];
                    while (mask_w != 0ULL) {
                        const int cell_idx = (w << 6) + config::bit_ctz_u64(mask_w);
                        if ((st.cands[cell_idx] & x) != 0ULL) {
                            b_x_cell = cell_idx;
                            break;
                        }
                        mask_w = config::bit_clear_lsb_u64(mask_w);
                    }
                }

                // Są w relacji zablokowanej jeśli po prostu się widzą (zatem oba nie mogą naraz przyjąć "X")
                if (a_x_cell >= 0 && b_x_cell >= 0 && st.is_peer(a_x_cell, b_x_cell)) {
                    x_is_restricted = true;
                }
                
                if (!x_is_restricted) continue;

                // Sukces: "X" jest zablokowane. Pozostałe wspólnie dzielone maski (Z) 
                // mogą służyć do uderzeń w planszę.
                const uint64_t zmask = common & ~x;
                uint64_t wz = zmask;
                
                while (wz != 0ULL) {
                    const uint64_t z = config::bit_lsb(wz);
                    wz = config::bit_clear_lsb_u64(wz);
                    
                    // Szukamy potencjalnej komórki docelowej do eliminacji 'Z'
                    for (int t = 0; t < nn; ++t) {
                        if (st.board->values[t] != 0) continue;
                        if ((st.cands[t] & z) == 0ULL) continue;
                        
                        // Nie możemy niszczyć macierzy wewnątrz naszych testowanych ALS-ów
                        if (shared::als_cell_in(a, t) || shared::als_cell_in(b, t)) continue;

                        // Target (t) musi widzieć WSZYSTKIE wystąpienia 'Z' w ALS A oraz w ALS B.
                        bool sees_all_a = true;
                        for (int w_a = 0; w_a < ((nn + 63) >> 6) && sees_all_a; ++w_a) {
                            uint64_t mask_w = a.cell_mask[w_a];
                            while (mask_w != 0ULL) {
                                const int cell_idx = (w_a << 6) + config::bit_ctz_u64(mask_w);
                                if ((st.cands[cell_idx] & z) != 0ULL && !st.is_peer(t, cell_idx)) {
                                    sees_all_a = false;
                                    break;
                                }
                                mask_w = config::bit_clear_lsb_u64(mask_w);
                            }
                        }
                        if (!sees_all_a) continue;

                        bool sees_all_b = true;
                        for (int w_b = 0; w_b < ((nn + 63) >> 6) && sees_all_b; ++w_b) {
                            uint64_t mask_w = b.cell_mask[w_b];
                            while (mask_w != 0ULL) {
                                const int cell_idx = (w_b << 6) + config::bit_ctz_u64(mask_w);
                                if ((st.cands[cell_idx] & z) != 0ULL && !st.is_peer(t, cell_idx)) {
                                    sees_all_b = false;
                                    break;
                                }
                                mask_w = config::bit_clear_lsb_u64(mask_w);
                            }
                        }
                        if (!sees_all_b) continue;
                        
                        // Eliminacja
                        const ApplyResult er = st.eliminate(t, z);
                        if (er == ApplyResult::Contradiction) { 
                            s.elapsed_ns += st.now_ns() - t0; 
                            return er; 
                        }
                        if (er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_als_xz = true;
                            progress = true;
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