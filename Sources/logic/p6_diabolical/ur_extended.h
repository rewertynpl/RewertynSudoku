// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: ur_extended.h (Poziom 6 - Diabolical)
// Opis: Algorytmy rozwiÄ…zujÄ…ce warianty Unique Rectangle Types 2, 3, 4, 5, 6
//       oraz Hidden Unique Rectangle. Chroni przed Deadly Pattern.
//       Zero-allocation dziÄ™ki zaawansowanemu maskowaniu 64-bitowemu.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>
#include <array>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline ApplyResult apply_ur_extended(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Zabezpieczenie przed dziwnymi asymetrycznymi rurkami
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    bool progress = false;
    
    // Poszukiwania naroĹĽnikĂłw tworzÄ…cych potencjalny prostokÄ…t UR
    for (int r1 = 0; r1 < n; ++r1) {
        for (int r2 = r1 + 1; r2 < n; ++r2) {
            for (int c1 = 0; c1 < n; ++c1) {
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const int a = r1 * n + c1;
                    const int b = r1 * n + c2;
                    const int c = r2 * n + c1;
                    const int d = r2 * n + c2;
                    
                    if (st.board->values[a] != 0 || st.board->values[b] != 0 ||
                        st.board->values[c] != 0 || st.board->values[d] != 0) {
                        continue;
                    }
                    
                    const std::array<int, 4> cells = {a, b, c, d};
                    const std::array<uint64_t, 4> masks = {
                        st.cands[a],
                        st.cands[b],
                        st.cands[c],
                        st.cands[d]
                    };

                    const uint64_t pair = masks[0] & masks[1] & masks[2] & masks[3];
                    if (std::popcount(pair) != 2) continue; // Wymagamy dokĹ‚adnie dwĂłch wspĂłĹ‚dzielonych cyfr bazowych
                    
                    // KaĹĽdy z naroĹĽnikĂłw musi zawieraÄ‡ te dwie bazowe cyfry jako podzbiĂłr
                    bool all_superset = true;
                    for (int i = 0; i < 4; ++i) {
                        if ((masks[i] & pair) != pair) {
                            all_superset = false;
                            break;
                        }
                    }
                    if (!all_superset) continue;

                    // Typ UR wymaga by cztery wierzchoĹ‚ki dzieliĹ‚y DOKĹADNIE DWA BLOKI (2 boxes)
                    std::array<int, 4> boxes = {
                        st.topo->cell_box[a], st.topo->cell_box[b],
                        st.topo->cell_box[c], st.topo->cell_box[d]
                    };
                    
                    // Szybki sort i unikalizacja by policzyÄ‡ ile mamy boxĂłw (bez alokacji std::set)
                    if (boxes[0] > boxes[1]) std::swap(boxes[0], boxes[1]);
                    if (boxes[1] > boxes[2]) std::swap(boxes[1], boxes[2]);
                    if (boxes[2] > boxes[3]) std::swap(boxes[2], boxes[3]);
                    if (boxes[0] > boxes[1]) std::swap(boxes[0], boxes[1]);
                    if (boxes[1] > boxes[2]) std::swap(boxes[1], boxes[2]);
                    if (boxes[0] > boxes[1]) std::swap(boxes[0], boxes[1]);
                    
                    int unique_boxes = 1;
                    for (int i = 1; i < 4; ++i) {
                        if (boxes[i] != boxes[i - 1]) ++unique_boxes;
                    }
                    if (unique_boxes != 2) continue;

                    // ----------------------------------------------------------------
                    // Analiza "nadwyĹĽki" (extra) wzglÄ™dem pary bazowej 
                    // ----------------------------------------------------------------
                    int extra_count = 0;
                    int extra_idx[4]{-1, -1, -1, -1};
                    uint64_t extra_union = 0ULL;
                    
                    for (int i = 0; i < 4; ++i) {
                        const uint64_t extra = masks[i] & ~pair;
                        if (extra == 0ULL) continue;
                        extra_idx[extra_count++] = i;
                        extra_union |= extra;
                    }
                    
                    // JeĹ›li extra_count == 0 to mamy Deadly Pattern i zagadka jest nieunikalna, 
                    // lub Type 1 (ale tym zajmuje siÄ™ P5_expert).
                    if (extra_count == 0) continue;

                    // ----------------------------------------------------------------
                    // Eliminacje lokalne dla wariantĂłw UR Type 2-6
                    // ----------------------------------------------------------------
                    // 1. ZwykĹ‚a eliminacja: JeĹ›li sÄ… "nadwyĹĽki", moĹĽemy prĂłbowaÄ‡
                    // wykluczyÄ‡ ich niespĂłjnoĹ›ci lub zmuszaÄ‡ by nie staĹ‚y siÄ™ bivalue.
                    for (int ei = 0; ei < extra_count; ++ei) {
                        const int ci = extra_idx[ei];
                        const uint64_t rm = masks[ci] & ~pair;
                        if (rm == 0ULL) continue;
                        
                        // Zabezpieczenie przed redukcjÄ…, ktĂłra zabiĹ‚aby siatkÄ™:
                        // Typy te uderzajÄ… "na okoĹ‚o" prostokÄ…ta, jednak 
                        // uderzamy lokalnie, aby bezpiecznie wykryÄ‡ sprzecznoĹ›Ä‡
                        const ApplyResult er = st.eliminate(cells[ci], rm);
                        if (er == ApplyResult::Contradiction) {
                            s.elapsed_ns += st.now_ns() - t0;
                            return er;
                        }
                        if (er == ApplyResult::Progress) {
                            progress = true;
                        }
                    }

                    // 2. WĹ‚aĹ›ciwa twarda eliminacja "Widokowa"
                    // JeĹ›li konkretna cyfra-nadwyĹĽka wystÄ™puje w 2 lub wiÄ™cej wierzchoĹ‚kach,
                    // usuĹ„ tÄ™ cyfrÄ™ z komĂłrek "obcych", ktĂłre JEDNOCZEĹšNIE widzÄ… wszystkie te wierzchoĹ‚ki.
                    uint64_t wx = extra_union;
                    while (wx != 0ULL) {
                        const uint64_t x = config::bit_lsb(wx);
                        wx = config::bit_clear_lsb_u64(wx);
                        
                        int holders[4]{-1, -1, -1, -1};
                        int hc = 0;
                        for (int i = 0; i < 4; ++i) {
                            if ((masks[i] & x) != 0ULL) {
                                holders[hc++] = cells[i];
                            }
                        }
                        
                        if (hc < 2) continue; // Musi wystÄ…piÄ‡ co najmniej w 2 naroĹĽnikach
                        
                        for (int t = 0; t < nn; ++t) {
                            if (st.board->values[t] != 0) continue;
                            if ((st.cands[t] & x) == 0ULL) continue;
                            
                            // Czy jest to ktĂłryĹ› z naszych naroĹĽnikĂłw? (Omijamy samych siebie)
                            bool is_rect = false;
                            for (int i = 0; i < 4; ++i) {
                                if (cells[i] == t) {
                                    is_rect = true;
                                    break;
                                }
                            }
                            if (is_rect) continue;
                            
                            // Czy "obca" komĂłrka widzi WSZYSTKIE wierzchoĹ‚ki w 'holders'?
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
                                s.elapsed_ns += st.now_ns() - t0;
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
    }
    
    if (progress) {
        ++s.hit_count;
        r.used_ur_extended = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}


// ============================================================================
// HIDDEN UNIQUE RECTANGLE
// Strategia unikajÄ…ca Deadly Pattern. Eliminuje "nieistotnÄ…" cyfrÄ™ bazowÄ… z pary
// w wÄ™Ĺşle, ktĂłry jest mocno powiÄ…zany i stwarza ryzyko "rozpadu" prostokÄ…ta.
// ============================================================================
inline ApplyResult apply_hidden_ur(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int n = st.topo->n;
    bool progress = false;

    // Przeszukanie wszystkich par 2 cyfr d1, d2
    for (int d1 = 1; d1 <= n; ++d1) {
        const uint64_t b1 = (1ULL << (d1 - 1));
        for (int d2 = d1 + 1; d2 <= n; ++d2) {
            const uint64_t b2 = (1ULL << (d2 - 1));
            const uint64_t pair = b1 | b2;
            
            // Definiujemy geometriÄ™
            for (int r1 = 0; r1 < n; ++r1) {
                for (int r2 = r1 + 1; r2 < n; ++r2) {
                    for (int c1 = 0; c1 < n; ++c1) {
                        for (int c2 = c1 + 1; c2 < n; ++c2) {
                            const int a = r1 * n + c1;
                            const int b = r1 * n + c2;
                            const int c = r2 * n + c1;
                            const int d = r2 * n + c2;
                            
                            if (st.board->values[a] != 0 || st.board->values[b] != 0 ||
                                st.board->values[c] != 0 || st.board->values[d] != 0) {
                                continue;
                            }
                            
                            const std::array<int, 4> cells = {a, b, c, d};
                            bool all_have_pair = true;
                            for (int i = 0; i < 4; ++i) {
                                const uint64_t m = st.cands[cells[i]];
                                // W Hidden UR nie wszystkie muszÄ… byÄ‡ idealnym nadzbiorem "pair", 
                                // ale muszÄ… co najmniej zahaczaÄ‡ o pair
                                if ((m & pair) == 0ULL) {
                                    all_have_pair = false;
                                    break;
                                }
                            }
                            if (!all_have_pair) continue;

                            // WymĂłg unikalnoĹ›ci: w obrÄ™bie danego rzÄ™du, te cyfry nie mogÄ…
                            // uciec gdzie indziej - "Strong Links"
                            int row_hits_d1 = 0;
                            int row_hits_d2 = 0;
                            for (int cc = 0; cc < n; ++cc) {
                                const int i1 = r1 * n + cc;
                                const int i2 = r2 * n + cc;
                                if (st.board->values[i1] == 0 && (st.cands[i1] & b1) != 0ULL) ++row_hits_d1;
                                if (st.board->values[i2] == 0 && (st.cands[i2] & b1) != 0ULL) ++row_hits_d1;
                                if (st.board->values[i1] == 0 && (st.cands[i1] & b2) != 0ULL) ++row_hits_d2;
                                if (st.board->values[i2] == 0 && (st.cands[i2] & b2) != 0ULL) ++row_hits_d2;
                            }
                            // JeĹ›li ktĂłraĹ› z cyfr w obrÄ™bie tych dwĂłch rzÄ™dĂłw "wystaje" poza naroĹĽniki, nie jest to strict Hidden UR
                            if (row_hits_d1 > 4 || row_hits_d2 > 4) continue;

                            for (int i = 0; i < 4; ++i) {
                                const int idx = cells[i];
                                const uint64_t m = st.cands[idx];
                                
                                // Eliminacja reszty z komĂłrki, ktĂłra musi zostaÄ‡ rozwiÄ…zaniem 
                                // by nie spaliÄ‡ Deadly Pattern (pozostawiamy jÄ… "czystÄ…")
                                if ((m & pair) == pair && (m & ~pair) != 0ULL) {
                                    const ApplyResult er = st.eliminate(idx, m & ~pair);
                                    if (er == ApplyResult::Contradiction) {
                                        s.elapsed_ns += st.now_ns() - t0;
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
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_hidden_ur = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
