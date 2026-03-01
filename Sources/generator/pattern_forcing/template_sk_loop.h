// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: template_sk_loop.h
// Opis: Generuje rygorystyczny matematyczny szablon dla strategii SK-Loop.
//       Wymusza powstanie łańcucha cyklicznego w DLX. Zero-allocation.
// ============================================================================

#pragma once

#include <array>
#include <cstdint>
#include <random>

// Zależności do topologii i struktury planu
#include "../../core/board.h"
#include "template_exocet.h" // Używamy wspóldzielonego ExactPatternTemplatePlan i pf_full_mask_for_n

namespace sudoku_hpc::pattern_forcing {

class TemplateSKLoop {
public:
    // Wstrzykuje układ wierzchołków dla SK-Loop (cykliczny prostokąt).
    // Dwie komórki stają się twardymi bivalue, a dwie dostają cyfry "wyjściowe" (exit digits).
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {}; // Reset struktury

        const int n = topo.n;
        // SK-Loop wymaga minimum siatki 4x4 (w praktyce sensowne dla N>=6)
        if (n < 4) return false;

        const uint64_t full = pf_full_mask_for_n(n);

        // Losujemy dwa różne rzędy i dwie różne kolumny
        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = r1;
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = c1;

        // Ograniczona pętla zapobiegająca nieskończonemu zawieszeniu
        for (int g = 0; g < 64 && r2 == r1; ++g) {
            r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        for (int g = 0; g < 64 && c2 == c1; ++g) {
            c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }

        if (r1 == r2 || c1 == c2) return false;

        // Wyznaczamy 4 wierzchołki (prostokąt)
        const int a = r1 * n + c1;
        const int b = r1 * n + c2;
        const int c = r2 * n + c1;
        const int d = r2 * n + c2;

        // Wybieramy dwie cyfry rdzeniowe (core digits)
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 64 && d2 == d1; ++g) {
            d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }

        const uint64_t core = (1ULL << d1) | (1ULL << d2);

        // Dodajemy cyfry wyjściowe dla dwóch wierzchołków by uniknąć natychmiastowego Unique Rectangle (Deadly Pattern)
        // Ograniczamy do modulo n, żeby nie przekroczyć rozmiaru maski
        const uint64_t ex1 = core | (1ULL << static_cast<int>((d1 + 2) % n));
        const uint64_t ex2 = core | (1ULL << static_cast<int>((d2 + 3) % n));

        // Wstrzykujemy ograniczenia
        // B i C to twarde węzły bivalue. A i D to węzły wyjściowe pętli.
        plan.add_anchor(a, ex1 & full);
        plan.add_anchor(b, core);
        plan.add_anchor(c, core);
        plan.add_anchor(d, ex2 & full);

        plan.valid = (plan.anchor_count == 4);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing