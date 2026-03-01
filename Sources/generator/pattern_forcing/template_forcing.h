// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: template_forcing.h
// Opis: Generuje matematyczny szablon dla Forcing Chains (łańcuchów wymuszających).
//       Tworzy zbiór zablokowanych komórek bivalue, inicjując siatkę powiązań.
//       Zero-allocation.
// ============================================================================

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
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {}; // Reset struktury

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

        // Maski bivalue (zazębiające się) łączące cyfry w zamknięty łańcuch (graf)
        const uint64_t m12 = (1ULL << d1) | (1ULL << d2);
        const uint64_t m23 = (1ULL << d2) | (1ULL << d3);
        const uint64_t m13 = (1ULL << d1) | (1ULL << d3);

        // Wstrzykujemy kotwice:
        // Pivot(m12) -> widzi A(m23) i T(m12)
        // A(m23) -> widzi Pivot(m12) i B(m13)
        // B(m13) -> widzi A(m23) i T(m12)
        // W efekcie tworzy to zamknięty graf zależności wymuszających dla Solver'a i Digger'a.
        plan.add_anchor(p, m12 & full);
        plan.add_anchor(a, m23 & full);
        plan.add_anchor(b, m13 & full);
        plan.add_anchor(t, m12 & full);

        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing