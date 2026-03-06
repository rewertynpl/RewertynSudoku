// ============================================================================
// SUDOKU HPC - EXACT PATTERN FORCING
// Moduł: pattern_planter.h
// Opis: Menedżer wstrzykiwania wzorców (Planter). Łączy konkretne szablony,
//       alokuje struktury na stosie w TLS i eksponuje widok (SeedView).
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <bit>
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
#include "template_msls_overlay.h"

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
    const int* anchor_idx = nullptr;
    const uint64_t* anchor_masks = nullptr;
    int anchor_count = 0;
    bool exact_template = false;
    int template_score = 0;
    int best_template_score = 0;
};

// Bufor współdzielony dla każdego wątku (Thread Local Storage)
// Gwarantuje ZERO-ALLOC podczas wielokrotnego generowania masek.
struct PatternScratch {
    int prepared_nn = 0;
    std::vector<uint16_t> seed_puzzle;
    std::vector<uint64_t> allowed_masks;
    std::vector<uint8_t> protected_cells;
    std::array<int, 64> anchors{};
    std::array<uint64_t, 64> anchor_masks{};
    int anchor_count = 0;
    bool exact_template = false;
    int template_score = 0;

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
        std::fill(anchor_masks.begin(), anchor_masks.end(), 0ULL);
        anchor_count = 0;
        exact_template = false;
        template_score = 0;
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

struct PatternMutationState {
    RequiredStrategy strategy = RequiredStrategy::None;
    PatternKind kind = PatternKind::None;
    bool have_last = false;
    bool have_best = false;
    ExactPatternTemplatePlan last_plan{};
    ExactPatternTemplatePlan best_plan{};
    int last_score = -1;
    int best_score = -1;
    int failure_streak = 0;
    int zero_use_streak = 0;

    void reset(RequiredStrategy rs, PatternKind pk) {
        strategy = rs;
        kind = pk;
        have_last = false;
        have_best = false;
        last_plan = {};
        best_plan = {};
        last_score = -1;
        best_score = -1;
        failure_streak = 0;
        zero_use_streak = 0;
    }
};

inline PatternMutationState& tls_pattern_mutation_state() {
    thread_local PatternMutationState s{};
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
    if (level >= 8) return PatternKind::ExocetLike;
    if (level >= 7) return PatternKind::LoopLike;
    if (level >= 6) return PatternKind::Chain;
    return PatternKind::None;
}

inline int exact_plan_mask_tightness(uint64_t mask) {
    const int bits = std::popcount(mask);
    if (bits <= 1) return 8;
    if (bits == 2) return 6;
    if (bits == 3) return 4;
    if (bits == 4) return 2;
    return 1;
}

inline int score_generic_exact_plan(
    const GenericTopology& topo,
    PatternKind kind,
    const ExactPatternTemplatePlan& plan) {
    if (!plan.valid || plan.anchor_count <= 0) return -1;

    int score = plan.anchor_count * 6;
    uint64_t row_seen = 0ULL;
    uint64_t col_seen = 0ULL;
    uint64_t box_seen = 0ULL;
    int bivalue_count = 0;
    int trivalue_count = 0;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int idx = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        const int bits = std::popcount(mask);
        score += exact_plan_mask_tightness(mask);
        if (bits <= 2) ++bivalue_count;
        else if (bits == 3) ++trivalue_count;

        const int row = topo.cell_row[static_cast<size_t>(idx)];
        const int col = topo.cell_col[static_cast<size_t>(idx)];
        const int box = topo.cell_box[static_cast<size_t>(idx)];
        row_seen |= (row < 64) ? (1ULL << row) : 0ULL;
        col_seen |= (col < 64) ? (1ULL << col) : 0ULL;
        box_seen |= (box < 64) ? (1ULL << box) : 0ULL;
    }

    score += 2 * std::popcount(row_seen);
    score += 2 * std::popcount(col_seen);
    score += std::popcount(box_seen);

    if (kind == PatternKind::ExocetLike) score += 12;
    else if (kind == PatternKind::LoopLike) score += 10;
    else if (kind == PatternKind::ForcingLike) score += 10;
    score += 2 * bivalue_count + trivalue_count;

    return score;
}

inline int score_exocet_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    bool senior_mode) {
    int score = score_generic_exact_plan(topo, PatternKind::ExocetLike, plan);
    if (score < 0) return score;

    int base_pair_score = 0;
    int cross_target_score = 0;
    int gate_score = 0;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mi) != 2) continue;
        const int row_i = topo.cell_row[static_cast<size_t>(ai)];
        const int col_i = topo.cell_col[static_cast<size_t>(ai)];
        const int box_i = topo.cell_box[static_cast<size_t>(ai)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const uint64_t mj = plan.anchor_masks[static_cast<size_t>(j)];
            if (mi != mj || std::popcount(mj) != 2) continue;
            const int row_j = topo.cell_row[static_cast<size_t>(aj)];
            const int col_j = topo.cell_col[static_cast<size_t>(aj)];
            const int box_j = topo.cell_box[static_cast<size_t>(aj)];
            if (box_i == box_j && row_i != row_j && col_i != col_j) {
                base_pair_score += 24;
                const int t1_row = row_i;
                const int t1_col = col_j;
                const int t2_row = row_j;
                const int t2_col = col_i;
                for (int k = 0; k < plan.anchor_count; ++k) {
                    const int ak = plan.anchor_idx[static_cast<size_t>(k)];
                    const uint64_t mk = plan.anchor_masks[static_cast<size_t>(k)];
                    const int row_k = topo.cell_row[static_cast<size_t>(ak)];
                    const int col_k = topo.cell_col[static_cast<size_t>(ak)];
                    if ((row_k == t1_row && col_k == t1_col) || (row_k == t2_row && col_k == t2_col)) {
                        cross_target_score += (std::popcount(mk) >= 3) ? 8 : 4;
                    }
                    if (row_k == row_i && col_k != col_i && col_k != col_j && topo.cell_box[static_cast<size_t>(ak)] != box_i) {
                        gate_score += 3;
                    }
                    if (col_k == col_j && row_k != row_i && row_k != row_j && topo.cell_box[static_cast<size_t>(ak)] != box_j) {
                        gate_score += 3;
                    }
                }
            }
        }
    }

    score += std::min(base_pair_score, 48);
    score += std::min(cross_target_score, 24);
    score += std::min(gate_score, senior_mode ? 24 : 16);
    if (senior_mode && plan.anchor_count >= 6) score += 10;
    return score;
}

inline int score_skloop_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::LoopLike, plan);
    if (score < 0) return score;

    int rectangle_score = 0;
    int core_score = 0;
    int exit_score = 0;
    int wing_score = 0;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const int row_i = topo.cell_row[static_cast<size_t>(ai)];
        const int col_i = topo.cell_col[static_cast<size_t>(ai)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const int row_j = topo.cell_row[static_cast<size_t>(aj)];
            const int col_j = topo.cell_col[static_cast<size_t>(aj)];
            if (row_i == row_j || col_i == col_j) continue;

            int corners = 0;
            int bivalue_corners = 0;
            uint64_t core_mask = 0ULL;
            for (int k = 0; k < plan.anchor_count; ++k) {
                const int ak = plan.anchor_idx[static_cast<size_t>(k)];
                const int row_k = topo.cell_row[static_cast<size_t>(ak)];
                const int col_k = topo.cell_col[static_cast<size_t>(ak)];
                if (!((row_k == row_i || row_k == row_j) && (col_k == col_i || col_k == col_j))) continue;
                ++corners;
                const uint64_t mk = plan.anchor_masks[static_cast<size_t>(k)];
                if (std::popcount(mk) == 2) {
                    ++bivalue_corners;
                    core_mask = (core_mask == 0ULL) ? mk : (core_mask & mk);
                } else if (std::popcount(mk) >= 3) {
                    exit_score += 3;
                }
            }
            if (corners >= 4) rectangle_score += 16;
            if (bivalue_corners >= 2 && std::popcount(core_mask) >= 2) core_score += 12;
        }
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mi) != 3) continue;
        const int row_i = topo.cell_row[static_cast<size_t>(ai)];
        const int col_i = topo.cell_col[static_cast<size_t>(ai)];
        for (int j = 0; j < plan.anchor_count; ++j) {
            if (i == j) continue;
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const uint64_t mj = plan.anchor_masks[static_cast<size_t>(j)];
            if (std::popcount(mj) != 2) continue;
            const int row_j = topo.cell_row[static_cast<size_t>(aj)];
            const int col_j = topo.cell_col[static_cast<size_t>(aj)];
            if (row_i == row_j || col_i == col_j) {
                const uint64_t inter = mi & mj;
                if (std::popcount(inter) == 2) wing_score += 2;
            }
        }
    }

    score += std::min(rectangle_score, 32);
    score += std::min(core_score, 24);
    score += std::min(exit_score, 16);
    score += std::min(wing_score, 12);
    return score;
}

inline int score_forcing_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    bool dynamic_mode) {
    int score = score_generic_exact_plan(topo, PatternKind::ForcingLike, plan);
    if (score < 0) return score;

    int bivalue_nodes = 0;
    int trivalue_nodes = 0;
    int link_score = 0;
    int branch_score = 0;
    int degree_hist[64]{};

    for (int i = 0; i < plan.anchor_count; ++i) {
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        const int bits = std::popcount(mi);
        if (bits == 2) ++bivalue_nodes;
        else if (bits == 3) ++trivalue_nodes;
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        int degree = 0;
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const uint64_t mj = plan.anchor_masks[static_cast<size_t>(j)];
            const bool sees =
                topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)] ||
                topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)];
            if (!sees) continue;
            const int overlap = std::popcount(mi & mj);
            if (overlap <= 0) continue;
            link_score += (overlap == 1) ? 4 : 2;
            ++degree;
            ++degree_hist[static_cast<size_t>(j)];
        }
        degree_hist[static_cast<size_t>(i)] += degree;
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int deg = degree_hist[static_cast<size_t>(i)];
        if (deg >= 2) branch_score += 3;
        if (deg >= 3) branch_score += 4;
    }

    score += 5 * bivalue_nodes + 3 * trivalue_nodes;
    score += std::min(link_score, dynamic_mode ? 36 : 28);
    score += std::min(branch_score, dynamic_mode ? 28 : 18);
    if (dynamic_mode && plan.anchor_count >= 6) score += 8;
    return score;
}

inline int score_exact_plan(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    PatternKind kind,
    const ExactPatternTemplatePlan& plan) {
    switch (required_strategy) {
    case RequiredStrategy::Exocet:
        return score_exocet_exact_plan(topo, plan, false);
    case RequiredStrategy::SeniorExocet:
        return score_exocet_exact_plan(topo, plan, true);
    case RequiredStrategy::SKLoop:
        return score_skloop_exact_plan(topo, plan);
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
        return score_forcing_exact_plan(topo, plan, false);
    case RequiredStrategy::DynamicForcingChains:
        return score_forcing_exact_plan(topo, plan, true);
    default:
        return score_generic_exact_plan(topo, kind, plan);
    }
}

inline uint64_t pick_random_bit_from_mask(uint64_t mask, std::mt19937_64& rng) {
    const int bits = std::popcount(mask);
    if (bits <= 0) return 0ULL;
    int pick = static_cast<int>(rng() % static_cast<uint64_t>(bits));
    uint64_t work = mask;
    while (work != 0ULL) {
        const int d0 = std::countr_zero(work);
        const uint64_t bit = 1ULL << d0;
        if (pick == 0) return bit;
        work &= ~bit;
        --pick;
    }
    return 0ULL;
}

inline uint64_t random_extra_digit(uint64_t full, uint64_t base, std::mt19937_64& rng) {
    uint64_t avail = full & ~base;
    if (avail == 0ULL) return 0ULL;
    return pick_random_bit_from_mask(avail, rng);
}

inline bool mutate_exocet_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    bool senior_mode,
    int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    const uint64_t base_mask = plan.anchor_masks[0] & plan.anchor_masks[1];
    bool changed = false;

    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = 2 + static_cast<int>(rng() % static_cast<uint64_t>(std::max(1, plan.anchor_count - 2)));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)];
        uint64_t extra = mask & ~base_mask;
        if (extra == 0ULL) {
            extra = random_extra_digit(full, base_mask, rng);
        } else if ((rng() & 1ULL) == 0ULL || std::popcount(extra) <= 1) {
            extra = random_extra_digit(full, base_mask, rng);
        } else {
            extra = pick_random_bit_from_mask(extra, rng);
        }
        uint64_t mutated = base_mask | extra;
        if (senior_mode && std::popcount(mutated) < 3) {
            mutated |= random_extra_digit(full, mutated, rng);
        }
        mutated &= full;
        if (mutated != 0ULL && mutated != mask) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mutated;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_skloop_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    uint64_t core = 0ULL;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mask) == 2) {
            core = (core == 0ULL) ? mask : (core & mask);
        }
    }
    if (core == 0ULL) core = plan.anchor_masks[1];

    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)];
        uint64_t mutated = mask;
        if (std::popcount(mask) == 2) {
            mutated = core;
        } else {
            mutated = core | random_extra_digit(full, core, rng);
        }
        if (std::popcount(mutated) < 3 && (rng() & 1ULL) != 0ULL) {
            mutated |= random_extra_digit(full, mutated, rng);
        }
        mutated &= full;
        if (mutated != 0ULL && mutated != mask) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mutated;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_forcing_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    bool dynamic_mode,
    int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    uint64_t union_mask = 0ULL;
    for (int i = 0; i < plan.anchor_count; ++i) {
        union_mask |= plan.anchor_masks[static_cast<size_t>(i)];
    }
    if (union_mask == 0ULL) union_mask = full;

    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        const uint64_t old_mask = plan.anchor_masks[static_cast<size_t>(idx)];
        uint64_t first = pick_random_bit_from_mask(union_mask, rng);
        uint64_t second = pick_random_bit_from_mask(union_mask & ~first, rng);
        if (second == 0ULL) second = random_extra_digit(full, first, rng);
        uint64_t mutated = first | second;
        if (dynamic_mode || ((rng() & 1ULL) != 0ULL)) {
            mutated |= random_extra_digit(full, mutated, rng);
        }
        mutated &= full;
        if (std::popcount(mutated) < 2) {
            mutated |= random_extra_digit(full, mutated, rng);
        }
        if (mutated != 0ULL && mutated != old_mask) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mutated;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_exact_template_for_family(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    switch (required_strategy) {
    case RequiredStrategy::Exocet:
        return mutate_exocet_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::SeniorExocet:
        return mutate_exocet_template_plan(topo, plan, rng, true, strength);
    case RequiredStrategy::SKLoop:
        return mutate_skloop_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::DynamicForcingChains:
        return mutate_forcing_template_plan(topo, plan, rng, true, strength);
    default:
        return false;
    }
}

inline void note_template_attempt_feedback(
    RequiredStrategy required_strategy,
    PatternKind kind,
    bool exact_template,
    int template_score,
    int required_analyzed,
    int required_use,
    int required_hit) {
    PatternMutationState& state = tls_pattern_mutation_state();
    if (state.strategy != required_strategy || state.kind != kind) {
        state.reset(required_strategy, kind);
    }
    if (!exact_template) {
        if (required_use > 0 || required_hit > 0) {
            state.zero_use_streak = 0;
            state.failure_streak = 0;
        }
        return;
    }
    if (required_analyzed > 0 && required_use == 0) {
        ++state.zero_use_streak;
    } else if (required_use > 0) {
        state.zero_use_streak = 0;
    }
    if (required_hit > 0) {
        state.failure_streak = 0;
    } else {
        ++state.failure_streak;
    }
    if (template_score > state.best_score) {
        state.best_score = template_score;
    }
}

inline bool try_exact_templates_for_level(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    int forcing_level,
    std::mt19937_64& rng,
    ExactPatternTemplatePlan& exact_plan,
    PatternKind& out_kind,
    int* out_score = nullptr) {
    exact_plan = {};
    out_kind = PatternKind::None;
    int best_score = -1;

    auto consider = [&](PatternKind candidate_kind, const ExactPatternTemplatePlan& candidate_plan) {
        const int score = score_exact_plan(topo, required_strategy, candidate_kind, candidate_plan);
        if (score > best_score) {
            best_score = score;
            exact_plan = candidate_plan;
            out_kind = candidate_kind;
        }
    };

    auto try_exocet = [&](bool senior_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateExocet::build(topo, rng, candidate, senior_mode)) {
            consider(PatternKind::ExocetLike, candidate);
        }
    };
    auto try_sk = [&](bool dense_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateSKLoop::build(topo, rng, candidate, dense_mode)) {
            consider(PatternKind::LoopLike, candidate);
        }
    };
    auto try_msls = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateMslsOverlay::build_msls(topo, rng, candidate)) {
            consider(PatternKind::LoopLike, candidate);
        }
    };
    auto try_overlay = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateMslsOverlay::build_overlay(topo, rng, candidate)) {
            consider(PatternKind::LoopLike, candidate);
        }
    };
    auto try_forcing = [&](bool dynamic_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateForcing::build(topo, rng, candidate, dynamic_mode)) {
            consider(PatternKind::ForcingLike, candidate);
        }
    };

    switch (required_strategy) {
        case RequiredStrategy::Exocet:
            try_exocet(false);
            break;
        case RequiredStrategy::SeniorExocet:
            try_exocet(true);
            break;
        case RequiredStrategy::SKLoop:
            try_sk(true);
            break;
        case RequiredStrategy::MSLS:
            try_msls();
            break;
        case RequiredStrategy::PatternOverlayMethod:
            try_overlay();
            break;
        case RequiredStrategy::ForcingChains:
            try_forcing(false);
            break;
        case RequiredStrategy::DynamicForcingChains:
            try_forcing(true);
            break;
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
            try_forcing(false);
            break;
        default:
            break;
    }

    if (forcing_level >= 8) {
        try_exocet(false);
        try_exocet(true);
        try_msls();
        try_overlay();
        try_sk(true);
        try_forcing(true);
    } else if (forcing_level >= 7) {
        try_msls();
        try_overlay();
        try_sk(true);
        try_forcing(false);
        try_exocet(false);
    }

    if (out_score != nullptr) *out_score = best_score;
    return best_score >= 0 && exact_plan.valid;
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
    PatternMutationState& mutation = tls_pattern_mutation_state();
    if (mutation.strategy != required_strategy || mutation.kind != kind) {
        mutation.reset(required_strategy, kind);
    }

    ExactPatternTemplatePlan exact_plan{};
    PatternKind exact_kind = PatternKind::None;
    int exact_score = -1;
    const bool exact_matched =
        try_exact_templates_for_level(topo, required_strategy, forcing_level, rng, exact_plan, exact_kind, &exact_score);

    if (mutation.have_last && mutation.strategy == required_strategy && mutation.kind == kind &&
        mutation.last_plan.valid && (mutation.failure_streak > 0 || mutation.zero_use_streak > 0)) {
        ExactPatternTemplatePlan mutated = mutation.last_plan;
        const int strength = std::clamp(1 + mutation.zero_use_streak + mutation.failure_streak / 2, 1, 4);
        if (mutate_exact_template_for_family(topo, required_strategy, mutated, rng, strength)) {
            const int mutated_score = score_exact_plan(topo, required_strategy, kind, mutated);
            if (mutated_score >= std::max(exact_score - 6, 0)) {
                exact_plan = mutated;
                exact_kind = kind;
                exact_score = mutated_score;
            }
        }
    }
    if (mutation.have_best && mutation.strategy == required_strategy && mutation.kind == kind &&
        mutation.best_plan.valid && mutation.zero_use_streak >= 2) {
        ExactPatternTemplatePlan mutated = mutation.best_plan;
        const int strength = std::clamp(1 + mutation.zero_use_streak / 2, 1, 4);
        if (mutate_exact_template_for_family(topo, required_strategy, mutated, rng, strength)) {
            const int mutated_score = score_exact_plan(topo, required_strategy, kind, mutated);
            if (mutated_score > exact_score) {
                exact_plan = mutated;
                exact_kind = kind;
                exact_score = mutated_score;
            }
        }
    }

    // Aplikacja masek z Exact Planu (jeśli się udał)
    if ((exact_matched || exact_score >= 0) && exact_plan.valid && exact_plan.anchor_count > 0) {
        sc.exact_template = true;
        sc.template_score = exact_score;
        for (int i = 0; i < exact_plan.anchor_count; ++i) {
            const int idx = exact_plan.anchor_idx[static_cast<size_t>(i)];
            if (!sc.add_anchor(idx)) continue;

            uint64_t mask = exact_plan.anchor_masks[static_cast<size_t>(i)];
            if (mask == 0ULL) mask = pf_full_mask_for_n(topo.n);
            
            sc.allowed_masks[static_cast<size_t>(idx)] = mask;
            sc.anchor_masks[static_cast<size_t>(i)] = mask;
        }

        // Chronione komórki zapobiegają ich usunięciu przez MCTS Digger
        if (cfg.pattern_forcing_lock_anchors) {
            for (int i = 0; i < sc.anchor_count; ++i) {
                const int idx = sc.anchors[static_cast<size_t>(i)];
                sc.protected_cells[static_cast<size_t>(idx)] = 1;
            }
        }
        
        if (sc.anchor_count > 0) {
            mutation.strategy = required_strategy;
            mutation.kind = (exact_kind == PatternKind::None) ? kind : exact_kind;
            mutation.have_last = true;
            mutation.last_plan = exact_plan;
            mutation.last_score = exact_score;
            if (!mutation.have_best || exact_score > mutation.best_score) {
                mutation.have_best = true;
                mutation.best_plan = exact_plan;
                mutation.best_score = exact_score;
            }
            out.kind = (exact_kind == PatternKind::None) ? kind : exact_kind;
            out.seed_puzzle = &sc.seed_puzzle;
            out.allowed_masks = &sc.allowed_masks;
            out.protected_cells = &sc.protected_cells;
            out.anchor_idx = sc.anchors.data();
            out.anchor_masks = sc.anchor_masks.data();
            out.anchor_count = sc.anchor_count;
            out.exact_template = sc.exact_template;
            out.template_score = sc.template_score;
            out.best_template_score = mutation.best_score;
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
    for (int i = 0; i < sc.anchor_count; ++i) {
        const int idx = sc.anchors[static_cast<size_t>(i)];
        sc.anchor_masks[static_cast<size_t>(i)] = sc.allowed_masks[static_cast<size_t>(idx)];
    }
    
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
    out.anchor_idx = sc.anchors.data();
    out.anchor_masks = sc.anchor_masks.data();
    out.anchor_count = sc.anchor_count;
    out.exact_template = false;
    out.template_score = 0;
    out.best_template_score = mutation.best_score;
    return true;
}

} // namespace sudoku_hpc::pattern_forcing
