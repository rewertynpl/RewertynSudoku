// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: template_forcing.h
// Opis: Generuje matematyczny szablon dla Forcing Chains (łańcuchów wymuszających).
//       Tworzy zbiór zablokowanych komórek bivalue, inicjując siatkę powiązań.
//       Zero-allocation.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <array>
#include <cstdint>
#include <random>

// Zależności do topologii i struktury planu
#include "../../core/board.h"
#include "template_exocet.h" // Używa ExactPatternTemplatePlan i pf_full_mask_for_n

namespace sudoku_hpc::pattern_forcing {

class TemplateForcing {
public:
    // Wstrzykuje "zalążek" łańcucha XY/Forcing Chain.
    // Tworzy 4 komórki w układzie prostokąta przypisując im zazębiające się pary cyfr.
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan, bool dynamic_mode = false) {
        plan = {}; // Reset struktury
        plan.explicit_skeleton = true;

        const int n = topo.n;
        // Wymaga minimum siatki 4x4
        if (n < 4) return false;

        const uint64_t full = pf_full_mask_for_n(n);

        // Losujemy 2 różne rzędy i 2 różne kolumny
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = r1;
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = c1;

        // Ograniczone próby zapobiegające nieskończonemu zawieszeniu
        for (int g = 0; g < 64 && r2 == r1; ++g) {
            r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        for (int g = 0; g < 64 && c2 == c1; ++g) {
            c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }

        if (r1 == r2 || c1 == c2) return false;

        // Definiujemy wierzchołki struktury wymuszającej:
        // p - pivot (komórka startowa)
        // a, b - ramiona łańcucha
        // t - target (cel łańcucha)
        const int p = r1 * n + c1;
        const int a = r1 * n + c2;
        const int b = r2 * n + c2;
        const int t = r2 * n + c1;

        // Wybieramy 3 różne cyfry, które utworzą łańcuch bivalue
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

        // Maski bivalue (zazębiające się) łączące cyfry w zamknięty łańcuch (graf)
        const uint64_t m12 = (1ULL << d1) | (1ULL << d2);
        const uint64_t m23 = (1ULL << d2) | (1ULL << d3);
        const uint64_t m13 = (1ULL << d1) | (1ULL << d3);
        const uint64_t m124 = m12 | (1ULL << d4);
        const uint64_t m234 = m23 | (1ULL << d4);
        const uint64_t m135 = m13 | (1ULL << d5);
        const uint64_t support_row = m12 | (1ULL << d5);
        const uint64_t support_col = m23 | (1ULL << d5);

        // Wstrzykujemy kotwice:
        // Pivot(m12) -> widzi A(m23) i T(m12)
        // A(m23) -> widzi Pivot(m12) i B(m13)
        // B(m13) -> widzi A(m23) i T(m12)
        // W efekcie tworzy to zamknięty graf zależności wymuszających dla Solver'a i Digger'a.
        plan.add_anchor(p, m124 & full);
        plan.add_anchor(a, m234 & full);
        plan.add_anchor(b, m135 & full);
        plan.add_anchor(t, m124 & full);

        int support_added = 0;
        for (int cc = 0; cc < n && support_added < 1; ++cc) {
            if (cc == c1 || cc == c2) continue;
            if (plan.add_skeleton(r1 * n + cc, support_row & full)) ++support_added;
        }
        support_added = 0;
        for (int rr = 0; rr < n && support_added < 1; ++rr) {
            if (rr == r1 || rr == r2) continue;
            if (plan.add_skeleton(rr * n + c2, support_col & full)) ++support_added;
        }

        if (dynamic_mode) {
            // Dodatkowe bivalue/trivalue odnogi zwiększają szansę na forcing branch.
            int rr = r2;
            int cc = c1;
            if (c1 + 1 < n && c1 + 1 != c2) cc = c1 + 1;
            else if (c1 - 1 >= 0 && c1 - 1 != c2) cc = c1 - 1;
            const int x1 = rr * n + cc;

            rr = r1;
            cc = c2;
            if (r1 + 1 < n && r1 + 1 != r2) rr = r1 + 1;
            else if (r1 - 1 >= 0 && r1 - 1 != r2) rr = r1 - 1;
            const int x2 = rr * n + cc;

            const uint64_t m24 = (1ULL << d2) | (1ULL << d4);
            const uint64_t m34 = (1ULL << d3) | (1ULL << d4);
            const uint64_t m134 = m13 | (1ULL << d4);
            const uint64_t m245 = m24 | (1ULL << d5);
            const uint64_t m345 = m34 | (1ULL << d5);
            plan.add_anchor(x1, m245 & full);
            plan.add_anchor(x2, m345 & full);
            plan.add_skeleton(((r1 + r2) / 2) * n + ((c1 + c2) / 2), m134 & full);
        }

        plan.valid = dynamic_mode ? (plan.anchor_count >= 6 && plan.skeleton_count >= 3)
                                  : (plan.anchor_count >= 4 && plan.skeleton_count >= 2);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing
