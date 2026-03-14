//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include "pattern_planter_anchor_builders.h"

namespace sudoku_hpc::pattern_forcing {

// GĹ‚Ăłwna funkcja orkiestratora
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

    const PatternStrategyPolicy policy = pattern_strategy_policy(required_strategy, forcing_level);
    PatternKind kind = policy.kind;
    if (kind == PatternKind::None) return false;
    const bool strict_exact_family = pattern_policy_requires_exact(policy.generator_policy);
    const bool allows_family_fallback = pattern_policy_allows_family_fallback(policy.generator_policy);
    const bool wants_exact_templates = pattern_policy_supports_exact(policy.generator_policy);

    PatternScratch& sc = tls_pattern_scratch();
    sc.reset(topo);
    PatternMutationState& mutation = tls_pattern_mutation_state();
    if (mutation.strategy != required_strategy || mutation.kind != kind) {
        mutation.reset(required_strategy, kind);
    }
    const bool force_family_fallback =
        allows_family_fallback &&
        (required_strategy == RequiredStrategy::ForcingChains ||
         required_strategy == RequiredStrategy::DynamicForcingChains) &&
        mutation.failure_streak >= 3;

    ExactPatternTemplatePlan exact_plan{};
    PatternKind exact_kind = PatternKind::None;
    int exact_score = -1;
    PatternMutationSource selected_source = PatternMutationSource::Random;
    int score_delta = 0;
    int selected_strength = 0;
    int adaptive_target_strength = 0;
    const bool exact_matched = (!force_family_fallback && wants_exact_templates)
        ? try_exact_templates_for_level(topo, required_strategy, forcing_level, rng, exact_plan, exact_kind, &exact_score)
        : false;

    if (!force_family_fallback && wants_exact_templates &&
        mutation.have_last && mutation.strategy == required_strategy && mutation.kind == kind &&
        mutation.last_plan.valid && (mutation.failure_streak > 0 || mutation.zero_use_streak > 0)) {
        ExactPatternTemplatePlan mutated = mutation.last_plan;
        const int strength = adaptive_mutation_strength(
            required_strategy, mutation.zero_use_streak, mutation.failure_streak, PatternMutationSource::Last);
        adaptive_target_strength = std::max(adaptive_target_strength, strength);
        if (mutate_exact_template_for_family(topo, required_strategy, mutated, rng, strength)) {
            const int mutated_score = score_exact_plan(topo, required_strategy, kind, mutated);
            if (mutated_score >= std::max(exact_score - 6, 0)) {
                exact_plan = mutated;
                exact_kind = kind;
                exact_score = mutated_score;
                selected_source = PatternMutationSource::Last;
                score_delta = mutated_score - std::max(0, mutation.last_score);
                selected_strength = strength;
            }
        }
    }
    if (!force_family_fallback && wants_exact_templates &&
        mutation.have_best && mutation.strategy == required_strategy && mutation.kind == kind &&
        mutation.best_plan.valid && mutation.zero_use_streak >= 2) {
        ExactPatternTemplatePlan mutated = mutation.best_plan;
        const int strength = adaptive_mutation_strength(
            required_strategy, mutation.zero_use_streak, mutation.failure_streak, PatternMutationSource::Best);
        adaptive_target_strength = std::max(adaptive_target_strength, strength);
        if (mutate_exact_template_for_family(topo, required_strategy, mutated, rng, strength)) {
            const int mutated_score = score_exact_plan(topo, required_strategy, kind, mutated);
            if (mutated_score > exact_score) {
                exact_plan = mutated;
                exact_kind = kind;
                exact_score = mutated_score;
                selected_source = PatternMutationSource::Best;
                score_delta = mutated_score - std::max(0, mutation.best_score);
                selected_strength = strength;
            }
        }
    }

    // Aplikacja masek z Exact Planu (je li si  uda o)
    if ((exact_matched || exact_score >= 0) && exact_plan.valid && exact_plan.anchor_count > 0) {
        sc.exact_template = true;
        sc.template_score = exact_score;
        expand_exact_template_skeleton(
            topo, (exact_kind == PatternKind::None) ? kind : exact_kind, exact_plan);
        for (int i = 0; i < exact_plan.anchor_count; ++i) {
            const int idx = exact_plan.anchor_idx[static_cast<size_t>(i)];
            if (!sc.add_anchor(idx)) continue;

            uint64_t mask = exact_plan.anchor_masks[static_cast<size_t>(i)];
            if (mask == 0ULL) mask = pf_full_mask_for_n(topo.n);
            
            sc.allowed_masks[static_cast<size_t>(idx)] = mask;
            sc.anchor_masks[static_cast<size_t>(i)] = mask;
        }

        for (int i = 0; i < exact_plan.skeleton_count; ++i) {
            const int idx = exact_plan.skeleton_idx[static_cast<size_t>(i)];
            if (idx < 0 || idx >= topo.nn) continue;
            uint64_t mask = exact_plan.skeleton_masks[static_cast<size_t>(i)];
            if (mask == 0ULL) continue;

            uint64_t& slot = sc.allowed_masks[static_cast<size_t>(idx)];
            if (slot == 0ULL) slot = mask;
            else slot &= mask;
            if (slot == 0ULL) slot = mask;
        }

        // Chronione kom rki exact skeleton, bez szumu spoza wzorca
        if (cfg.pattern_forcing_lock_anchors) {
            protect_exact_template_skeleton(sc, exact_plan, required_strategy);
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
            out.generator_policy = policy.generator_policy;
            out.seed_puzzle = &sc.seed_puzzle;
            out.allowed_masks = &sc.allowed_masks;
            out.protected_cells = &sc.protected_cells;
            out.anchor_idx = sc.anchors.data();
            out.anchor_masks = sc.anchor_masks.data();
            out.anchor_count = sc.anchor_count;
            out.exact_template = sc.exact_template;
            out.family_fallback_used = false;
            out.required_strategy_exact_contract_met =
                !pattern_policy_requires_exact(policy.generator_policy) || sc.exact_template;
            out.template_score = sc.template_score;
            out.best_template_score = mutation.best_score;
            out.template_score_delta = score_delta;
            out.mutation_strength = selected_strength;
            out.planner_zero_use_streak = mutation.zero_use_streak;
            out.planner_failure_streak = mutation.failure_streak;
            out.adaptive_target_strength = adaptive_target_strength;
            out.mutation_source = selected_source;
            return true;
        }
    }

    if (strict_exact_family) {
        return false;
    }
    if (!allows_family_fallback) {
        return false;
    }

    // Fallback: luĹşne kotwice (jeĹ›li Exact Plan zawiĂłdĹ‚ z powodu asymetrii lub P1-P6)
    bool ok = false;
    switch (kind) {
        case PatternKind::ExocetLike:
            ok = build_exocet_like_anchors(topo, sc, rng);
            break;
        case PatternKind::LoopLike:
            ok = build_loop_like_anchors(topo, sc, rng);
            break;
        case PatternKind::ColorLike:
            ok = build_color_like_anchors(topo, sc, rng);
            break;
        case PatternKind::PetalLike:
            ok = build_petal_like_anchors(topo, sc, rng);
            break;
        case PatternKind::IntersectionLike:
            ok = build_intersection_like_anchors(topo, sc, rng);
            break;
        case PatternKind::FishLike:
            ok = build_fish_like_anchors(topo, sc, rng);
            break;
        case PatternKind::FrankenLike:
            ok = build_franken_like_anchors(topo, sc, rng);
            break;
        case PatternKind::MutantLike:
            ok = build_mutant_like_anchors(topo, sc, rng);
            break;
        case PatternKind::SquirmLike:
            ok = build_squirm_like_anchors(topo, sc, rng);
            break;
        case PatternKind::SwordfishLike:
        case PatternKind::JellyfishLike:
        case PatternKind::FinnedFishLike:
            ok = build_fish_like_anchors(topo, sc, rng);
            break;
        case PatternKind::AlsLike:
            ok = build_als_like_anchors(topo, sc, rng);
            break;
        case PatternKind::ExclusionLike:
            ok = build_exclusion_like_anchors(topo, sc, rng);
            break;
        case PatternKind::AicLike:
            ok = build_aic_like_anchors(topo, sc, rng);
            break;
        case PatternKind::GroupedAicLike:
            ok = build_grouped_aic_like_anchors(topo, sc, rng);
            break;
        case PatternKind::GroupedCycleLike:
            ok = build_grouped_cycle_like_anchors(topo, sc, rng);
            break;
        case PatternKind::NiceLoopLike:
            ok = build_niceloop_like_anchors(topo, sc, rng);
            break;
        case PatternKind::XChainLike:
            ok = build_aic_like_anchors(topo, sc, rng);
            break;
        case PatternKind::XYChainLike:
            ok = build_niceloop_like_anchors(topo, sc, rng);
            break;
        case PatternKind::EmptyRectangleLike:
            ok = build_empty_rectangle_like_anchors(topo, sc, rng);
            break;
        case PatternKind::RemotePairsLike:
            ok = build_remote_pairs_like_anchors(topo, sc, rng);
            break;
        case PatternKind::ForcingLike:
            ok = build_forcing_like_anchors(topo, sc, rng);
            break;
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

    // DopeĹ‚nianie luĹşnymi cyframi
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
        protect_pattern_cells_from_masks(topo, sc);
    }

    out.kind = kind;
    out.generator_policy = policy.generator_policy;
    out.seed_puzzle = &sc.seed_puzzle;
    out.allowed_masks = &sc.allowed_masks;
    out.protected_cells = &sc.protected_cells;
    out.anchor_idx = sc.anchors.data();
    out.anchor_masks = sc.anchor_masks.data();
    out.anchor_count = sc.anchor_count;
    out.exact_template = false;
    out.family_fallback_used = true;
    out.required_strategy_exact_contract_met = !pattern_policy_requires_exact(policy.generator_policy);
    out.template_score = 0;
    out.best_template_score = mutation.best_score;
    out.template_score_delta = 0;
    out.mutation_strength = 0;
    out.planner_zero_use_streak = mutation.zero_use_streak;
    out.planner_failure_streak = mutation.failure_streak;
    out.adaptive_target_strength = adaptive_target_strength;
    out.mutation_source = PatternMutationSource::Random;
    return true;
}

} // namespace sudoku_hpc::pattern_forcing
