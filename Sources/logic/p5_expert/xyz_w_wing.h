// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: xyz_w_wing.h (Poziom 5 - Expert)
// Opis: Implementacja strategii skrzydeł (Wings) dla Poziomu 5:
//       - XYZ-Wing: trójelementowy pivot (XYZ) + dwa bivalue.
//       - W-Wing: 2x bivalue sprzęgnięte oddalonym "Strong Linkiem".
//       System Zero-Allocation.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p5_expert {

inline ApplyResult apply_xyz_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    bool progress = false;

    // XYZ-Wing szuka w pierwszej kolejności Pivota (węzła o równej liczbie 3 kandydatów, np. {X, Y, Z})
    for (int pivot = 0; pivot < st.topo->nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;
        const uint64_t mp = st.cands[pivot];
        if (std::popcount(mp) != 3) continue;

        // Szukamy pierwszego skrzydła widocznego dla Pivota
        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        
        for (int i = p0; i < p1; ++i) {
            const int a = st.topo->peers_flat[i];
            if (st.board->values[a] != 0) continue;
            
            const uint64_t ma = st.cands[a];
            // Skrzydło musi być Bivalue i być podzbiorem masek Pivota (np. {X, Z})
            if (std::popcount(ma) != 2 || (ma & ~mp) != 0ULL) continue;

            // Szukamy drugiego skrzydła widocznego dla Pivota
            for (int j = i + 1; j < p1; ++j) {
                const int b = st.topo->peers_flat[j];
                if (st.board->values[b] != 0) continue;
                
                const uint64_t mb = st.cands[b];
                // Drugie skrzydło również musi być podzbiorem (np. {Y, Z})
                if (std::popcount(mb) != 2 || (mb & ~mp) != 0ULL) continue;

                // Razem, A i B muszą rekonstruować całą maskę MP Pivota (np. XZ | YZ = XYZ)
                if ((ma | mb) != mp) continue;
                
                // Element "Z" do wyeliminowania to część wspólna masek obu skrzydeł (np. Z z {X, Z} i {Y, Z})
                const uint64_t z = ma & mb;
                if (std::popcount(z) != 1) continue;

                // Szukamy targetu: Komórka, z której będziemy uderzać (eliminować), musi 
                // widzieć JEDNOCZEŚNIE pivota oraz OBA skrzydła (w przeciwieństwie do Y-Wing).
                const int ap0 = st.topo->peer_offsets[a];
                const int ap1 = st.topo->peer_offsets[a + 1];
                for (int p = ap0; p < ap1; ++p) {
                    const int t = st.topo->peers_flat[p];
                    if (t == pivot || t == a || t == b) continue;
                    
                    if (!st.is_peer(t, b)) continue;
                    if (!st.is_peer(t, pivot)) continue;
                    
                    const ApplyResult er = st.eliminate(t, z);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_xyz_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_w_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    const int n = st.topo->n;
    bool progress = false;
    
    auto& sp = shared::exact_pattern_scratchpad();

    // 1. Zbudowanie bufora silnych powiązań (Strong Links) per cyfra d.
    // Używamy zintegrowanych tablic 2D (zero alloc) z shared/exact_pattern_scratchpad.
    for (int d = 1; d <= n; ++d) {
        sp.strong_count[d] = 0;
        const uint64_t bit = (1ULL << (d - 1));
        
        for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
            const int p0 = st.topo->house_offsets[h];
            const int p1 = st.topo->house_offsets[h + 1];
            int a = -1, b = -1, cnt = 0;
            
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[p];
                if (st.board->values[idx] != 0) continue;
                if ((st.cands[idx] & bit) == 0ULL) continue;
                
                if (cnt == 0) a = idx;
                else if (cnt == 1) b = idx;
                
                ++cnt;
                if (cnt > 2) break;
            }
            // Jeśli występuje tylko 2 razy - silne powiązanie (Strong Link)
            if (cnt == 2 && a >= 0 && b >= 0) {
                int at = sp.strong_count[d];
                if (at < ExactPatternScratchpad::MAX_STRONG_LINKS_PER_DIGIT) {
                    sp.strong_a[d][at] = a;
                    sp.strong_b[d][at] = b;
                    ++sp.strong_count[d];
                }
            }
        }
    }

    // 2. Zebranie komórek bivalue w płaską listę, aby nie robić N^2 skanów
    sp.als_cell_count = 0; 
    for (int idx = 0; idx < st.topo->nn; ++idx) {
        if (st.board->values[idx] != 0) continue;
        if (std::popcount(st.cands[idx]) == 2) {
            sp.als_cells[sp.als_cell_count++] = idx;
        }
    }

    const int bn = sp.als_cell_count;
    for (int i = 0; i < bn; ++i) {
        const int a = sp.als_cells[i];
        const uint64_t ma = st.cands[a];
        
        for (int j = i + 1; j < bn; ++j) {
            const int b = sp.als_cells[j];
            
            // W-Wing dotyczy komórek o identycznych maskach, które się NIE widzą
            if (st.is_peer(a, b)) continue;
            const uint64_t mb = st.cands[b];
            
            if (ma != mb) continue;
            
            uint64_t bit1 = ma & (~ma + 1ULL);
            uint64_t bit2 = ma ^ bit1;
            
            const std::array<uint64_t, 2> z_bits = {bit1, bit2};
            
            // Próbujemy spiąć cyfrą 'z', by dokonać eliminacji drugiej cyfry ('other')
            for (const uint64_t z : z_bits) {
                if (z == 0ULL) continue;
                
                const uint64_t other = ma ^ z;
                const int zd = config::bit_ctz_u64(z) + 1;
                if (zd < 1 || zd > n) continue;
                
                bool linked = false;
                // Sprawdzenie powiązań z bufora zero-alloc
                for (int li = 0; li < sp.strong_count[zd]; ++li) {
                    const int u = sp.strong_a[zd][li];
                    const int v = sp.strong_b[zd][li];
                    
                    // Most (Bridge) wymaga silnego powiązania, którego dwa końce 
                    // widzą jedno i drugie ramię bivalue.
                    if ((st.is_peer(a, u) && st.is_peer(b, v)) ||
                        (st.is_peer(a, v) && st.is_peer(b, u))) {
                        linked = true;
                        break;
                    }
                }
                
                if (!linked) continue;
                
                // Dokonano dowodu łączności W-Winga, redukujemy 'other' ze stref skrzyżowania A i B.
                const int p0 = st.topo->peer_offsets[a];
                const int p1 = st.topo->peer_offsets[a + 1];
                for (int p = p0; p < p1; ++p) {
                    const int t = st.topo->peers_flat[p];
                    if (t == a || t == b) continue;
                    
                    if (!st.is_peer(t, b)) continue; // Tylko intersekcje
                    
                    const ApplyResult er = st.eliminate(t, other);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += st.now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_w_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p5_expert