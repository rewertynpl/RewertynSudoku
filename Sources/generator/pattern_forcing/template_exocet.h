// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: template_exocet.h
// Opis: Generuje rygorystyczny matematyczny szablon dla strategii Exocet
//       wsparcia asymetrycznych geometrii NxN. Zero-allocation.
// ============================================================================

#pragma once

#include <array>
#include <cstdint>
#include <random>

// Zależności do głównej struktury topologii (zakładając ścieżkę do core/board.h)
#include "../../core/board.h"

namespace sudoku_hpc::pattern_forcing {

// Współdzielona struktura planu wstrzykiwania (Zero-Allocation na stosie)
struct ExactPatternTemplatePlan {
    bool valid = false;
    int anchor_count = 0;
    std::array<int, 64> anchor_idx{};
    std::array<uint64_t, 64> anchor_masks{};

    // Szybkie dodawanie komórki "zakotwiczonej" z rygorystyczną maską
    inline bool add_anchor(int idx, uint64_t mask) {
        if (idx < 0 || anchor_count >= 64) return false;
        
        // Unikamy duplikatów
        for (int i = 0; i < anchor_count; ++i) {
            if (anchor_idx[i] == idx) return false;
        }
        
        anchor_idx[anchor_count] = idx;
        anchor_masks[anchor_count] = mask;
        ++anchor_count;
        return true;
    }
};

// Generowanie pełnej maski bitowej dla danego N
inline uint64_t pf_full_mask_for_n(int n) {
    return (n >= 64) ? ~0ULL : ((1ULL << n) - 1ULL);
}

class TemplateExocet {
public:
    // Wstrzykuje układ Base Cells i Target Cells charakterystyczny dla Exoceta.
    // Zmusza DLX solver do wybudowania reszty planszy "wokół" tego szablonu.
    static bool build(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {}; // Reset struktury

        // Exocet matematycznie wymaga podziału pudełek na minimum 2x2.
        if (topo.box_rows <= 1 || topo.box_cols <= 1) {
            return false;
        }

        const int n = topo.n;
        const uint64_t full = pf_full_mask_for_n(n);

        // Losujemy blok, który posłuży jako dom dla komórek bazowych (Base Cells)
        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));

        // Zgodnie z architekturą: 0..N-1 (rzędy), N..2N-1 (kolumny), 2N..3N-1 (bloki)
        const int house = 2 * n + box;
        const int p0 = topo.house_offsets[static_cast<size_t>(house)];
        const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];

        // Oczekujemy minimum 4 komórek w bloku
        if (p1 - p0 < 4) return false;

        int b1 = -1;
        int b2 = -1;

        // Baza Exoceta (Base Cells) musi leżeć w różnych rzędach i kolumnach tego samego bloku
        for (int i = p0; i < p1 && b1 < 0; ++i) {
            b1 = topo.houses_flat[static_cast<size_t>(i)];
        }

        for (int i = p1 - 1; i >= p0 && b2 < 0; --i) {
            const int c = topo.houses_flat[static_cast<size_t>(i)];
            if (b1 == c) continue;
            
            // Weryfikacja asymetrii rzędów i kolumn
            if (topo.cell_row[static_cast<size_t>(c)] == topo.cell_row[static_cast<size_t>(b1)]) continue;
            if (topo.cell_col[static_cast<size_t>(c)] == topo.cell_col[static_cast<size_t>(b1)]) continue;
            b2 = c;
        }

        if (b1 < 0 || b2 < 0) return false;

        const int r1 = topo.cell_row[static_cast<size_t>(b1)];
        const int c1 = topo.cell_col[static_cast<size_t>(b1)];
        const int r2 = topo.cell_row[static_cast<size_t>(b2)];
        const int c2 = topo.cell_col[static_cast<size_t>(b2)];

        // Cele krzyżowe (Target Cells)
        const int t1 = r1 * n + c2;
        const int t2 = r2 * n + c1;

        // Wybieramy dwie cyfry dla komórek bazowych
        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 64 && d2 == d1; ++g) {
            d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }

        // Maska komórek bazowych i docelowych (w docelowych dorzucamy "szum" by wzbudzić Exocet, a nie prostego Naked Pair)
        const uint64_t base_mask = (1ULL << d1) | (1ULL << d2);
        const uint64_t cross_mask = base_mask | (1ULL << static_cast<int>((d1 + 1) % n));

        // Wstrzykiwanie masek w plan (100% precyzyjne ograniczenie dla DLX Solvera)
        plan.add_anchor(b1, base_mask);
        plan.add_anchor(b2, base_mask);
        plan.add_anchor(t1, cross_mask & full);
        plan.add_anchor(t2, cross_mask & full);

        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing