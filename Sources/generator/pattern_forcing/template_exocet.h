// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// ModuĹ‚: template_exocet.h
// Opis: Generuje rygorystyczny matematyczny szablon dla strategii Exocet
//       wsparcia asymetrycznych geometrii NxN. Zero-allocation.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <array>
#include <cstdint>
#include <random>

// ZaleĹĽnoĹ›ci do gĹ‚Ăłwnej struktury topologii (zakĹ‚adajÄ…c Ĺ›cieĹĽkÄ™ do core/board.h)
#include "../../core/board.h"

namespace sudoku_hpc::pattern_forcing {

// WspĂłĹ‚dzielona struktura planu wstrzykiwania (Zero-Allocation na stosie)
struct ExactPatternTemplatePlan {
    bool valid = false;
    bool explicit_skeleton = false;
    int anchor_count = 0;
    std::array<int, 64> anchor_idx{};
    std::array<uint64_t, 64> anchor_masks{};
    int skeleton_count = 0;
    std::array<int, 256> skeleton_idx{};
    std::array<uint64_t, 256> skeleton_masks{};

    inline uint64_t merge_mask(uint64_t lhs, uint64_t rhs) const {
        if (lhs == 0ULL) return rhs;
        if (rhs == 0ULL) return lhs;
        return lhs & rhs;
    }

    inline int find_anchor(int idx) const {
        for (int i = 0; i < anchor_count; ++i) {
            if (anchor_idx[static_cast<size_t>(i)] == idx) return i;
        }
        return -1;
    }

    inline int find_skeleton(int idx) const {
        for (int i = 0; i < skeleton_count; ++i) {
            if (skeleton_idx[static_cast<size_t>(i)] == idx) return i;
        }
        return -1;
    }

    inline bool add_skeleton(int idx, uint64_t mask) {
        if (idx < 0) return false;
        const int existing = find_skeleton(idx);
        if (existing >= 0) {
            const uint64_t merged = merge_mask(
                skeleton_masks[static_cast<size_t>(existing)], mask);
            if (merged == 0ULL) return false;
            skeleton_masks[static_cast<size_t>(existing)] = merged;
            return true;
        }
        if (skeleton_count >= static_cast<int>(skeleton_idx.size())) return false;
        skeleton_idx[static_cast<size_t>(skeleton_count)] = idx;
        skeleton_masks[static_cast<size_t>(skeleton_count)] = mask;
        ++skeleton_count;
        return true;
    }

    // Szybkie dodawanie komĂłrki "zakotwiczonej" z rygorystycznÄ… maskÄ…
    inline bool add_anchor(int idx, uint64_t mask) {
        if (idx < 0) return false;
        const int existing = find_anchor(idx);
        if (existing >= 0) {
            const uint64_t merged = merge_mask(
                anchor_masks[static_cast<size_t>(existing)], mask);
            if (merged == 0ULL) return false;
            anchor_masks[static_cast<size_t>(existing)] = merged;
            add_skeleton(idx, merged);
            return true;
        }
        if (anchor_count >= static_cast<int>(anchor_idx.size())) return false;
        anchor_idx[static_cast<size_t>(anchor_count)] = idx;
        anchor_masks[static_cast<size_t>(anchor_count)] = mask;
        ++anchor_count;
        add_skeleton(idx, mask);
        return true;
    }
};

// Generowanie peĹ‚nej maski bitowej dla danego N
inline uint64_t pf_full_mask_for_n(int n) {
    return (n >= 64) ? ~0ULL : ((1ULL << n) - 1ULL);
}

class TemplateExocet {
public:
    // Wstrzykuje ukĹ‚ad Base Cells i Target Cells charakterystyczny dla Exoceta.
    // Zmusza DLX solver do wybudowania reszty planszy "wokĂłĹ‚" tego szablonu.
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool senior_mode = false) {
        plan = {}; // Reset struktury
        plan.explicit_skeleton = true;

        // Exocet matematycznie wymaga podziaĹ‚u pudeĹ‚ek na minimum 2x2.
        if (topo.box_rows <= 1 || topo.box_cols <= 1) {
            return false;
        }

        const int n = topo.n;
        const uint64_t full = pf_full_mask_for_n(n);

        // Losujemy blok, ktĂłry posĹ‚uĹĽy jako dom dla komĂłrek bazowych (Base Cells)
        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));

        // Zgodnie z architekturÄ…: 0..N-1 (rzÄ™dy), N..2N-1 (kolumny), 2N..3N-1 (bloki)
        const int house = 2 * n + box;
        const int p0 = topo.house_offsets[static_cast<size_t>(house)];
        const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];

        // Oczekujemy minimum 4 komĂłrek w bloku
        if (p1 - p0 < 4) return false;

        int b1 = -1;
        int b2 = -1;

        // Baza Exoceta (Base Cells) musi leĹĽeÄ‡ w rĂłĹĽnych rzÄ™dach i kolumnach tego samego bloku
        for (int i = p0; i < p1 && b1 < 0; ++i) {
            b1 = topo.houses_flat[static_cast<size_t>(i)];
        }

        for (int i = p1 - 1; i >= p0 && b2 < 0; --i) {
            const int c = topo.houses_flat[static_cast<size_t>(i)];
            if (b1 == c) continue;
            
            // Weryfikacja asymetrii rzÄ™dĂłw i kolumn
            if (topo.cell_row[static_cast<size_t>(c)] == topo.cell_row[static_cast<size_t>(b1)]) continue;
            if (topo.cell_col[static_cast<size_t>(c)] == topo.cell_col[static_cast<size_t>(b1)]) continue;
            b2 = c;
        }

        if (b1 < 0 || b2 < 0) return false;

        const int r1 = topo.cell_row[static_cast<size_t>(b1)];
        const int c1 = topo.cell_col[static_cast<size_t>(b1)];
        const int r2 = topo.cell_row[static_cast<size_t>(b2)];
        const int c2 = topo.cell_col[static_cast<size_t>(b2)];

        // Cele krzyĹĽowe (Target Cells)
        const int t1 = r1 * n + c2;
        const int t2 = r2 * n + c1;

        // Wybieramy cyfry dla komĂłrek bazowych i linii wspierajÄ…cych
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 64 && d2 == d1; ++g) {
            d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        int d3 = d2;
        for (int g = 0; g < 64 && (d3 == d1 || d3 == d2); ++g) {
            d3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        int d4 = d3;
        for (int g = 0; g < 64 && (d4 == d1 || d4 == d2 || d4 == d3); ++g) {
            d4 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        int d5 = d4;
        for (int g = 0; g < 64 && (d5 == d1 || d5 == d2 || d5 == d3 || d5 == d4); ++g) {
            d5 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }

        // Maska komĂłrek bazowych i docelowych (w docelowych dorzucamy "szum" by wzbudziÄ‡ Exocet, a nie prostego Naked Pair)
        const uint64_t base_mask = (1ULL << d1) | (1ULL << d2);
        const uint64_t cross_mask = base_mask | (1ULL << d3) | (1ULL << d4);
        const uint64_t row_gate_mask = base_mask | (1ULL << d4) | (1ULL << d5);
        const uint64_t col_gate_mask = base_mask | (1ULL << d3) | (1ULL << d5);

        // Wstrzykiwanie masek w plan:
        // dla SeniorExocet trzymamy twardo tylko bazÄ™, a cele i wsparcia jako miÄ™kki skeleton.
        plan.add_anchor(b1, base_mask);
        plan.add_anchor(b2, base_mask);

        if (senior_mode) {
            const uint64_t soft_target_mask = (base_mask | (1ULL << d3) | (1ULL << d4)) & full;
            const uint64_t soft_gate_mask = (base_mask | (1ULL << d4) | (1ULL << d5)) & full;
            plan.add_anchor(t1, soft_target_mask);
            plan.add_anchor(t2, soft_target_mask);

            for (int cc = 0; cc < n; ++cc) {
                if (cc == c1 || cc == c2) continue;
                const int idx = r1 * n + cc;
                if (topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(b1)]) continue;
                if (plan.add_skeleton(idx, soft_gate_mask)) break;
            }
            for (int rr = 0; rr < n; ++rr) {
                if (rr == r1 || rr == r2) continue;
                const int idx = rr * n + c2;
                if (topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(b2)]) continue;
                if (plan.add_skeleton(idx, soft_gate_mask)) break;
            }
        } else {
            plan.add_anchor(t1, cross_mask & full);
            plan.add_anchor(t2, cross_mask & full);
            int added = 0;
            for (int cc = 0; cc < n && added < 1; ++cc) {
                if (cc == c1 || cc == c2) continue;
                const int idx = r1 * n + cc;
                if (topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(b1)]) continue;
                if (plan.add_skeleton(idx, row_gate_mask & full)) ++added;
            }
            added = 0;
            for (int rr = 0; rr < n && added < 1; ++rr) {
                if (rr == r1 || rr == r2) continue;
                const int idx = rr * n + c2;
                if (topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(b2)]) continue;
                if (plan.add_skeleton(idx, col_gate_mask & full)) ++added;
            }
        }

        plan.valid = senior_mode ? (plan.anchor_count >= 2 && plan.skeleton_count >= 4)
                                 : (plan.anchor_count >= 4);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing




