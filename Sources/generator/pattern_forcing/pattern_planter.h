// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: pattern_planter.h
// Opis: Menedżer wstrzykiwania wzorców (Planter). Łączy konkretne szablony,
//       alokuje struktury na stosie w TLS i eksponuje widok (SeedView).
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <vector>

// Core 
#include "../../core/board.h"
#include "../../config/run_config.h"

// Specyficzne szablony
#include "template_exocet.h"
#include "template_sk_loop.h"
#include "template_forcing.h"

namespace sudoku_hpc::pattern_forcing {

// Typy wzorców (do losowych "luźnych" kotwic)
enum class PatternKind : uint8_t {
    None = 0,
    Chain,
    ExocetLike,
    LoopLike,
    ForcingLike
};

// Mapowanie z flagi w main()
enum class TargetPattern : uint8_t {
    None = 0,
    XChain,
    XYChain,
    Exocet,
    MSLS
};

// Bezpieczny widok przekazywany na zewnątrz bez kopiowania wektorów
struct PatternSeedView {
    PatternKind kind = PatternKind::None;
    const std::vector<uint16_t>* seed_puzzle = nullptr;
    const std::vector<uint64_t>* allowed_masks = nullptr;
    const std::vector<uint8_t>* protected_cells = nullptr;
    int anchor_count = 0;
};

// Bufor współdzielony dla każdego wątku (Thread Local Storage)
// Gwarantuje ZERO-ALLOC podczas wielokrotnego generowania masek.
struct PatternScratch {
    int prepared_nn = 0;
    std::vector<uint16_t> seed_puzzle;
    std::vector<uint64_t> allowed_masks;
    std::vector<uint8_t> protected_cells;
    std::array<int, 64> anchors{};
    int anchor_count = 0;

    void ensure(const GenericTopology& topo) {
        if (prepared_nn != topo.nn) {
            seed_puzzle.assign(static_cast<size_t>(topo.nn), 0);
            allowed_masks.assign(static_cast<size_t>(topo.nn), 0ULL);
            protected_cells.assign(static_cast<size_t>(topo.nn), 0);
            prepared_nn = topo.nn;
        }
    }

    void reset(const GenericTopology& topo) {
        ensure(topo);
        std::fill(seed_puzzle.begin(), seed_puzzle.end(), 0);
        const uint64_t full = pf_full_mask_for_n(topo.n);
        std::fill(allowed_masks.begin(), allowed_masks.end(), full);
        std::fill(protected_cells.begin(), protected_cells.end(), static_cast<uint8_t>(0));
        anchor_count = 0;
    }

    bool add_anchor(int idx) {
        if (idx < 0 || idx >= prepared_nn) return false;
        // Zabezpieczenie przed duplikatami
        for (int i = 0; i < anchor_count; ++i) {
            if (anchors[static_cast<size_t>(i)] == idx) return false;
        }
        if (anchor_count >= static_cast<int>(anchors.size())) return false;
        anchors[static_cast<size_t>(anchor_count++)] = idx;
        return true;
    }
};

inline PatternScratch& tls_pattern_scratch() {
    thread_local PatternScratch s{};
    return s;
}

// Funkcja pomocnicza - zwraca maskę złożoną z `want` unikalnych losowych cyfr
inline uint64_t random_digit_mask(int n, int want, std::mt19937_64& rng) {
    if (n <= 0) return 0ULL;
    const int k = std::clamp(want, 1, n);
    uint64_t m = 0ULL;
    int placed = 0;
    int guard = 0;
    while (placed < k && guard < n * 8) {
        ++guard;
        const int d0 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const uint64_t bit = 1ULL << d0;
        if ((m & bit) != 0ULL) continue;
        m |= bit;
        ++placed;
    }
    if (m == 0ULL) {
        const int d0 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        m = (1ULL << d0);
    }
    return m;
}

inline PatternKind pick_kind(RequiredStrategy required, int level) {
    switch (required) {
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return PatternKind::ExocetLike;
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::MSLS:
    case RequiredStrategy::PatternOverlayMethod:
        return PatternKind::LoopLike;
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
        return PatternKind::ForcingLike;
    default:
        break;
    }
    if (level >= 8) return PatternKind::ForcingLike;
    if (level >= 6) return PatternKind::Chain;
    return PatternKind::None;
}

// --- Szablony luźne (fallback gdy brakuje Exact Template) ---

inline bool build_chain_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 2) return false;
    const int n = topo.n;
    int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int r2 = r1;
    int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c2 = c1;
    for (int t = 0; t < 64 && r2 == r1; ++t) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int t = 0; t < 64 && c2 == c1; ++t) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (r1 == r2 || c1 == c2) return false;
    sc.add_anchor(r1 * n + c1);
    sc.add_anchor(r1 * n + c2);
    sc.add_anchor(r2 * n + c2);
    sc.add_anchor(r2 * n + c1);
    return sc.anchor_count >= 4;
}

inline bool build_exocet_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.box_rows <= 1 || topo.box_cols <= 1) return false;
    const int n = topo.n;
    const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
    const int house = 2 * n + box;
    const int start = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    if (end - start < 2) return false;

    int b1 = topo.houses_flat[static_cast<size_t>(start + (rng() % static_cast<uint64_t>(end - start)))];
    int b2 = b1;
    for (int t = 0; t < 128; ++t) {
        const int c = topo.houses_flat[static_cast<size_t>(start + (rng() % static_cast<uint64_t>(end - start)))];
        if (c == b1) continue;
        if (topo.cell_row[static_cast<size_t>(c)] == topo.cell_row[static_cast<size_t>(b1)]) continue;
        if (topo.cell_col[static_cast<size_t>(c)] == topo.cell_col[static_cast<size_t>(b1)]) continue;
        b2 = c;
        break;
    }
    if (b1 == b2) return false;
    sc.add_anchor(b1);
    sc.add_anchor(b2);

    const int r1 = topo.cell_row[static_cast<size_t>(b1)];
    const int r2 = topo.cell_row[static_cast<size_t>(b2)];
    const int c1 = topo.cell_col[static_cast<size_t>(b1)];
    const int c2 = topo.cell_col[static_cast<size_t>(b2)];
    sc.add_anchor(r1 * n + c2);
    sc.add_anchor(r2 * n + c1);
    return sc.anchor_count >= 2;
}

inline bool build_loop_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) return false;
    const int n = topo.n;
    int r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    sc.add_anchor(r3 * n + c3);
    r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    c3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    sc.add_anchor(r3 * n + c3);
    return sc.anchor_count >= 4;
}

inline int default_anchor_count(const GenericTopology& topo, PatternKind kind) {
    switch (kind) {
    case PatternKind::ExocetLike: return std::clamp(topo.n / 2, 4, 10);
    case PatternKind::LoopLike: return std::clamp(topo.n / 2 + 2, 6, 12);
    case PatternKind::ForcingLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::Chain: return std::clamp(topo.n / 3 + 3, 4, 10);
    default: return 0;
    }
}

inline void apply_anchor_masks(const GenericTopology& topo, PatternScratch& sc, PatternKind kind, std::mt19937_64& rng) {
    if (sc.anchor_count <= 0) return;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    uint64_t mask_a = random_digit_mask(topo.n, 2, rng);
    uint64_t mask_b = random_digit_mask(topo.n, 2, rng);
    uint64_t mask_c = random_digit_mask(topo.n, 3, rng);
    
    if (kind == PatternKind::ExocetLike) {
        const uint64_t shared = random_digit_mask(topo.n, 3, rng);
        if (sc.anchor_count >= 1) sc.allowed_masks[static_cast<size_t>(sc.anchors[0])] = shared;
        if (sc.anchor_count >= 2) sc.allowed_masks[static_cast<size_t>(sc.anchors[1])] = shared;
        for (int i = 2; i < sc.anchor_count; ++i) {
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = mask_c;
        }
        return;
    }
    if (kind == PatternKind::ForcingLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i & 1) ? (mask_a | mask_b) : (mask_b | mask_c);
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::LoopLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            const uint64_t m = (i % 3 == 0) ? mask_c : ((i & 1) ? mask_a : mask_b);
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    // Domyślnie Chain
    for (int i = 0; i < sc.anchor_count; ++i) {
        sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (i & 1) ? mask_a : mask_b;
    }
}

// Główna funkcja orkiestratora
inline bool build_seed(
    const GenericTopology& topo,
    const GenerateRunConfig& cfg,
    RequiredStrategy required_strategy,
    int difficulty_level_required,
    std::mt19937_64& rng,
    PatternSeedView& out) {
    
    out = {};
    if (!cfg.pattern_forcing_enabled) return false;

    int forcing_level = std::clamp(difficulty_level_required, 1, 8);
    if (cfg.max_pattern_depth > 0) {
        forcing_level = std::min(forcing_level, cfg.max_pattern_depth);
        forcing_level = std::clamp(forcing_level, 1, 8);
    }

    PatternKind kind = pick_kind(required_strategy, forcing_level);
    if (kind == PatternKind::None) return false;

    PatternScratch& sc = tls_pattern_scratch();
    sc.reset(topo);

    ExactPatternTemplatePlan exact_plan{};
    bool exact_matched = false;

    // Próba wstrzyknięcia precyzyjnego matematycznego szablonu dla P8
    switch (required_strategy) {
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
            exact_matched = TemplateExocet::build(topo, rng, exact_plan);
            break;
        case RequiredStrategy::SKLoop:
            exact_matched = TemplateSKLoop::build(topo, rng, exact_plan);
            break;
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
            exact_matched = TemplateForcing::build(topo, rng, exact_plan);
            break;
        default:
            break;
    }
    
    // Fallback dla wysokich poziomów jeśli brak zdefiniowanej konkretnej strategii
    if (!exact_matched) {
        if (forcing_level >= 8) {
            exact_matched = TemplateForcing::build(topo, rng, exact_plan);
        } else if (forcing_level >= 7) {
            exact_matched = TemplateSKLoop::build(topo, rng, exact_plan);
        }
    }

    // Aplikacja masek z Exact Planu (jeśli się udał)
    if (exact_matched && exact_plan.valid && exact_plan.anchor_count > 0) {
        for (int i = 0; i < exact_plan.anchor_count; ++i) {
            const int idx = exact_plan.anchor_idx[static_cast<size_t>(i)];
            if (!sc.add_anchor(idx)) continue;

            uint64_t mask = exact_plan.anchor_masks[static_cast<size_t>(i)];
            if (mask == 0ULL) mask = pf_full_mask_for_n(topo.n);
            
            sc.allowed_masks[static_cast<size_t>(idx)] = mask;
        }

        // Chronione komórki zapobiegają ich usunięciu przez MCTS Digger
        if (cfg.pattern_forcing_lock_anchors) {
            for (int i = 0; i < sc.anchor_count; ++i) {
                const int idx = sc.anchors[static_cast<size_t>(i)];
                sc.protected_cells[static_cast<size_t>(idx)] = 1;
            }
        }
        
        if (sc.anchor_count > 0) {
            out.kind = (kind == PatternKind::None) ? PatternKind::ForcingLike : kind;
            out.seed_puzzle = &sc.seed_puzzle;
            out.allowed_masks = &sc.allowed_masks;
            out.protected_cells = &sc.protected_cells;
            out.anchor_count = sc.anchor_count;
            return true;
        }
    }

    // Fallback: luźne kotwice (jeśli Exact Plan zawiódł z powodu asymetrii lub P1-P6)
    bool ok = false;
    switch (kind) {
        case PatternKind::ExocetLike:
            ok = build_exocet_like_anchors(topo, sc, rng);
            break;
        case PatternKind::LoopLike:
            ok = build_loop_like_anchors(topo, sc, rng);
            break;
        case PatternKind::ForcingLike:
        case PatternKind::Chain:
            ok = build_chain_anchors(topo, sc, rng);
            break;
        default:
            break;
    }
    
    if (!ok || sc.anchor_count <= 0) return false;

    int anchor_target = cfg.pattern_forcing_anchor_count > 0
        ? cfg.pattern_forcing_anchor_count
        : default_anchor_count(topo, kind);
    anchor_target = std::clamp(anchor_target, sc.anchor_count, std::min(topo.nn, 32));

    // Dopełnianie luźnymi cyframi
    int guard = 0;
    while (sc.anchor_count < anchor_target && guard < topo.nn * 4) {
        ++guard;
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
        sc.add_anchor(idx);
    }

    apply_anchor_masks(topo, sc, kind, rng);
    
    if (cfg.pattern_forcing_lock_anchors) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            const int idx = sc.anchors[static_cast<size_t>(i)];
            sc.protected_cells[static_cast<size_t>(idx)] = 1;
        }
    }

    out.kind = kind;
    out.seed_puzzle = &sc.seed_puzzle;
    out.allowed_masks = &sc.allowed_masks;
    out.protected_cells = &sc.protected_cells;
    out.anchor_count = sc.anchor_count;
    return true;
}

} // namespace sudoku_hpc::pattern_forcing
