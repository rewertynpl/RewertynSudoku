//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <random>
#include <vector>

#include "../../core/board.h"
#include "../../config/run_config.h"

#include "template_exocet.h"
#include "template_sk_loop.h"
#include "template_forcing.h"
#include "template_msls_overlay.h"
#include "template_p6_exact.h"

namespace sudoku_hpc::pattern_forcing {

// Typy wzorcĂłw (do losowych "luĹşnych" kotwic)
enum class PatternKind : uint8_t {
    None = 0,
    Chain,
    ExocetLike,
    LoopLike,
    ForcingLike,
    ColorLike,
    PetalLike,
    IntersectionLike,
    FishLike,
    FrankenLike,
    MutantLike,
    SquirmLike,
    AlsLike,
    ExclusionLike,
    AicLike,
    GroupedAicLike,
    GroupedCycleLike,
    NiceLoopLike,
    XChainLike,
    XYChainLike,
    EmptyRectangleLike,
    RemotePairsLike,
    SwordfishLike,
    JellyfishLike,
    FinnedFishLike
};

enum class PatternMutationSource : uint8_t {
    Random = 0,
    Last,
    Best
};

// Mapowanie z flagi w main()
enum class TargetPattern : uint8_t {
    None = 0,
    XChain,
    XYChain,
    Exocet,
    MSLS
};

// Bezpieczny widok przekazywany na zewnÄ…trz bez kopiowania wektorĂłw
struct PatternSeedView {
    PatternKind kind = PatternKind::None;
    PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;
    const std::vector<uint16_t>* seed_puzzle = nullptr;
    const std::vector<uint64_t>* allowed_masks = nullptr;
    const std::vector<uint8_t>* protected_cells = nullptr;
    const int* anchor_idx = nullptr;
    const uint64_t* anchor_masks = nullptr;
    int anchor_count = 0;
    bool exact_template = false;
    bool family_fallback_used = false;
    bool required_strategy_exact_contract_met = false;
    int template_score = 0;
    int best_template_score = 0;
    int template_score_delta = 0;
    int mutation_strength = 0;
    int planner_zero_use_streak = 0;
    int planner_failure_streak = 0;
    int adaptive_target_strength = 0;
    PatternMutationSource mutation_source = PatternMutationSource::Random;
};

// Bufor wspĂłĹ‚dzielony dla kaĹĽdego wÄ…tku (Thread Local Storage)
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
    thread_local PatternScratch s;
    return s;
}

inline bool pattern_required_is_fragile_candidate_structure(RequiredStrategy required_strategy) {
    switch (required_strategy) {
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::FinnedSwordfishJellyfish:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::UniqueLoop:
    case RequiredStrategy::AvoidableRectangle:
    case RequiredStrategy::BivalueOddagon:
    case RequiredStrategy::UniqueRectangleExtended:
    case RequiredStrategy::HiddenUniqueRectangle:
    case RequiredStrategy::BUGType2:
    case RequiredStrategy::BUGType3:
    case RequiredStrategy::BUGType4:
    case RequiredStrategy::BorescoperQiuDeadlyPattern:
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::MSLS:
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::RemotePairs:
    case RequiredStrategy::SimpleColoring:
        return true;
    default:
        return false;
    }
}

inline void protect_pattern_cells_from_masks(
    const GenericTopology& topo,
    PatternScratch& sc) {
    for (int idx = 0; idx < topo.nn; ++idx) {
        if (sc.seed_puzzle[static_cast<size_t>(idx)] != 0U) {
            sc.protected_cells[static_cast<size_t>(idx)] = 1;
        }
    }
}

inline uint64_t truncate_mask_bits(uint64_t mask, int keep_bits) {
    if (mask == 0ULL || keep_bits <= 0) return mask;
    if (std::popcount(mask) <= keep_bits) return mask;
    uint64_t out = 0ULL;
    for (int kept = 0; kept < keep_bits && mask != 0ULL; ++kept) {
        const int bit = std::countr_zero(mask);
        out |= (1ULL << bit);
        mask &= (mask - 1ULL);
    }
    return out;
}

inline uint64_t exact_family_core_mask(
    const ExactPatternTemplatePlan& plan,
    int begin_idx,
    int end_idx) {
    uint64_t core = 0ULL;
    for (int i = begin_idx; i < end_idx; ++i) {
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        core = (core == 0ULL) ? mask : (core & mask);
    }
    return core;
}

inline void expand_exact_template_skeleton(
    const GenericTopology& topo,
    PatternKind kind,
    ExactPatternTemplatePlan& plan) {
    if (!plan.valid || plan.anchor_count <= 0) return;
    if (plan.explicit_skeleton) return;

    const uint64_t full = pf_full_mask_for_n(topo.n);
    const int n = topo.n;

    int base_core = 0;
    int core_count = 0;
    int extra_cap = 8;
    int keep_bits = 3;

    switch (kind) {
    case PatternKind::ExocetLike:
        core_count = std::min(plan.anchor_count, 4);
        extra_cap = 14;
        keep_bits = 3;
        break;
    case PatternKind::LoopLike:
        core_count = std::min(plan.anchor_count, 4);
        extra_cap = 12;
        keep_bits = 3;
        break;
    case PatternKind::ForcingLike:
    case PatternKind::AicLike:
    case PatternKind::GroupedAicLike:
    case PatternKind::GroupedCycleLike:
    case PatternKind::NiceLoopLike:
    case PatternKind::XChainLike:
    case PatternKind::XYChainLike:
    case PatternKind::EmptyRectangleLike:
    case PatternKind::RemotePairsLike:
        core_count = std::min(plan.anchor_count, 4);
        extra_cap = 10;
        keep_bits = 3;
        break;
    case PatternKind::ColorLike:
        core_count = std::min(plan.anchor_count, 6);
        extra_cap = 10;
        keep_bits = 3;
        break;
    case PatternKind::PetalLike:
        core_count = std::min(plan.anchor_count, 4);
        extra_cap = 10;
        keep_bits = 3;
        break;
    case PatternKind::IntersectionLike:
        core_count = std::min(plan.anchor_count, 6);
        extra_cap = 12;
        keep_bits = 4;
        break;
    case PatternKind::FishLike:
    case PatternKind::FrankenLike:
    case PatternKind::MutantLike:
    case PatternKind::SquirmLike:
    case PatternKind::SwordfishLike:
    case PatternKind::JellyfishLike:
    case PatternKind::FinnedFishLike:
        core_count = std::min(plan.anchor_count, 6);
        extra_cap = 12;
        keep_bits = 3;
        break;
    case PatternKind::AlsLike:
        core_count = std::min(plan.anchor_count, 4);
        extra_cap = 10;
        keep_bits = 4;
        break;
    case PatternKind::ExclusionLike:
        core_count = std::min(plan.anchor_count, 3);
        extra_cap = 8;
        keep_bits = 3;
        break;
    default:
        core_count = std::min(plan.anchor_count, 4);
        break;
    }

    for (int i = 0; i < core_count; ++i) {
        plan.add_skeleton(
            plan.anchor_idx[static_cast<size_t>(i)],
            plan.anchor_masks[static_cast<size_t>(i)] & full);
    }

    base_core = exact_family_core_mask(plan, 0, std::max(core_count, 1));
    if (base_core == 0ULL) {
        for (int i = 0; i < core_count; ++i) {
            base_core |= truncate_mask_bits(plan.anchor_masks[static_cast<size_t>(i)], keep_bits);
        }
        base_core = truncate_mask_bits(base_core, keep_bits);
    }
    if (base_core == 0ULL) base_core = truncate_mask_bits(plan.anchor_masks[0], keep_bits);

    int row_count[64]{};
    int col_count[64]{};
    int box_count[64]{};
    uint64_t row_union[64]{};
    uint64_t col_union[64]{};
    uint64_t box_union[64]{};

    for (int i = 0; i < core_count; ++i) {
        const int idx = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)] & full;
        const int row = topo.cell_row[static_cast<size_t>(idx)];
        const int col = topo.cell_col[static_cast<size_t>(idx)];
        const int box = topo.cell_box[static_cast<size_t>(idx)];
        ++row_count[row];
        ++col_count[col];
        ++box_count[box];
        row_union[row] |= mask;
        col_union[col] |= mask;
        box_union[box] |= mask;
    }

    int added = 0;
    for (int pass = 0; pass < 2 && added < extra_cap; ++pass) {
        const int need_houses = (pass == 0) ? 2 : 1;
        for (int idx = 0; idx < topo.nn && added < extra_cap; ++idx) {
            if (plan.find_anchor(idx) >= 0 || plan.find_skeleton(idx) >= 0) continue;

            const int row = topo.cell_row[static_cast<size_t>(idx)];
            const int col = topo.cell_col[static_cast<size_t>(idx)];
            const int box = topo.cell_box[static_cast<size_t>(idx)];

            int active_houses = 0;
            uint64_t mask = 0ULL;

            if (row_count[row] >= 2) {
                ++active_houses;
                mask |= truncate_mask_bits(row_union[row] | base_core, keep_bits);
            }
            if (col_count[col] >= 2) {
                ++active_houses;
                mask |= truncate_mask_bits(col_union[col] | base_core, keep_bits);
            }
            if (box_count[box] >= 2) {
                ++active_houses;
                mask |= truncate_mask_bits(box_union[box] | base_core, keep_bits);
            }

            if (active_houses < need_houses) continue;
            mask &= full;
            mask = truncate_mask_bits(mask, keep_bits);
            if (mask == 0ULL || mask == full || std::popcount(mask) < 2) continue;
            if (plan.add_skeleton(idx, mask)) ++added;
        }
    }
}

inline void protect_exact_template_skeleton(
    PatternScratch& sc,
    const ExactPatternTemplatePlan& plan,
    RequiredStrategy required_strategy) {
    const bool is_fragile_pattern = pattern_required_is_fragile_candidate_structure(required_strategy);

    for (int i = 0; i < plan.skeleton_count; ++i) {
        const int idx = plan.skeleton_idx[static_cast<size_t>(i)];
        if (idx < 0 || idx >= sc.prepared_nn) continue;
        if (plan.find_anchor(idx) >= 0) {
            continue;
        }
        if (is_fragile_pattern) {
            continue;
        }
        sc.protected_cells[static_cast<size_t>(idx)] = 1;
    }
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
    thread_local PatternMutationState s;
    return s;
}

// Funkcja pomocnicza - zwraca maskÄ™ zĹ‚oĹĽonÄ… z `want` unikalnych losowych cyfr
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

} // namespace sudoku_hpc::pattern_forcing
