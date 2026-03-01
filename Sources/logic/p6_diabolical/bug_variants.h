// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: bug_variants.h (Poziom 6 - Diabolical)
// Opis: Implementacja zaawansowanych wariantów Bivalue Universal Grave 
//       (BUG Type 2, BUG Type 3, BUG Type 4). Służą do eliminacji kandydatów 
//       na krawędzi Deadly Pattern'u, gdy występuje więcej niż jedna komórka 
//       typu trivalue.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p6_diabolical {

// ============================================================================
// BUG Type 2
// Szuka dwóch komórek (trivalue), które mają identyczne maski i się "widzą" 
// na planszy opanowanej przez komórki bivalue. Współdzielona "trzecia" cyfra 
// zostaje usunięta z ich wspólnej strefy widzenia.
// ============================================================================
inline ApplyResult apply_bug_type2(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Heurystyka odrzucająca wczesny etap (BUG wymaga dużej gęstości planszy)
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int nn = st.topo->nn;
    int tri[64]{};
    int tc = 0;
    
    // Filtrujemy planszę w poszukiwaniu komórek trivalue
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 3) {
            if (tc < 64) tri[tc++] = idx;
        }
    }
    
    bool progress = false;
    for (int i = 0; i < tc; ++i) {
        const int a = tri[i];
        const uint64_t ma = st.cands[a];
        
        for (int j = i + 1; j < tc; ++j) {
            const int b = tri[j];
            if (!st.is_peer(a, b)) continue; // Muszą być we wspólnym "domku"
            
            const uint64_t mb = st.cands[b];
            const uint64_t shared = ma & mb;
            
            // Warunek: Dwie wspólne "oryginalne" cyfry BUG-a, oraz trzecia "obca"
            if (std::popcount(shared) != 2) continue;
            
            // "Obca" cyfra z poza pary shared to nasza cyfra X. Musi zostać w A lub B.
            // BUG Type 2 uderza we wspólne cele tych dwóch komórek i usuwa stamtąd cyfrę X.
            // Dla ułatwienia operacji, w C++ "st.keep_only(..., shared)" eliminuje wszystkie
            // INNE cyfry (w tym cyfrę X) dla komórek widzących a i b, które nie są częścią BUG'a.
            
            // Jednak to nie jest w 100% bezpieczne dla asymetrycznych siatek bez głębokiej walidacji.
            // Bardziej ortodoksyjna wersja Type 2: My wymuszamy, że cyfra X musi być 
            // prawdziwa dla A lub dla B. Wobec tego każdy 'target' widzący i A i B 
            // nie może być X.
            
            uint64_t xa = ma & ~shared;
            uint64_t xb = mb & ~shared;
            
            if (xa == 0ULL || xa != xb) continue; // Cyfra wykraczająca poza układ BUG musi być TĄ SAMĄ
            
            const uint64_t extra_digit = xa;
            
            // Eliminacja z intersection A i B
            const int p0 = st.topo->peer_offsets[a];
            const int p1 = st.topo->peer_offsets[a + 1];
            for (int p = p0; p < p1; ++p) {
                const int t = st.topo->peers_flat[p];
                if (t == a || t == b) continue;
                if (!st.is_peer(t, b)) continue; // Musi widzieć oba węzły
                
                const ApplyResult er = st.eliminate(t, extra_digit);
                if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                progress = progress || (er == ApplyResult::Progress);
            }
        }
    }
    
    if (progress) {
        ++s.hit_count;
        r.used_bug_type2 = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}


// ============================================================================
// BUG Type 3
// Przekłada poszukiwanie z komórek na "domki". Jeżeli te same trivalue cells 
// dzielą wspólny domek i ich wspólne unikalne cyfry występują w tym domku tylko
// w tych specyficznych komórkach, redukuje bivalues.
// (Uproszczona, zoptymalizowana hybryda skanu dla HPC).
// ============================================================================
inline ApplyResult apply_bug_type3(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int n = st.topo->n;
    const int nn = st.topo->nn;
    int tri[64]{};
    int tc = 0;
    
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 3 && tc < 64) {
            tri[tc++] = idx;
        }
    }
    if (tc == 0) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    for (int ti = 0; ti < tc; ++ti) {
        const int idx = tri[ti];
        const int rr = st.topo->cell_row[idx];
        const int cc = st.topo->cell_col[idx];
        const int bb = st.topo->cell_box[idx];
        const uint64_t m = st.cands[idx];
        
        uint64_t w = m;
        while (w != 0ULL) {
            const uint64_t bit = config::bit_lsb(w);
            w = config::bit_clear_lsb_u64(w);
            
            int row_cnt = 0;
            int col_cnt = 0;
            int box_cnt = 0;
            
            for (int c = 0; c < n; ++c) {
                const int t = rr * n + c;
                if (st.board->values[t] == 0 && (st.cands[t] & bit) != 0ULL) ++row_cnt;
            }
            for (int r0 = 0; r0 < n; ++r0) {
                const int t = r0 * n + cc;
                if (st.board->values[t] == 0 && (st.cands[t] & bit) != 0ULL) ++col_cnt;
            }
            for (int t = 0; t < nn; ++t) {
                if (st.topo->cell_box[t] != bb) continue;
                if (st.board->values[t] == 0 && (st.cands[t] & bit) != 0ULL) ++box_cnt;
            }
            
            // Warunek rozwiązujący "True Value" paradoksu
            if ((row_cnt & 1) == 1 && (col_cnt & 1) == 1 && (box_cnt & 1) == 1) {
                const int d = config::bit_ctz_u64(bit) + 1;
                if (!st.place(idx, d)) {
                    s.elapsed_ns += st.now_ns() - t0;
                    return ApplyResult::Contradiction;
                }
                
                ++s.hit_count;
                ++s.placements;
                ++r.steps;
                r.used_bug_type3 = true;
                
                s.elapsed_ns += st.now_ns() - t0;
                return ApplyResult::Progress;
            }
        }
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}


// ============================================================================
// BUG Type 4
// Skomplikowane powiązanie krzyżowe między dwiema komórkami trivalue, 
// bazujące na wspólnym kandydującym duecie połączonym ze słabym ogniwem.
// ============================================================================
inline ApplyResult apply_bug_type4(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    if (st.board->empty_cells > (st.topo->nn - st.topo->n)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int nn = st.topo->nn;
    int tri[64]{};
    int tc = 0;
    
    for (int idx = 0; idx < nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 3 && tc < 64) {
            tri[tc++] = idx;
        }
    }
    
    bool progress = false;
    for (int i = 0; i < tc; ++i) {
        const int a = tri[i];
        const uint64_t ma = st.cands[a];
        
        for (int j = i + 1; j < tc; ++j) {
            const int b = tri[j];
            const uint64_t mb = st.cands[b];
            
            // Szukamy pary trivalue współdzielącej wyłącznie jeden, specyficzny "słaby" punkt
            const uint64_t common = ma & mb;
            if (std::popcount(common) != 1) continue;
            
            // Muszą się widzieć z oboma trivalue węzłami
            for (int t = 0; t < nn; ++t) {
                if (t == a || t == b) continue;
                if (st.board->values[t] != 0) continue;
                if (!st.is_peer(t, a) || !st.is_peer(t, b)) continue;
                
                // Redukcja wspólnego ułamka z targetu
                const ApplyResult er = st.eliminate(t, common);
                if (er == ApplyResult::Contradiction) { 
                    s.elapsed_ns += st.now_ns() - t0; 
                    return er; 
                }
                progress = progress || (er == ApplyResult::Progress);
            }
        }
    }
    
    if (progress) {
        ++s.hit_count;
        r.used_bug_type4 = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical