// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: house_subsets.h (Poziomy 2, 3, 4)
// Opis: Wykrywanie i aplikacja podzbiorów (Naked/Hidden Pairs, Triples, Quads).
//       Kompletnie zunifikowana funkcja zero-allocation używająca masek bitowych
//       i bitowych sum (union). 
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p3_subsets {

// Parametry:
// subset = 2 (Pair), 3 (Triple), 4 (Quad)
// hidden = true (szukamy na podstawie dystrybucji masek wewnątrz rzędu/kolumny/boxa)
//        = false (szukamy na podstawie pojemności pojedynczej komórki)
inline ApplyResult apply_house_subset(
    CandidateState& st, 
    StrategyStats& s, 
    GenericLogicCertifyResult& r, 
    int subset, 
    bool hidden) {
    
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    const int n = st.topo->n;
    bool progress = false;
    
    // Tablice na stosie (gwarantowane wsparcie N=64 bez heap-alloc)
    uint64_t pos[64]{};
    int cells[64]{};
    int active_digits[64]{};

    const size_t house_count = st.topo->house_offsets.size() - 1;
    for (size_t h = 0; h < house_count; ++h) {
        const int p0 = st.topo->house_offsets[h];
        const int p1 = st.topo->house_offsets[h + 1];
        
        // ====================================================================
        // TRYB: NAKED SUBSETS
        // ====================================================================
        if (!hidden) {
            int m = 0;
            // Szukamy komórek, które mają od 2 do N kandydatów
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[idx] != 0) continue;
                
                const int bits = std::popcount(st.cands[idx]);
                // Tylko komórki mające tyle lub mniej kandydatów co nasz docelowy podzbiór
                if (bits >= 2 && bits <= subset) {
                    if (m < 64) cells[m++] = idx;
                }
            }
            
            if (m < subset) continue;
            
            // Funkcja do weryfikacji i zastosowania Naked Subsetu
            auto try_apply_naked_union = [&](int a, int b, int c, int d) -> ApplyResult {
                uint64_t um = st.cands[cells[a]] | st.cands[cells[b]];
                if (c >= 0) um |= st.cands[cells[c]];
                if (d >= 0) um |= st.cands[cells[d]];
                
                if (std::popcount(um) != subset) return ApplyResult::NoProgress;
                
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    
                    // Omijamy komórki, które tworzą podzbiór
                    if (idx == cells[a] || idx == cells[b] ||
                        (c >= 0 && idx == cells[c]) ||
                        (d >= 0 && idx == cells[d])) {
                        continue;
                    }
                    
                    const ApplyResult er = st.eliminate(idx, um);
                    if (er == ApplyResult::Contradiction) return er;
                    progress = progress || (er == ApplyResult::Progress);
                }
                return ApplyResult::NoProgress;
            };

            // Permutacje dla N=2,3,4 bez używania n-silni
            for (int i = 0; i < m; ++i) {
                for (int j = i + 1; j < m; ++j) {
                    if (subset == 2) {
                        const ApplyResult rr = try_apply_naked_union(i, j, -1, -1);
                        if (rr == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return rr; }
                        continue;
                    }
                    for (int k = j + 1; k < m; ++k) {
                        if (subset == 3) {
                            const ApplyResult rr = try_apply_naked_union(i, j, k, -1);
                            if (rr == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return rr; }
                            continue;
                        }
                        for (int l = k + 1; l < m; ++l) {
                            const ApplyResult rr = try_apply_naked_union(i, j, k, l);
                            if (rr == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return rr; }
                        }
                    }
                }
            }
        } 
        // ====================================================================
        // TRYB: HIDDEN SUBSETS
        // ====================================================================
        else {
            std::fill_n(pos, n, 0ULL);
            
            // Mapowanie cyfra -> maska wystąpień (gdzie maska to pozycja komórki w "domku")
            for (int d = 1; d <= n; ++d) {
                const uint64_t bit = (1ULL << (d - 1));
                uint64_t bits = 0ULL;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (st.board->values[idx] != 0) continue;
                    
                    if ((st.cands[idx] & bit) != 0ULL) {
                        bits |= (1ULL << (p - p0));
                    }
                }
                pos[d - 1] = bits;
            }
            
            int ad_count = 0;
            for (int d = 1; d <= n; ++d) {
                const int cnt = std::popcount(pos[d - 1]);
                if (cnt >= 1 && cnt <= subset) {
                    if (ad_count < 64) active_digits[ad_count++] = d;
                }
            }
            
            if (ad_count < subset) continue;
            
            auto try_apply_hidden_union = [&](int d1, int d2, int d3, int d4) -> ApplyResult {
                uint64_t up = pos[d1 - 1] | pos[d2 - 1];
                uint64_t allowed = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
                
                if (d3 > 0) {
                    up |= pos[d3 - 1];
                    allowed |= (1ULL << (d3 - 1));
                }
                if (d4 > 0) {
                    up |= pos[d4 - 1];
                    allowed |= (1ULL << (d4 - 1));
                }
                
                if (std::popcount(up) != subset) return ApplyResult::NoProgress;
                
                // Redukujemy kandydatów do tylko dozwolonych w tych specyficznych komórkach
                for (uint64_t w = up; w != 0ULL; w &= (w - 1ULL)) {
                    const int b = config::bit_ctz_u64(w);
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p0 + b)];
                    
                    const ApplyResult rr = st.keep_only(idx, allowed);
                    if (rr == ApplyResult::Contradiction) return rr;
                    progress = progress || (rr == ApplyResult::Progress);
                }
                return ApplyResult::NoProgress;
            };

            for (int i = 0; i < ad_count; ++i) {
                const int d1 = active_digits[i];
                for (int j = i + 1; j < ad_count; ++j) {
                    const int d2 = active_digits[j];
                    if (subset == 2) {
                        const ApplyResult rr = try_apply_hidden_union(d1, d2, -1, -1);
                        if (rr == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return rr; }
                        continue;
                    }
                    for (int k = j + 1; k < ad_count; ++k) {
                        const int d3 = active_digits[k];
                        if (subset == 3) {
                            const ApplyResult rr = try_apply_hidden_union(d1, d2, d3, -1);
                            if (rr == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return rr; }
                            continue;
                        }
                        for (int l = k + 1; l < ad_count; ++l) {
                            const int d4 = active_digits[l];
                            const ApplyResult rr = try_apply_hidden_union(d1, d2, d3, d4);
                            if (rr == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return rr; }
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        if (!hidden && subset == 2) r.used_naked_pair = true;
        if (!hidden && subset == 3) r.used_naked_triple = true;
        if (!hidden && subset == 4) r.used_naked_quad = true;
        if (hidden && subset == 2) r.used_hidden_pair = true;
        if (hidden && subset == 3) r.used_hidden_triple = true;
        if (hidden && subset == 4) r.used_hidden_quad = true;
        
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p3_subsets