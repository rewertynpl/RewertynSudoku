// ============================================================================
// SUDOKU HPC - POST PROCESSING
// Moduł: quality_metrics.h
// Opis: Ocena estetyki siatki, rozkładu wskazówek (clues) oraz entropii.
//       Zawiera optymalizacje LUT (Look-Up Table) dla obliczeń logarytmów.
//       Zero-allocation w gorącej ścieżce wywołań.
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../../core/board.h"
#include "../../config/run_config.h"

namespace sudoku_hpc::post_processing {

struct QualityMetrics {
    int clues = 0;
    int row_min = 0;
    int row_max = 0;
    int col_min = 0;
    int col_max = 0;
    int box_min = 0;
    int box_max = 0;
    int digit_min = 0;
    int digit_max = 0;
    
    double normalized_entropy = 0.0;
    double entropy_threshold = 0.0;
    
    bool symmetry_ok = true;
    bool distribution_balance_ok = true;
};

// ============================================================================
// BŁYSKAWICZNA ENTROPIA (LUT)
// Tworzymy Look-Up Table (LUT) przy starcie dla funkcji x * log2(x).
// W siatce do 64x64 pojedyncza cyfra może wystąpić max 64 razy, więc rozmiar 82 
// zostawia bezpieczny bufor dla standardowych i rozszerzonych plansz.
// ============================================================================
inline const std::array<double, 82>& get_c_log2_c_lut() {
    static const std::array<double, 82> lut =[]() {
        std::array<double, 82> arr{};
        arr[0] = 0.0;
        for (int i = 1; i <= 81; ++i) {
            arr[static_cast<size_t>(i)] = static_cast<double>(i) * std::log2(static_cast<double>(i));
        }
        return arr;
    }();
    return lut;
}

inline double entropy_threshold_for_n(int n) {
    if (n <= 12) return 0.40;
    if (n <= 24) return 0.55;
    return 0.65;
}

// Sprawdza czy plansza posiada symetrię środkową (180 stopni)
inline bool check_center_symmetry_givens(const std::vector<uint16_t>& puzzle, const GenericTopology& topo) {
    const auto& sym = topo.cell_center_sym;
    const uint16_t* const puzzle_ptr = puzzle.data();
    for (int idx = 0; idx < topo.nn; ++idx) {
        const int sym_idx = sym[static_cast<size_t>(idx)];
        // Sprawdzamy tylko do połowy, by nie powtarzać pracy
        if (idx > sym_idx) continue;
        
        const bool given_a = puzzle_ptr[static_cast<size_t>(idx)] != 0;
        const bool given_b = puzzle_ptr[static_cast<size_t>(sym_idx)] != 0;
        if (given_a != given_b) {
            return false;
        }
    }
    return true;
}

// Bufor wątkowy dla statystyk, zapobiegający alokacjom std::vector w ewaluacji
struct QualityScratch {
    int prepared_n = 0;
    std::vector<int> row_counts;
    std::vector<int> col_counts;
    std::vector<int> box_counts;
    std::vector<int> digit_counts;

    void ensure(int n) {
        if (prepared_n != n) {
            row_counts.assign(static_cast<size_t>(n), 0);
            col_counts.assign(static_cast<size_t>(n), 0);
            box_counts.assign(static_cast<size_t>(n), 0);
            digit_counts.assign(static_cast<size_t>(n), 0);
            prepared_n = n;
        } else {
            std::fill(row_counts.begin(), row_counts.end(), 0);
            std::fill(col_counts.begin(), col_counts.end(), 0);
            std::fill(box_counts.begin(), box_counts.end(), 0);
            std::fill(digit_counts.begin(), digit_counts.end(), 0);
        }
    }
};

inline QualityScratch& tls_quality_scratch() {
    thread_local QualityScratch s{};
    return s;
}

inline QualityMetrics evaluate_quality_metrics(
    const std::vector<uint16_t>& puzzle,
    const GenericTopology& topo,
    const GenerateRunConfig& cfg) {
    
    QualityMetrics m{};
    QualityScratch& scratch = tls_quality_scratch();
    scratch.ensure(topo.n);

    int* const row_ptr = scratch.row_counts.data();
    int* const col_ptr = scratch.col_counts.data();
    int* const box_ptr = scratch.box_counts.data();
    int* const digit_ptr = scratch.digit_counts.data();
    const uint32_t* const packed = topo.cell_rcb_packed.data();
    const uint16_t* const puzzle_ptr = puzzle.data();

    // 1. Zbieranie statystyk
    for (int idx = 0; idx < topo.nn; ++idx) {
        const int v = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
        if (v <= 0) continue;
        
        ++m.clues;
        const uint32_t p = packed[static_cast<size_t>(idx)];
        // Dekodowanie z 32-bitowego wektora
        const int r = static_cast<int>(p & 63U);
        const int c = static_cast<int>((p >> 6U) & 63U);
        const int b = static_cast<int>((p >> 12U) & 63U);
        
        ++row_ptr[static_cast<size_t>(r)];
        ++col_ptr[static_cast<size_t>(c)];
        ++box_ptr[static_cast<size_t>(b)];
        if (v <= topo.n) {
            ++digit_ptr[static_cast<size_t>(v - 1)];
        }
    }

    auto get_minmax = [n = topo.n](const std::vector<int>& counts) -> std::pair<int, int> {
        if (n <= 0) return {0, 0};
        int mn = counts[0];
        int mx = counts[0];
        for (int i = 1; i < n; ++i) {
            const int x = counts[static_cast<size_t>(i)];
            if (x < mn) mn = x;
            if (x > mx) mx = x;
        }
        return {mn, mx};
    };

    // 2. Analiza dystrybucji Min/Max
    const auto [row_min, row_max] = get_minmax(scratch.row_counts);
    const auto [col_min, col_max] = get_minmax(scratch.col_counts);
    const auto[box_min, box_max] = get_minmax(scratch.box_counts);
    const auto[digit_min, digit_max] = get_minmax(scratch.digit_counts);
    
    m.row_min = row_min; m.row_max = row_max;
    m.col_min = col_min; m.col_max = col_max;
    m.box_min = box_min; m.box_max = box_max;
    m.digit_min = digit_min; m.digit_max = digit_max;

    // 3. Obliczanie znormalizowanej Entropii Shannon'a
    if (m.clues > 0) {
        double sum_c_log2_c = 0.0;
        const auto& lut = get_c_log2_c_lut();
        for (int i = 0; i < topo.n; ++i) {
            const int c = digit_ptr[static_cast<size_t>(i)];
            // Chroni przed wyjściem poza LUT w skrajnych nieobsługiwanych siatkach
            if (c > 0 && c <= 81) { 
                sum_c_log2_c += lut[static_cast<size_t>(c)];
            }
        }
        const double T = static_cast<double>(m.clues);
        const double entropy = std::log2(T) - (sum_c_log2_c / T);
        
        const double max_entropy = std::log2(static_cast<double>(topo.n));
        m.normalized_entropy = max_entropy > 0.0 ? (entropy / max_entropy) : 0.0;
    }
    
    m.entropy_threshold = entropy_threshold_for_n(topo.n);
    m.symmetry_ok = (!cfg.symmetry_center) || check_center_symmetry_givens(puzzle, topo);

    // 4. Tolerancja dystrybucji
    const double ideal = topo.n > 0 ? static_cast<double>(m.clues) / static_cast<double>(topo.n) : 0.0;
    const double allowed_dev = std::max(2.0, ideal * 0.80);
    
    auto within_dev = [ideal, allowed_dev](int x) {
        return std::abs(static_cast<double>(x) - ideal) <= (allowed_dev + 1.0);
    };
    
    m.distribution_balance_ok = 
        within_dev(m.row_min) && within_dev(m.row_max) &&
        within_dev(m.col_min) && within_dev(m.col_max) &&
        within_dev(m.box_min) && within_dev(m.box_max) &&
        within_dev(m.digit_min) && within_dev(m.digit_max);

    return m;
}

// Interfejs umowy jakościowej - czy grid spełnia wymagania run_config
struct QualityContract {
    bool is_unique = true;
    bool logic_replay_ok = true;
    bool clue_range_ok = true;
    bool symmetry_ok = true;
    bool givens_entropy_ok = true;
    bool distribution_balance_ok = true;
    std::string generation_mode;
};

inline bool quality_contract_passed(const QualityContract& c, const GenerateRunConfig& cfg) {
    if (!cfg.enable_quality_contract) return true;
    if (!c.is_unique) return false;
    if (!c.clue_range_ok || !c.symmetry_ok) return false;
    if (!c.givens_entropy_ok) return false;
    if (cfg.enable_distribution_filter && !c.distribution_balance_ok) return false;
    if (cfg.enable_replay_validation && !c.logic_replay_ok) return false;
    return true;
}

} // namespace sudoku_hpc::post_processing