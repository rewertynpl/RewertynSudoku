// ============================================================================
// SUDOKU HPC - MCTS DIGGER
// Moduł: bottleneck_digger.h
// Opis: Główny silnik kopacza (Digger) używający Monte Carlo Tree Search.
//       Szuka logicznych "wąskich gardeł" (bottlenecks) poprzez symulacje usuwania.
//       Zero-Allocation w gorącej pętli.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "mcts_node.h"
#include "mcts_ucb_policy.h"

// Zależności do głównych silników (zostaną dostarczone w fazach 3 i 5)
#include "../../core/board.h"
#include "../../config/run_config.h"
#include "../../utils/logging.h"
#include "../core_engines/dlx_solver.h" // GenericUniquenessCounter
#include "../../logic/sudoku_logic_engine.h" // GenericLogicCertify i GenericLogicCertifyResult

namespace sudoku_hpc::mcts_digger {

using core_engines::GenericUniquenessCounter;
using core_engines::SearchAbortControl;
using logic::GenericLogicCertify;
using logic::GenericLogicCertifyResult;

// Helper: mapuje enum RequiredStrategy na indeks tablicy statystyk w Certyfikatorze
inline bool mcts_required_strategy_slot(RequiredStrategy rs, size_t& out_slot) {
    return GenericLogicCertify::slot_from_required_strategy(rs, out_slot);
}

inline int mcts_count_protected_cells(const uint8_t* protected_cells, int nn) {
    if (protected_cells == nullptr) return 0;
    int count = 0;
    for (int i = 0; i < nn; ++i) {
        if (protected_cells[static_cast<size_t>(i)] != 0) ++count;
    }
    return count;
}

inline const char* mcts_termination_reason_label(int code) {
    switch (code) {
    case 1: return "target-reached";
    case 2: return "no-active";
    case 3: return "fail-cap";
    case 4: return "iter-cap";
    case 5: return "logic-timeout";
    default: return "unknown";
    }
}

inline int select_seeded_high_clue_action(
    const MctsNodeScratch& sc,
    std::mt19937_64& rng,
    double excess_ratio) {
    if (sc.active_count <= 0) {
        return -1;
    }
    std::uniform_real_distribution<double> jitter(-0.01, 0.01);
    int best_cell = -1;
    double best_score = -1.0e300;
    for (int i = 0; i < sc.active_count; ++i) {
        const int cell = sc.active_cells[static_cast<size_t>(i)];
        if (cell < 0) continue;
        const double prior = sc.prior_bonus[static_cast<size_t>(cell)];
        const uint32_t visits = sc.visits[static_cast<size_t>(cell)];
        const double avg_reward =
            (visits > 0U)
                ? (sc.reward_sum[static_cast<size_t>(cell)] / static_cast<double>(visits))
                : 0.0;
        const double novelty =
            (visits == 0U)
                ? 1.0
                : (1.0 / (1.0 + static_cast<double>(visits)));
        const double score =
            (-2.80 * excess_ratio * prior) +
            (0.60 * novelty) +
            (0.08 * avg_reward) +
            jitter(rng);
        if (score > best_score) {
            best_score = score;
            best_cell = cell;
        }
    }
    return best_cell;
}

inline int mcts_pattern_anchor_position(
    const int* pattern_anchor_idx,
    int pattern_anchor_count,
    int idx) {
    if (pattern_anchor_idx == nullptr || pattern_anchor_count <= 0) return -1;
    for (int i = 0; i < pattern_anchor_count; ++i) {
        if (pattern_anchor_idx[static_cast<size_t>(i)] == idx) return i;
    }
    return -1;
}

inline int mcts_count_filled_pattern_anchors(
    std::span<const uint16_t> puzzle,
    const int* pattern_anchor_idx,
    int begin_idx,
    int end_idx) {
    if (pattern_anchor_idx == nullptr) return 0;
    int count = 0;
    for (int i = begin_idx; i < end_idx; ++i) {
        const int idx = pattern_anchor_idx[static_cast<size_t>(i)];
        if (idx >= 0 && idx < static_cast<int>(puzzle.size()) && puzzle[static_cast<size_t>(idx)] != 0) {
            ++count;
        }
    }
    return count;
}

inline int select_seeded_anchor_release_action(
    const MctsNodeScratch& sc,
    std::span<const uint16_t> puzzle,
    const int* pattern_anchor_idx,
    int pattern_anchor_count,
    std::mt19937_64& rng) {
    if (pattern_anchor_idx == nullptr || pattern_anchor_count <= 0 || sc.active_count <= 0) {
        return -1;
    }

    std::uniform_real_distribution<double> jitter(-0.01, 0.01);
    int best_core = -1;
    double best_core_score = -1.0e300;
    int best_any = -1;
    double best_any_score = -1.0e300;

    for (int i = 0; i < sc.active_count; ++i) {
        const int cell = sc.active_cells[static_cast<size_t>(i)];
        if (cell < 0 || cell >= static_cast<int>(puzzle.size()) || puzzle[static_cast<size_t>(cell)] == 0) continue;
        const int anchor_pos = mcts_pattern_anchor_position(pattern_anchor_idx, pattern_anchor_count, cell);
        if (anchor_pos < 0) continue;

        const uint32_t visits = sc.visits[static_cast<size_t>(cell)];
        const double avg_reward =
            (visits > 0U)
                ? (sc.reward_sum[static_cast<size_t>(cell)] / static_cast<double>(visits))
                : 0.0;
        const double novelty =
            (visits == 0U)
                ? 1.0
                : (1.0 / (1.0 + static_cast<double>(visits)));
        const double base_score =
            (1.20 * sc.prior_bonus[static_cast<size_t>(cell)]) +
            (0.50 * novelty) +
            (0.08 * avg_reward) +
            jitter(rng);

        if (anchor_pos < std::min(pattern_anchor_count, 4)) {
            const double core_score = base_score + 0.80;
            if (core_score > best_core_score) {
                best_core_score = core_score;
                best_core = cell;
            }
        }
        if (base_score > best_any_score) {
            best_any_score = base_score;
            best_any = cell;
        }
    }

    return (best_core >= 0) ? best_core : best_any;
}

inline bool mcts_required_needs_core_anchor_driving(RequiredStrategy rs);
inline int mcts_required_core_anchor_count(RequiredStrategy rs, int pattern_anchor_count);
inline int mcts_required_core_keep_floor(RequiredStrategy rs);
inline int mcts_required_contract_max_filled_core_anchors(RequiredStrategy rs, bool near_target_window);

inline bool pre_evacuate_required_core_anchors(
    std::span<uint16_t> out_puzzle,
    const GenericTopology& topo,
    RequiredStrategy rs,
    int target_clues,
    const GenericUniquenessCounter& uniq,
    SearchAbortControl* budget,
    MctsNodeScratch& sc,
    const int* pattern_anchor_idx,
    int pattern_anchor_count,
    std::mt19937_64& rng,
    int& clues,
    int& fail_streak,
    int& termination_reason,
    int* accepted_removals,
    int* rejected_uniqueness) {
    const int core_anchor_count = mcts_required_core_anchor_count(rs, pattern_anchor_count);
    if (pattern_anchor_idx == nullptr || core_anchor_count < 2) {
        return true;
    }

    const int keep_floor = mcts_required_core_keep_floor(rs);
    const int release_budget = std::min(
        std::max(0, clues - target_clues),
        std::max(0, core_anchor_count - keep_floor));
    for (int pass = 0; pass < release_budget; ++pass) {
        const int filled_core_anchors = mcts_count_filled_pattern_anchors(
            std::span<const uint16_t>(out_puzzle.data(), out_puzzle.size()),
            pattern_anchor_idx,
            0,
            core_anchor_count);
        if (filled_core_anchors <= keep_floor || clues <= target_clues) {
            break;
        }

        const int idx = select_seeded_anchor_release_action(
            sc,
            std::span<const uint16_t>(out_puzzle.data(), out_puzzle.size()),
            pattern_anchor_idx,
            core_anchor_count,
            rng);
        if (idx < 0) {
            break;
        }
        if (out_puzzle[static_cast<size_t>(idx)] == 0) {
            sc.disable(idx);
            continue;
        }

        const uint16_t old = out_puzzle[static_cast<size_t>(idx)];
        out_puzzle[static_cast<size_t>(idx)] = 0;
        const int solutions = uniq.count_solutions_limit2(out_puzzle, topo, budget);
        if (solutions < 0) {
            out_puzzle[static_cast<size_t>(idx)] = old;
            termination_reason = 5;
            return false;
        }
        if (solutions != 1) {
            out_puzzle[static_cast<size_t>(idx)] = old;
            sc.update(idx, -5.0);
            sc.disable(idx);
            ++fail_streak;
            if (rejected_uniqueness != nullptr) {
                ++(*rejected_uniqueness);
            }
            continue;
        }

        clues -= 1;
        fail_streak = 0;
        sc.update(idx, 1.40);
        sc.disable(idx);
        if (accepted_removals != nullptr) {
            ++(*accepted_removals);
        }
    }

    return true;
}

inline double mcts_required_anchor_bonus(
    const GenericTopology& topo,
    int idx,
    const uint8_t* protected_cells) {
    if (protected_cells == nullptr) return 0.0;
    const int n = topo.n;
    const int row = topo.cell_row[static_cast<size_t>(idx)];
    const int col = topo.cell_col[static_cast<size_t>(idx)];
    const int box = topo.cell_box[static_cast<size_t>(idx)];

    int affinity = 0;
    for (int j = 0; j < topo.nn; ++j) {
        if (protected_cells[static_cast<size_t>(j)] == 0) continue;
        if (topo.cell_row[static_cast<size_t>(j)] == row) ++affinity;
        if (topo.cell_col[static_cast<size_t>(j)] == col) ++affinity;
        if (topo.cell_box[static_cast<size_t>(j)] == box) ++affinity;
    }
    affinity = std::min(affinity, std::max(2, n / 2));
    return 0.12 * static_cast<double>(affinity);
}

inline bool mcts_required_is_forcing_family(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::KrakenFish:
        return true;
    default:
        return false;
    }
}

inline bool mcts_required_is_loop_family(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::MSLS:
    case RequiredStrategy::Medusa3D:
        return true;
    default:
        return false;
    }
}

inline bool mcts_required_is_color_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::Medusa3D;
}

inline bool mcts_required_is_petal_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::DeathBlossom;
}

inline bool mcts_required_is_intersection_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::SueDeCoq;
}

inline bool mcts_required_is_exocet_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::Exocet || rs == RequiredStrategy::SeniorExocet;
}

inline bool mcts_required_is_fish_family(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::Squirmbag:
        return true;
    default:
        return false;
    }
}

inline bool mcts_required_is_als_family(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
        return true;
    default:
        return false;
    }
}

inline bool mcts_required_is_aic_family(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return true;
    default:
        return false;
    }
}

inline bool mcts_required_is_overlay_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::PatternOverlayMethod;
}

inline bool mcts_required_is_msls_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::MSLS;
}

inline bool mcts_required_is_skloop_family(RequiredStrategy rs) {
    return rs == RequiredStrategy::SKLoop;
}

inline bool mcts_required_needs_strict_contract(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
    case RequiredStrategy::SimpleColoring:
    case RequiredStrategy::XChain:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return true;
    default:
        return false;
    }
}

inline int mcts_required_min_uses_for_contract(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::EmptyRectangle:
        return 5;
    case RequiredStrategy::RemotePairs:
        return 6;
    case RequiredStrategy::XChain:
        return 3;
    case RequiredStrategy::SimpleColoring:
        return 2;
    case RequiredStrategy::WXYZWing:
        return 2;
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return 2;
    default:
        return 1;
    }
}

inline bool mcts_required_needs_core_anchor_driving(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return true;
    default:
        return false;
    }
}

inline int mcts_required_core_anchor_count(RequiredStrategy rs, int pattern_anchor_count) {
    switch (rs) {
    case RequiredStrategy::SueDeCoq:
        return std::min(pattern_anchor_count, 6);
    case RequiredStrategy::Exocet:
        return std::min(pattern_anchor_count, 4);
    case RequiredStrategy::SeniorExocet:
        return std::min(pattern_anchor_count, 4);
    case RequiredStrategy::ForcingChains:
        return std::min(pattern_anchor_count, 4);
    case RequiredStrategy::DynamicForcingChains:
        return std::min(pattern_anchor_count, 6);
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::DeathBlossom:
        return std::min(pattern_anchor_count, 4);
    default:
        return 0;
    }
}

inline int mcts_required_core_keep_floor(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::SueDeCoq:
        return 0;
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return 0;
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::DeathBlossom:
        return (rs == RequiredStrategy::DeathBlossom) ? 0 : 1;
    default:
        return 0;
    }
}

inline int mcts_required_contract_max_filled_core_anchors(RequiredStrategy rs, bool near_target_window) {
    switch (rs) {
    case RequiredStrategy::SueDeCoq:
        return 0;
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return 0;
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::DeathBlossom:
        return (rs == RequiredStrategy::DeathBlossom) ? 0 : (near_target_window ? 0 : 1);
    default:
        return 64;
    }
}

inline bool mcts_required_prefers_hit_only_near_target(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
        return true;
    default:
        return false;
    }
}

inline int mcts_required_max_basic_steps_without_use(RequiredStrategy rs, int n) {
    switch (rs) {
    case RequiredStrategy::SueDeCoq:
        return std::max(10, 2 * n);
    case RequiredStrategy::DeathBlossom:
        return std::max(12, 2 * n + 2);
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return std::max(14, 2 * n + 4);
    case RequiredStrategy::ForcingChains:
        return std::max(12, n + 4);
    case RequiredStrategy::DynamicForcingChains:
        return std::max(14, n + 6);
    case RequiredStrategy::MSLS:
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
        return std::max(16, 2 * n + 6);
    default:
        return 1 << 20;
    }
}

struct MctsRequiredRewardProfile {
    double hit_mult = 1.0;
    double use_bonus = 1.0;
    double miss_penalty = 1.0;
};

inline MctsRequiredRewardProfile mcts_required_reward_profile(RequiredStrategy rs) {
    MctsRequiredRewardProfile p{};
    switch (rs) {
    case RequiredStrategy::ALSXYWing:
        p.hit_mult = 1.15;
        p.use_bonus = 1.10;
        p.miss_penalty = 1.05;
        break;
    case RequiredStrategy::WXYZWing:
        p.hit_mult = 1.20;
        p.use_bonus = 1.10;
        p.miss_penalty = 1.08;
        break;
    case RequiredStrategy::ALSXZ:
        p.hit_mult = 1.28;
        p.use_bonus = 1.22;
        p.miss_penalty = 1.04;
        break;
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
        p.hit_mult = 1.25;
        p.use_bonus = 1.15;
        p.miss_penalty = 1.10;
        break;
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
        p.hit_mult = 1.22;
        p.use_bonus = 1.12;
        p.miss_penalty = 1.08;
        break;
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
        p.hit_mult = 1.18;
        p.use_bonus = 1.10;
        p.miss_penalty = 1.06;
        break;
    case RequiredStrategy::PatternOverlayMethod:
        p.hit_mult = 1.20;
        p.use_bonus = 1.08;
        p.miss_penalty = 1.10;
        break;
    case RequiredStrategy::Exocet:
        p.hit_mult = 1.30;
        p.use_bonus = 1.20;
        p.miss_penalty = 1.10;
        break;
    case RequiredStrategy::SeniorExocet:
        p.hit_mult = 1.34;
        p.use_bonus = 1.24;
        p.miss_penalty = 1.14;
        break;
    case RequiredStrategy::MSLS:
        p.hit_mult = 1.22;
        p.use_bonus = 1.08;
        p.miss_penalty = 1.12;
        break;
    case RequiredStrategy::SKLoop:
        p.hit_mult = 1.16;
        p.use_bonus = 1.08;
        p.miss_penalty = 1.08;
        break;
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::KrakenFish:
        p.hit_mult = 1.10;
        p.use_bonus = 1.06;
        p.miss_penalty = 1.04;
        break;
    case RequiredStrategy::SueDeCoq:
        p.hit_mult = 1.22;
        p.use_bonus = 1.18;
        p.miss_penalty = 1.12;
        break;
    case RequiredStrategy::SimpleColoring:
        p.hit_mult = 1.22;
        p.use_bonus = 1.04;
        p.miss_penalty = 1.16;
        break;
    case RequiredStrategy::EmptyRectangle:
        p.hit_mult = 1.32;
        p.use_bonus = 1.02;
        p.miss_penalty = 1.22;
        break;
    case RequiredStrategy::RemotePairs:
        p.hit_mult = 1.30;
        p.use_bonus = 1.02;
        p.miss_penalty = 1.24;
        break;
    case RequiredStrategy::XChain:
        p.hit_mult = 1.34;
        p.use_bonus = 1.03;
        p.miss_penalty = 1.26;
        break;
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        p.hit_mult = 1.28;
        p.use_bonus = 1.18;
        p.miss_penalty = 1.14;
        break;
    default:
        break;
    }
    return p;
}

inline double mcts_required_strategy_reward_bias(RequiredStrategy rs, double prior_bonus, bool basic_solved) {
    double bias = 0.0;
    if (mcts_required_is_als_family(rs)) {
        bias += 0.18 * prior_bonus;
        if (!basic_solved) bias += 0.45;
    } else if (mcts_required_is_exocet_family(rs)) {
        bias += 0.22 * prior_bonus;
        bias += basic_solved ? -0.40 : 0.55;
    } else if (rs == RequiredStrategy::ForcingChains) {
        bias += 0.20 * prior_bonus;
        bias += basic_solved ? -0.30 : 0.44;
    } else if (rs == RequiredStrategy::DynamicForcingChains) {
        bias += 0.22 * prior_bonus;
        bias += basic_solved ? -0.34 : 0.50;
    } else if (mcts_required_is_intersection_family(rs)) {
        bias += 0.18 * prior_bonus;
        bias += basic_solved ? -0.35 : 0.48;
    } else if (mcts_required_is_aic_family(rs)) {
        bias += 0.16 * prior_bonus;
        if (!basic_solved) bias += 0.35;
    } else if (mcts_required_is_overlay_family(rs)) {
        bias += 0.14 * prior_bonus;
        if (!basic_solved) bias += 0.30;
    } else if (mcts_required_is_msls_family(rs)) {
        bias += 0.14 * prior_bonus;
        if (!basic_solved) bias += 0.25;
    } else if (mcts_required_is_skloop_family(rs)) {
        bias += 0.15 * prior_bonus;
        if (!basic_solved) bias += 0.28;
    } else if (mcts_required_is_color_family(rs) || mcts_required_is_petal_family(rs) ||
               mcts_required_is_fish_family(rs)) {
        bias += 0.12 * prior_bonus;
        if (!basic_solved) bias += 0.20;
    }
    return bias;
}

inline double mcts_required_structural_family_bonus(
    const GenericTopology& topo,
    int idx,
    RequiredStrategy rs,
    const uint64_t* pattern_allowed_masks,
    const int* pattern_anchor_idx,
    const uint64_t* pattern_anchor_masks,
    int pattern_anchor_count,
    bool exact_pattern) {
    if (idx < 0 || idx >= topo.nn || pattern_anchor_idx == nullptr || pattern_anchor_masks == nullptr || pattern_anchor_count <= 0) {
        return 0.0;
    }

    const int row = topo.cell_row[static_cast<size_t>(idx)];
    const int col = topo.cell_col[static_cast<size_t>(idx)];
    const int box = topo.cell_box[static_cast<size_t>(idx)];
    int same_row = 0;
    int same_col = 0;
    int same_box = 0;
    int visible = 0;
    int tight = 0;
    for (int ai = 0; ai < pattern_anchor_count; ++ai) {
        const int anchor = pattern_anchor_idx[static_cast<size_t>(ai)];
        if (anchor < 0 || anchor >= topo.nn) continue;
        const int arow = topo.cell_row[static_cast<size_t>(anchor)];
        const int acol = topo.cell_col[static_cast<size_t>(anchor)];
        const int abox = topo.cell_box[static_cast<size_t>(anchor)];
        if (arow == row) ++same_row;
        if (acol == col) ++same_col;
        if (abox == box) ++same_box;
        if (arow == row || acol == col || abox == box) ++visible;
        if (std::popcount(pattern_anchor_masks[static_cast<size_t>(ai)]) <= 2) ++tight;
    }

    double bonus = 0.0;
    if (mcts_required_is_color_family(rs)) {
        if (same_row > 0 && same_col > 0) bonus += 0.40;
        if (same_box > 0) bonus += 0.20;
        bonus += 0.08 * std::min(tight, 4);
    } else if (rs == RequiredStrategy::ForcingChains || rs == RequiredStrategy::DynamicForcingChains) {
        const int anchor_pos = mcts_pattern_anchor_position(pattern_anchor_idx, pattern_anchor_count, idx);
        if (anchor_pos >= 0) {
            if (anchor_pos == 0 || anchor_pos == 3) bonus += 0.82;
            else if (anchor_pos == 1 || anchor_pos == 2) bonus += 0.62;
            else bonus += 0.36;
        }
        if (same_row > 0) bonus += 0.20;
        if (same_col > 0) bonus += 0.20;
        if (same_box > 0) bonus += 0.12;
        if (visible >= 2) bonus += 0.26;
        bonus += 0.06 * std::min(tight, 5);
        if (pattern_allowed_masks != nullptr) {
            const uint64_t local_mask = pattern_allowed_masks[static_cast<size_t>(idx)];
            const int bits = std::popcount(local_mask);
            if (bits == 2) bonus += 0.28;
            else if (bits == 3) bonus += 0.16;
        }
    } else if (mcts_required_is_exocet_family(rs)) {
        const int anchor_pos = mcts_pattern_anchor_position(pattern_anchor_idx, pattern_anchor_count, idx);
        if (anchor_pos >= 0) {
            if (anchor_pos < 2) bonus += 1.35;
            else if (anchor_pos < 4) bonus += 0.95;
            else bonus += 0.40;
        }
        if (same_row > 0) bonus += 0.28;
        if (same_col > 0) bonus += 0.28;
        if (same_box > 0) bonus += 0.18;
        if (visible >= 2) bonus += 0.24;
        bonus += 0.05 * std::min(tight, 5);
        if (pattern_allowed_masks != nullptr) {
            const uint64_t local_mask = pattern_allowed_masks[static_cast<size_t>(idx)];
            const int bits = std::popcount(local_mask);
            if (bits == 2) bonus += 0.32;
            else if (bits == 3) bonus += 0.18;
        }
    } else if (mcts_required_is_als_family(rs)) {
        if (visible >= 2) bonus += 0.35;
        if (same_row > 0 && same_col > 0) bonus += 0.18;
        bonus += 0.07 * std::min(tight, 5);
        if (rs == RequiredStrategy::ALSXZ) {
            uint64_t visible_common = 0ULL;
            for (int a = 0; a < pattern_anchor_count; ++a) {
                const int anchor_a = pattern_anchor_idx[static_cast<size_t>(a)];
                if (anchor_a < 0 || anchor_a >= topo.nn) continue;
                const int row_a = topo.cell_row[static_cast<size_t>(anchor_a)];
                const int col_a = topo.cell_col[static_cast<size_t>(anchor_a)];
                const int box_a = topo.cell_box[static_cast<size_t>(anchor_a)];
                if (!(row_a == row || col_a == col || box_a == box)) continue;
                for (int b = a + 1; b < pattern_anchor_count; ++b) {
                    const int anchor_b = pattern_anchor_idx[static_cast<size_t>(b)];
                    if (anchor_b < 0 || anchor_b >= topo.nn) continue;
                    const int row_b = topo.cell_row[static_cast<size_t>(anchor_b)];
                    const int col_b = topo.cell_col[static_cast<size_t>(anchor_b)];
                    const int box_b = topo.cell_box[static_cast<size_t>(anchor_b)];
                    if (!(row_b == row || col_b == col || box_b == box)) continue;
                    visible_common |= pattern_anchor_masks[static_cast<size_t>(a)] &
                                      pattern_anchor_masks[static_cast<size_t>(b)];
                }
            }
            if (same_row >= 2) bonus += 0.20;
            if (same_col >= 2) bonus += 0.24;
            if (visible >= 3) bonus += 0.22;
            if (same_box > 0) bonus += 0.08;
            if (pattern_allowed_masks != nullptr) {
                const uint64_t local_mask = pattern_allowed_masks[static_cast<size_t>(idx)];
                if ((local_mask & visible_common) != 0ULL) {
                    bonus += 0.24;
                }
                if (std::popcount(local_mask) == 2) {
                    bonus += 0.16;
                }
            }
        }
    } else if (mcts_required_is_aic_family(rs)) {
        if (visible >= 2) bonus += 0.30;
        if (same_row > 0 || same_col > 0 || same_box > 0) bonus += 0.16;
        bonus += 0.05 * std::min(tight, 5);
    } else if (mcts_required_is_petal_family(rs)) {
        if (same_row > 0 || same_col > 0 || same_box > 0) bonus += 0.30;
        if (visible >= 2) bonus += 0.25;
        bonus += 0.05 * std::min(pattern_anchor_count, 6);
        const int anchor_pos = mcts_pattern_anchor_position(pattern_anchor_idx, pattern_anchor_count, idx);
        if (anchor_pos == 0) bonus += 0.55;
        else if (anchor_pos > 0 && anchor_pos < 4) bonus += 0.42;
    } else if (mcts_required_is_overlay_family(rs)) {
        if (same_row > 0) bonus += 0.18;
        if (same_col > 0) bonus += 0.18;
        if (same_box > 0) bonus += 0.10;
        bonus += 0.05 * std::min(visible, 6);
    } else if (mcts_required_is_msls_family(rs)) {
        if (same_row > 0) bonus += 0.18;
        if (same_col > 0) bonus += 0.18;
        if (same_box > 0) bonus += 0.22;
        bonus += 0.05 * std::min(visible, 6);
    } else if (mcts_required_is_skloop_family(rs)) {
        if (same_row > 0 && same_col > 0) bonus += 0.28;
        if (same_box > 0) bonus += 0.10;
        bonus += 0.04 * std::min(visible, 6);
    } else if (mcts_required_is_intersection_family(rs)) {
        if (same_box > 0 && (same_row > 0 || same_col > 0)) bonus += 0.55;
        else if (same_box > 0) bonus += 0.20;
        if (same_box >= 3) bonus += 0.16;
        if (same_row >= 2 || same_col >= 2) bonus += 0.18;
        bonus += 0.05 * std::min(tight, 4);
        bonus += 0.06 * std::min(visible, 5);
        const int anchor_pos = mcts_pattern_anchor_position(pattern_anchor_idx, pattern_anchor_count, idx);
        if (anchor_pos == 0) bonus += 0.70;
        else if (anchor_pos == 3 || anchor_pos == 4) bonus += 0.48;
        else if (anchor_pos > 0) bonus += 0.34;
    } else if (mcts_required_is_fish_family(rs)) {
        if (same_row >= 2 || same_col >= 2) bonus += 0.45;
        if (same_box > 0) bonus += 0.10;
        bonus += 0.05 * std::min(tight, 5);
    }

    if (pattern_allowed_masks != nullptr) {
        const int local_bits = std::popcount(pattern_allowed_masks[static_cast<size_t>(idx)]);
        if (local_bits == 2) bonus += 0.25;
        else if (local_bits == 3) bonus += 0.12;
    }
    if (exact_pattern) {
        bonus += 0.10;
    }
    return bonus;
}

inline double mcts_large_geometry_required_bonus(
    const GenericTopology& topo,
    int idx,
    RequiredStrategy rs,
    const uint64_t* pattern_allowed_masks,
    bool exact_pattern) {
    if (topo.n < 25 || idx < 0 || idx >= topo.nn) {
        return 0.0;
    }

    const int row = topo.cell_row[static_cast<size_t>(idx)];
    const int col = topo.cell_col[static_cast<size_t>(idx)];
    const int row_in_box = (topo.box_rows > 0) ? (row % topo.box_rows) : 0;
    const int col_in_box = (topo.box_cols > 0) ? (col % topo.box_cols) : 0;
    const bool row_edge = (row_in_box == 0) || (row_in_box == topo.box_rows - 1);
    const bool col_edge = (col_in_box == 0) || (col_in_box == topo.box_cols - 1);
    const bool corner = row_edge && col_edge;

    const int box_r_count = std::max(1, topo.n / std::max(1, topo.box_rows));
    const int box_c_count = std::max(1, topo.n / std::max(1, topo.box_cols));
    const int box_r = row / std::max(1, topo.box_rows);
    const int box_c = col / std::max(1, topo.box_cols);
    const bool inner_box =
        box_r > 0 && box_r + 1 < box_r_count &&
        box_c > 0 && box_c + 1 < box_c_count;

    const double center_r = static_cast<double>(topo.n - 1) * 0.5;
    const double center_c = static_cast<double>(topo.n - 1) * 0.5;
    const double norm_r = 1.0 - (std::abs(static_cast<double>(row) - center_r) / std::max(1.0, center_r));
    const double norm_c = 1.0 - (std::abs(static_cast<double>(col) - center_c) / std::max(1.0, center_c));
    const double center_bonus = 0.12 * std::max(0.0, 0.5 * (norm_r + norm_c));

    double bonus = 0.0;
    if (mcts_required_is_forcing_family(rs)) {
        if (corner) bonus += 0.90;
        else if (row_edge || col_edge) bonus += 0.50;
        if (inner_box) bonus += 0.25;
        bonus += center_bonus;
    } else if (mcts_required_is_exocet_family(rs)) {
        if (corner) bonus += 0.18;
        else if (row_edge || col_edge) bonus += 0.24;
        if (inner_box) bonus += 0.48;
        bonus += 1.55 * center_bonus;
    } else if (mcts_required_is_loop_family(rs)) {
        if (corner) bonus += 0.55;
        else if (row_edge || col_edge) bonus += 0.30;
        if (inner_box) bonus += 0.35;
        bonus += 1.35 * center_bonus;
    } else {
        if (corner) bonus += 0.45;
        else if (row_edge || col_edge) bonus += 0.25;
        if (inner_box) bonus += 0.20;
        bonus += center_bonus;
    }

    if (pattern_allowed_masks != nullptr) {
        const uint64_t local_mask = pattern_allowed_masks[static_cast<size_t>(idx)];
        const int local_bits = std::popcount(local_mask);
        if (local_bits > 0 && local_bits < topo.n) {
            bonus += (local_bits <= 2) ? 0.85 : ((local_bits <= 4) ? 0.45 : 0.15);
        }
    } else {
        bonus += exact_pattern ? 0.20 : 0.08;
    }

    return bonus;
}

class GenericMctsBottleneckDigger {
public:
    struct RunStats {
        bool used = false;
        bool bottleneck_hit = false;
        int accepted_removals = 0;
        int rejected_uniqueness = 0;
        int rejected_contract = 0;
        int rejected_logic_timeout = 0;
        int skipped_target_floor = 0;
        int skipped_empty_or_disabled = 0;
        int iterations = 0;
        int advanced_evals = 0;
        int advanced_p7_hits = 0;
        int advanced_p8_hits = 0;
        int required_strategy_analyzed = 0;
        int required_strategy_uses = 0;
        int required_strategy_hits = 0;
        int target_clues = 0;
        int final_clues = 0;
        int final_fail_streak = 0;
        int final_active_count = 0;
        int protected_count = 0;
        int termination_reason = 0;
    };

    // Przeprowadza proces "kopania" na gotowej planszy (solved)
    bool dig_into(
        std::span<const uint16_t> solved,
        const GenericTopology& topo,
        const GenerateRunConfig& cfg,
        std::mt19937_64& rng,
        const GenericUniquenessCounter& uniq,
        const GenericLogicCertify& logic,
        std::span<uint16_t> out_puzzle,
        int& out_clues,
        const uint8_t* protected_cells = nullptr,
        const uint64_t* pattern_allowed_masks = nullptr,
        const int* pattern_anchor_idx = nullptr,
        const uint64_t* pattern_anchor_masks = nullptr,
        int pattern_anchor_count = 0,
        bool exact_pattern = false,
        SearchAbortControl* budget = nullptr,
        RunStats* stats = nullptr) const {

        if (stats != nullptr) {
            *stats = {};
            stats->used = true;
        }

        // Kopia startowa planszy
        if (static_cast<int>(solved.size()) != topo.nn || static_cast<int>(out_puzzle.size()) != topo.nn) {
            return false;
        }
        std::copy(solved.begin(), solved.end(), out_puzzle.begin());

        int min_clues = std::clamp(cfg.min_clues, 0, topo.nn);
        int max_clues = std::clamp(cfg.max_clues, min_clues, topo.nn);
        std::uniform_int_distribution<int> pick_target(min_clues, max_clues);
        const int target_clues = pick_target(rng);
        const bool large_required_geometry = (topo.n >= 25) && (cfg.required_strategy != RequiredStrategy::None);
        const bool trace_required_contract =
            (cfg.required_strategy != RequiredStrategy::None) &&
            (cfg.difficulty_level_required >= 8);
        const int protected_count = mcts_count_protected_cells(protected_cells, topo.nn);
        int termination_reason = 4;

        if (stats != nullptr) {
            stats->target_clues = target_clues;
            stats->protected_count = protected_count;
        }

        // Reset bufora MCTS (Zero-Allocation)
        MctsNodeScratch& sc = tls_mcts_node_scratch();
        sc.reset(topo.nn);

        // Aktywacja wszystkich niechronionych komórek
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (protected_cells != nullptr && protected_cells[static_cast<size_t>(idx)] != 0) {
                continue;
            }
            sc.activate(idx);
            if (cfg.required_strategy != RequiredStrategy::None) {
                double prior = mcts_required_anchor_bonus(topo, idx, protected_cells);
                if (large_required_geometry) {
                    prior += mcts_large_geometry_required_bonus(
                        topo, idx, cfg.required_strategy, pattern_allowed_masks, exact_pattern);
                }
                if (pattern_anchor_idx != nullptr && pattern_anchor_masks != nullptr && pattern_anchor_count > 0) {
                    int visible_anchors = 0;
                    double structural_bonus = 0.0;
                    for (int ai = 0; ai < pattern_anchor_count; ++ai) {
                        const int anchor = pattern_anchor_idx[static_cast<size_t>(ai)];
                        if (anchor < 0 || anchor >= topo.nn) continue;
                        const bool sees =
                            topo.cell_row[static_cast<size_t>(anchor)] == topo.cell_row[static_cast<size_t>(idx)] ||
                            topo.cell_col[static_cast<size_t>(anchor)] == topo.cell_col[static_cast<size_t>(idx)] ||
                            topo.cell_box[static_cast<size_t>(anchor)] == topo.cell_box[static_cast<size_t>(idx)];
                        if (!sees) continue;
                        ++visible_anchors;
                        const int bits = std::popcount(pattern_anchor_masks[static_cast<size_t>(ai)]);
                        structural_bonus += (bits <= 2) ? 0.35 : ((bits == 3) ? 0.22 : 0.10);
                    }

                    if (cfg.required_strategy == RequiredStrategy::ALSXZ) {
                        const int anchor_pos = mcts_pattern_anchor_position(pattern_anchor_idx, pattern_anchor_count, idx);
                        if (anchor_pos >= 0) {
                            structural_bonus += (anchor_pos < std::min(pattern_anchor_count, 4)) ? 1.15 : 0.55;
                        }
                    }

                    if (visible_anchors >= 2) {
                        structural_bonus += exact_pattern ? 0.45 : 0.20;
                    }

                    if (pattern_allowed_masks != nullptr) {
                        const uint64_t local_mask = pattern_allowed_masks[static_cast<size_t>(idx)];
                        const int local_bits = std::popcount(local_mask);
                        if (local_bits > 0 && local_bits < topo.n) {
                            structural_bonus += (local_bits <= 2) ? 0.35 : ((local_bits == 3) ? 0.20 : 0.08);
                        }
                    }

                    if (exact_pattern && pattern_anchor_count >= 2) {
                        for (int a = 0; a < pattern_anchor_count; ++a) {
                            const int anchor_a = pattern_anchor_idx[static_cast<size_t>(a)];
                            if (anchor_a < 0 || anchor_a >= topo.nn) continue;
                            const int row_a = topo.cell_row[static_cast<size_t>(anchor_a)];
                            const int col_a = topo.cell_col[static_cast<size_t>(anchor_a)];
                            for (int b = a + 1; b < pattern_anchor_count; ++b) {
                                const int anchor_b = pattern_anchor_idx[static_cast<size_t>(b)];
                                if (anchor_b < 0 || anchor_b >= topo.nn) continue;
                                const int row_b = topo.cell_row[static_cast<size_t>(anchor_b)];
                                const int col_b = topo.cell_col[static_cast<size_t>(anchor_b)];
                                if ((topo.cell_row[static_cast<size_t>(idx)] == row_a &&
                                     topo.cell_col[static_cast<size_t>(idx)] == col_b) ||
                                    (topo.cell_row[static_cast<size_t>(idx)] == row_b &&
                                     topo.cell_col[static_cast<size_t>(idx)] == col_a)) {
                                    structural_bonus += 0.10;
                                }
                            }
                        }
                    }
                    prior += structural_bonus;
                }
                prior += mcts_required_structural_family_bonus(
                    topo,
                    idx,
                    cfg.required_strategy,
                    pattern_allowed_masks,
                    pattern_anchor_idx,
                    pattern_anchor_masks,
                    pattern_anchor_count,
                    exact_pattern);
                sc.set_prior(idx, prior);
            }
        }

        int clues = topo.nn;
        int fail_streak = 0;
        const int iter_cap = (cfg.mcts_digger_iterations > 0) ? cfg.mcts_digger_iterations : std::max(256, topo.nn * 8);
        const int basic_level = std::clamp(cfg.mcts_basic_logic_level, 1, 5);
        const double ucb_c = std::clamp(cfg.mcts_ucb_c, 0.1, 4.0);
        
        const MctsAdvancedTuning tuning = resolve_mcts_advanced_tuning(cfg, topo);
        size_t required_slot = 0;
        const bool has_required_slot = mcts_required_strategy_slot(cfg.required_strategy, required_slot);
        const int required_level = has_required_slot
            ? static_cast<int>(GenericLogicCertify::strategy_meta_for_slot(required_slot).level)
            : 0;
        int advanced_level = std::clamp(std::max(6, std::max(cfg.difficulty_level_required, required_level)), 6, 8);
        if (cfg.max_pattern_depth > 0) {
            advanced_level = std::min(advanced_level, cfg.max_pattern_depth);
            advanced_level = std::clamp(advanced_level, 6, 8);
        }
        const bool wants_p8 = (cfg.difficulty_level_required >= 8) || mcts_is_level8_strategy(cfg.required_strategy);
        const MctsRequiredRewardProfile required_reward_profile = mcts_required_reward_profile(cfg.required_strategy);
        const bool seeded_high_clue_push =
            exact_pattern &&
            (wants_p8 || mcts_required_needs_core_anchor_driving(cfg.required_strategy)) &&
            has_required_slot &&
            (cfg.required_strategy != RequiredStrategy::None);
        const int fail_cap = std::max(16, cfg.mcts_fail_cap);
        if (mcts_required_needs_core_anchor_driving(cfg.required_strategy) &&
            exact_pattern &&
            pattern_anchor_idx != nullptr &&
            mcts_required_core_anchor_count(cfg.required_strategy, pattern_anchor_count) >= 2) {
            int* accepted_ptr = (stats != nullptr) ? &stats->accepted_removals : nullptr;
            int* rejected_uniqueness_ptr = (stats != nullptr) ? &stats->rejected_uniqueness : nullptr;
            if (!pre_evacuate_required_core_anchors(
                    std::span<uint16_t>(out_puzzle.data(), out_puzzle.size()),
                    topo,
                    cfg.required_strategy,
                    target_clues,
                    uniq,
                    budget,
                    sc,
                    pattern_anchor_idx,
                    pattern_anchor_count,
                    rng,
                    clues,
                    fail_streak,
                    termination_reason,
                    accepted_ptr,
                    rejected_uniqueness_ptr)) {
                return false;
            }
        }

        // Główna pętla MCTS
        for (int iter = 0; iter < iter_cap; ++iter) {
            if (stats != nullptr) stats->iterations = iter + 1;
            if (budget != nullptr && !budget->step()) return false;
            if (clues <= target_clues) {
                termination_reason = 1;
                break;
            }
            if (sc.active_count <= 0) {
                termination_reason = 2;
                break;
            }
            if (fail_streak >= fail_cap) {
                termination_reason = 3;
                break;
            }

            // Faza 1: Wybór akcji wg. UCB1
            int idx = -1;
            const int required_core_anchor_count =
                mcts_required_core_anchor_count(cfg.required_strategy, pattern_anchor_count);
            const int filled_core_anchors_pre =
                (required_core_anchor_count > 0)
                    ? mcts_count_filled_pattern_anchors(
                          std::span<const uint16_t>(out_puzzle.data(), out_puzzle.size()),
                          pattern_anchor_idx,
                          0,
                          required_core_anchor_count)
                    : 0;
            const bool required_anchor_release_needed =
                mcts_required_needs_core_anchor_driving(cfg.required_strategy) &&
                exact_pattern &&
                (filled_core_anchors_pre > mcts_required_core_keep_floor(cfg.required_strategy));
            if (seeded_high_clue_push && (clues > max_clues || required_anchor_release_needed)) {
                idx = select_seeded_anchor_release_action(
                    sc,
                    std::span<const uint16_t>(out_puzzle.data(), out_puzzle.size()),
                    pattern_anchor_idx,
                    pattern_anchor_count,
                    rng);
            }
            if (idx < 0 && seeded_high_clue_push && clues > max_clues && !required_anchor_release_needed) {
                const double excess_ratio =
                    static_cast<double>(clues - max_clues) /
                    static_cast<double>(std::max(1, topo.nn - max_clues));
                idx = select_seeded_high_clue_action(
                    sc,
                    rng,
                    std::clamp(excess_ratio, 0.0, 1.0));
            }
            if (idx < 0) {
                idx = select_ucb_action(sc, rng, ucb_c);
            }
            if (idx < 0) break;
            
            // Pusta komórka? Wyłącz ją.
            if (out_puzzle[static_cast<size_t>(idx)] == 0) {
                sc.disable(idx);
                if (stats != nullptr) ++stats->skipped_empty_or_disabled;
                continue;
            }

            int sym_idx = -1;
            bool remove_pair = false;
            
            // Sprawdzenie symetrii
            if (cfg.symmetry_center) {
                sym_idx = topo.cell_center_sym[static_cast<size_t>(idx)];
                if (sym_idx >= 0 && sym_idx != idx && out_puzzle[static_cast<size_t>(sym_idx)] != 0) {
                    if (!(protected_cells != nullptr && protected_cells[static_cast<size_t>(sym_idx)] != 0)) {
                        remove_pair = true;
                    }
                }
            }

            const int removal = remove_pair ? 2 : 1;
            if (clues - removal < target_clues) {
                sc.disable(idx);
                if (remove_pair) sc.disable(sym_idx);
                if (stats != nullptr) ++stats->skipped_target_floor;
                continue;
            }

            // Symulacja usunięcia
            const uint16_t old_a = out_puzzle[static_cast<size_t>(idx)];
            const uint16_t old_b = remove_pair ? out_puzzle[static_cast<size_t>(sym_idx)] : 0;
            
            out_puzzle[static_cast<size_t>(idx)] = 0;
            if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = 0;

            // Odrzucenie: brak unikalności (wielokrotne rozwiązania)
            const int solutions = uniq.count_solutions_limit2(out_puzzle, topo, budget);
            if (solutions < 0) { // Timeout w DLX
                out_puzzle[static_cast<size_t>(idx)] = old_a;
                if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                if (stats != nullptr) ++stats->rejected_logic_timeout;
                termination_reason = 5;
                return false;
            }
            if (solutions != 1) { // Strata unikalności -> kara dla węzła
                out_puzzle[static_cast<size_t>(idx)] = old_a;
                if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                
                sc.update(idx, -6.0); // Mocna kara za popsucie planszy
                sc.disable(idx);
                if (remove_pair) {
                    sc.update(sym_idx, -6.0);
                    sc.disable(sym_idx);
                }
                
                ++fail_streak;
                if (stats != nullptr) ++stats->rejected_uniqueness;
                continue;
            }

            // Faza 2: Ocena stanu Basic (Poziomy 1-5)
            const GenericLogicCertifyResult basic = logic.certify_up_to_level(out_puzzle, topo, basic_level, budget, false);
            if (basic.timed_out) {
                out_puzzle[static_cast<size_t>(idx)] = old_a;
                if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                if (stats != nullptr) ++stats->rejected_logic_timeout;
                termination_reason = 5;
                return false;
            }

            const bool basic_solved = basic.solved;
            
            // Faza 3: Wyznaczanie Nagrody (Reward Function)
            // Jeśli basic logic potrafi to rozwiązać, to nagroda jest mała (exploit).
            // Jeśli utknie, to nagroda jest gigantyczna, bo stworzyliśmy BOTTLENECK.
            double reward = basic_solved 
                ? (1.0 + 0.002 * static_cast<double>(std::max(0, basic.steps))) 
                : (18.0 + 0.005 * static_cast<double>(std::max(0, basic.steps)));

            int p7_hits = 0;
            int p8_hits = 0;
            int required_analyzed = 0;
            int required_hits = 0;
            int required_uses = 0;
            bool advanced_signal = false;
            bool stopping_signal = !basic_solved;

            // Faza 4: Certyfikacja zaawansowana (Opcjonalne dla wyższych poziomów MCTS Tuning)
            const bool do_advanced_eval = 
                tuning.enabled && 
                ((!basic_solved) || 
                 ((clues - removal) <= (target_clues + tuning.near_window)) || 
                 ((iter % tuning.eval_stride) == 0) ||
                 (large_required_geometry && has_required_slot && ((iter % std::max(1, tuning.eval_stride / 2)) == 0)));

            if (do_advanced_eval) {
                const GenericLogicCertifyResult adv = logic.certify_up_to_level(out_puzzle, topo, advanced_level, budget, false);
                if (adv.timed_out) {
                    out_puzzle[static_cast<size_t>(idx)] = old_a;
                    if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                    if (stats != nullptr) ++stats->rejected_logic_timeout;
                    return false;
                }

                // Analiza wyników P7 (Sloty Medusa3D do KrakenFish / ALSAIC)
                for (size_t slot = GenericLogicCertify::SlotMedusa3D; slot <= GenericLogicCertify::SlotALSAIC; ++slot) {
                    p7_hits += static_cast<int>(adv.strategy_stats[slot].hit_count);
                }
                
                // Analiza wyników P8 (Sloty MSLS do DynamicForcingChains)
                for (size_t slot = GenericLogicCertify::SlotMSLS; slot <= GenericLogicCertify::SlotDynamicForcingChains; ++slot) {
                    p8_hits += static_cast<int>(adv.strategy_stats[slot].hit_count);
                }

                if (has_required_slot) {
                    ++required_analyzed;
                    required_hits = static_cast<int>(adv.strategy_stats[required_slot].hit_count);
                    required_uses = static_cast<int>(adv.strategy_stats[required_slot].use_count);
                }

                const bool do_required_eval =
                    has_required_slot &&
                    required_level > 0 &&
                    required_hits == 0 &&
                    (((iter % std::max(1, tuning.eval_stride / 2)) == 0) ||
                     ((clues - removal) <= (target_clues + tuning.near_window)));

                if (do_required_eval) {
                    ++required_analyzed;
                    const GenericLogicCertifyResult req = logic.certify_up_to_level(
                        out_puzzle, topo, required_level, budget, false);
                    if (req.timed_out) {
                        out_puzzle[static_cast<size_t>(idx)] = old_a;
                        if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                        if (stats != nullptr) ++stats->rejected_logic_timeout;
                        termination_reason = 5;
                        return false;
                    }
                    required_hits = std::max(required_hits, static_cast<int>(req.strategy_stats[required_slot].hit_count));
                    required_uses = std::max(required_uses, static_cast<int>(req.strategy_stats[required_slot].use_count));
                }

                // Kalkulacja zysku (Backpropagation value)
                reward += tuning.p7_hit_weight * static_cast<double>(p7_hits);
                reward += tuning.p8_hit_weight * static_cast<double>(p8_hits);
                reward += (tuning.required_hit_weight * required_reward_profile.hit_mult) * static_cast<double>(required_hits);
                const int strict_required_min_uses = mcts_required_min_uses_for_contract(cfg.required_strategy);
                const bool prefer_hit_only_near_target = mcts_required_prefers_hit_only_near_target(cfg.required_strategy);
                const bool near_target_window = ((clues - removal) <= (target_clues + tuning.near_window));
                const int max_basic_steps_without_use =
                    mcts_required_max_basic_steps_without_use(cfg.required_strategy, topo.n);
                if (required_hits == 0 && required_uses >= strict_required_min_uses) {
                    reward += tuning.required_use_weight * required_reward_profile.use_bonus;
                } else if (required_hits == 0 && required_uses > 0 && mcts_required_needs_strict_contract(cfg.required_strategy)) {
                    reward -= 0.35 * tuning.required_use_weight * required_reward_profile.miss_penalty;
                }
                if (required_hits == 0 && prefer_hit_only_near_target && near_target_window) {
                    reward -= 0.85 * tuning.required_use_weight * required_reward_profile.miss_penalty;
                }
                if (has_required_slot && required_hits == 0 && required_uses == 0) {
                    reward -= (large_required_geometry ? 0.25 : 0.75) *
                              tuning.required_use_weight *
                              required_reward_profile.miss_penalty;
                }
                if (has_required_slot &&
                    required_hits == 0 &&
                    required_uses == 0 &&
                    basic.steps > max_basic_steps_without_use) {
                    reward -= 0.90 * tuning.required_use_weight * required_reward_profile.miss_penalty;
                }
                reward += mcts_required_strategy_reward_bias(
                    cfg.required_strategy,
                    sc.prior_bonus[static_cast<size_t>(idx)],
                    basic_solved);
                
                if (wants_p8 && p8_hits == 0 && required_hits == 0) {
                    reward -= (large_required_geometry ? 0.40 : 1.0) * tuning.p8_miss_penalty;
                }
                if (large_required_geometry) {
                    reward += 0.20 * sc.prior_bonus[static_cast<size_t>(idx)];
                    if (remove_pair) {
                        reward += 0.10 * sc.prior_bonus[static_cast<size_t>(sym_idx)];
                    }
                    if (!basic_solved) {
                        reward += 0.75;
                    }
                }
                
                reward = std::max(tuning.min_reward, reward);
                advanced_signal = (p7_hits + p8_hits + required_hits) > 0;

                const bool strict_required_contract = mcts_required_needs_strict_contract(cfg.required_strategy);
                const int filled_core_anchors =
                    (required_core_anchor_count > 0)
                        ? mcts_count_filled_pattern_anchors(
                              std::span<const uint16_t>(out_puzzle.data(), out_puzzle.size()),
                              pattern_anchor_idx,
                              0,
                              required_core_anchor_count)
                        : 0;
                const bool contract_reject =
                    has_required_slot &&
                    required_hits == 0 &&
                    (((required_uses == 0) && (basic_solved || (!advanced_signal && !large_required_geometry))) ||
                     ((required_uses == 0) && near_target_window) ||
                     (mcts_required_needs_core_anchor_driving(cfg.required_strategy) &&
                      (filled_core_anchors >
                       mcts_required_contract_max_filled_core_anchors(
                           cfg.required_strategy, near_target_window))) ||
                     (strict_required_contract &&
                      near_target_window &&
                      (required_uses < strict_required_min_uses)) ||
                     (strict_required_contract &&
                      basic_solved &&
                      (required_uses < (strict_required_min_uses + 1))) ||
                     ((required_uses == 0) &&
                      (basic.steps > max_basic_steps_without_use)) ||
                     (prefer_hit_only_near_target &&
                      near_target_window &&
                      (required_uses > 0)) ||
                     (prefer_hit_only_near_target &&
                      basic_solved &&
                      (required_uses > 0)));

                if (contract_reject) {
                    out_puzzle[static_cast<size_t>(idx)] = old_a;
                    if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;

                    sc.update(idx, reward - (2.0 * required_reward_profile.miss_penalty));
                    sc.disable(idx);
                    if (remove_pair) {
                        sc.update(sym_idx, reward - (2.0 * required_reward_profile.miss_penalty));
                        sc.disable(sym_idx);
                    }

                    ++fail_streak;
                    if (stats != nullptr) {
                        ++stats->advanced_evals;
                        stats->advanced_p7_hits += p7_hits;
                        stats->advanced_p8_hits += p8_hits;
                        stats->required_strategy_analyzed += required_analyzed;
                        stats->required_strategy_uses += required_uses;
                        stats->required_strategy_hits += required_hits;
                        ++stats->rejected_contract;
                    }
                    continue;
                }
                
                if (has_required_slot) {
                    stopping_signal = (required_hits > 0);
                } else if (tuning.require_p8_signal_for_stop) {
                    stopping_signal = (required_hits > 0 || p8_hits > 0 || (!basic_solved && (p7_hits > 0 || required_uses > 0)));
                } else {
                    stopping_signal = (!basic_solved || advanced_signal || required_uses > 0);
                }

                if (stats != nullptr) {
                    ++stats->advanced_evals;
                    stats->advanced_p7_hits += p7_hits;
                    stats->advanced_p8_hits += p8_hits;
                    stats->required_strategy_analyzed += required_analyzed;
                    stats->required_strategy_uses += required_uses;
                    stats->required_strategy_hits += required_hits;
                }
            }

            // Aktualizacja węzła MCTS
            sc.update(idx, reward);
            if (remove_pair) {
                sc.update(sym_idx, reward);
            }
            
            // Zatwierdzenie modyfikacji
            clues -= removal;
            fail_streak = 0;
            
            if (stats != nullptr) {
                stats->accepted_removals += removal;
                if (stopping_signal) {
                    stats->bottleneck_hit = true;
                }
            }

            // Osiągnięcie celu - wcześniejsze wyjście dla optymalizacji czasowej
            if (stopping_signal && clues <= max_clues && clues >= min_clues) {
                break;
            }
        }

        out_clues = clues;
        if (stats != nullptr) {
            stats->final_clues = clues;
            stats->final_fail_streak = fail_streak;
            stats->final_active_count = sc.active_count;
            stats->termination_reason = termination_reason;
        }
        if (trace_required_contract) {
            std::ostringstream oss;
            oss << "required=" << to_string(cfg.required_strategy)
                << " target=" << target_clues
                << " final=" << clues
                << " range=" << min_clues << "-" << max_clues
                << " over_max=" << ((clues > max_clues) ? 1 : 0)
                << " accepted_removals=" << ((stats != nullptr) ? stats->accepted_removals : (topo.nn - clues))
                << " rejected_uniqueness=" << ((stats != nullptr) ? stats->rejected_uniqueness : 0)
                << " rejected_contract=" << ((stats != nullptr) ? stats->rejected_contract : 0)
                << " rejected_logic_timeout=" << ((stats != nullptr) ? stats->rejected_logic_timeout : 0)
                << " skipped_target_floor=" << ((stats != nullptr) ? stats->skipped_target_floor : 0)
                << " skipped_empty=" << ((stats != nullptr) ? stats->skipped_empty_or_disabled : 0)
                << " iterations=" << ((stats != nullptr) ? stats->iterations : 0)
                << " fail_streak=" << fail_streak
                << " fail_cap=" << fail_cap
                << " active_count=" << sc.active_count
                << " protected=" << protected_count
                << " exact=" << (exact_pattern ? 1 : 0)
                << " anchors=" << pattern_anchor_count
                << " reason=" << mcts_termination_reason_label(termination_reason);
            log_info("digger.contract", oss.str());
        }
        return true;
    }
};

} // namespace sudoku_hpc::mcts_digger
