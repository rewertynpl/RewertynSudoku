//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include "pattern_planter_exact_mutation.h"

namespace sudoku_hpc::pattern_forcing {

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
    int best_family_score = -1;
    PatternKind best_any_kind = PatternKind::None;
    PatternKind best_family_kind = PatternKind::None;
    ExactPatternTemplatePlan best_any_plan{};
    ExactPatternTemplatePlan best_family_plan{};
    const PatternStrategyPolicy preferred_policy = pattern_strategy_policy(required_strategy, forcing_level);
    const PatternKind preferred_kind = preferred_policy.kind;

    auto consider = [&](PatternKind candidate_kind, const ExactPatternTemplatePlan& candidate_plan) {
        const int score = score_exact_plan(topo, required_strategy, candidate_kind, candidate_plan);
        if (score > best_score) {
            best_score = score;
            best_any_plan = candidate_plan;
            best_any_kind = candidate_kind;
        }
        if (candidate_kind == preferred_kind && score > best_family_score) {
            best_family_score = score;
            best_family_plan = candidate_plan;
            best_family_kind = candidate_kind;
        }
    };

    auto repeat_attempts = [&](int tries, auto&& builder) {
        for (int attempt = 0; attempt < tries; ++attempt) {
            builder();
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
    auto try_medusa = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_medusa(topo, rng, candidate)) {
            consider(PatternKind::ColorLike, candidate);
        }
    };
    auto try_death_blossom = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_death_blossom(topo, rng, candidate)) {
            consider(PatternKind::PetalLike, candidate);
        }
    };
    auto try_suedecoq = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_sue_de_coq(topo, rng, candidate)) {
            consider(PatternKind::IntersectionLike, candidate);
        }
    };
    auto try_kraken = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_kraken_fish(topo, rng, candidate)) {
            consider(PatternKind::FishLike, candidate);
        }
    };
    auto try_franken = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_franken_fish(topo, rng, candidate)) {
            consider(PatternKind::FrankenLike, candidate);
        }
    };
    auto try_mutant = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_mutant_fish(topo, rng, candidate)) {
            consider(PatternKind::MutantLike, candidate);
        }
    };
    auto try_squirm = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_squirmbag(topo, rng, candidate)) {
            consider(PatternKind::SquirmLike, candidate);
        }
    };
    auto try_als_xy = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_als_xy_wing(topo, rng, candidate)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_als_xz = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_als_xz(topo, rng, candidate)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_wxyz = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_wxyz_wing(topo, rng, candidate)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_als_chain = [&](bool aic_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_als_chain(topo, rng, candidate, aic_mode)) {
            consider(PatternKind::AlsLike, candidate);
        }
    };
    auto try_exclusion = [&](bool triple_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_aligned_exclusion(topo, rng, candidate, triple_mode)) {
            consider(PatternKind::ExclusionLike, candidate);
        }
    };
    auto try_aic = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_aic(topo, rng, candidate)) {
            consider(PatternKind::AicLike, candidate);
        }
    };
    auto try_grouped_aic = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_grouped_aic(topo, rng, candidate)) {
            consider(PatternKind::GroupedAicLike, candidate);
        }
    };
    auto try_grouped_xcycle = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_grouped_x_cycle(topo, rng, candidate)) {
            consider(PatternKind::GroupedCycleLike, candidate);
        }
    };
    auto try_continuous_nice_loop = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_continuous_nice_loop(topo, rng, candidate)) {
            consider(PatternKind::NiceLoopLike, candidate);
        }
    };
    auto try_xchain = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_x_chain(topo, rng, candidate)) {
            consider(PatternKind::XChainLike, candidate);
        }
    };
    auto try_xychain = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_xy_chain(topo, rng, candidate)) {
            consider(PatternKind::XYChainLike, candidate);
        }
    };
    auto try_empty_rectangle = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_empty_rectangle(topo, rng, candidate)) {
            consider(PatternKind::EmptyRectangleLike, candidate);
        }
    };
    auto try_remote_pairs = [&]() {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_remote_pairs(topo, rng, candidate)) {
            consider(PatternKind::RemotePairsLike, candidate);
        }
    };
    auto try_large_fish = [&](PatternKind kind, int line_count, bool finned_mode) {
        ExactPatternTemplatePlan candidate{};
        if (TemplateP6Exact::build_large_fish(topo, rng, candidate, line_count, finned_mode)) {
            consider(kind, candidate);
        }
    };

    const bool strict_exact_family =
        pattern_policy_requires_exact(pattern_strategy_policy(required_strategy, forcing_level).generator_policy);

    switch (required_strategy) {
        case RequiredStrategy::Exocet:
            repeat_attempts(12, [&]() { try_exocet(false); });
            break;
        case RequiredStrategy::SeniorExocet:
            repeat_attempts(12, [&]() { try_exocet(true); });
            break;
        case RequiredStrategy::SKLoop:
            repeat_attempts(8, [&]() { try_sk(false); });
            repeat_attempts(6, [&]() { try_sk(true); });
            repeat_attempts(6, [&]() { try_overlay(); });
            break;
        case RequiredStrategy::MSLS:
            repeat_attempts(12, [&]() { try_msls(); });
            break;
        case RequiredStrategy::PatternOverlayMethod:
            repeat_attempts(8, [&]() { try_overlay(); });
            break;
        case RequiredStrategy::Medusa3D:
            try_medusa();
            break;
        case RequiredStrategy::DeathBlossom:
            try_death_blossom();
            break;
        case RequiredStrategy::SueDeCoq:
            try_suedecoq();
            break;
        case RequiredStrategy::KrakenFish:
            try_kraken();
            break;
        case RequiredStrategy::FrankenFish:
            try_franken();
            break;
        case RequiredStrategy::MutantFish:
            try_mutant();
            break;
        case RequiredStrategy::Squirmbag:
            try_squirm();
            break;
        case RequiredStrategy::ALSXYWing:
            try_als_xy();
            break;
        case RequiredStrategy::ALSXZ:
            try_als_xz();
            break;
        case RequiredStrategy::WXYZWing:
            try_wxyz();
            break;
        case RequiredStrategy::ALSChain:
            try_als_chain(false);
            break;
        case RequiredStrategy::ALSAIC:
            try_als_chain(true);
            break;
        case RequiredStrategy::AlignedPairExclusion:
            try_exclusion(false);
            break;
        case RequiredStrategy::AlignedTripleExclusion:
            try_exclusion(true);
            break;
        case RequiredStrategy::ForcingChains:
            repeat_attempts(8, [&]() { try_forcing(false); });
            break;
        case RequiredStrategy::DynamicForcingChains:
            repeat_attempts(8, [&]() { try_forcing(true); });
            break;
        case RequiredStrategy::AIC:
            try_aic();
            break;
        case RequiredStrategy::GroupedAIC:
            try_grouped_aic();
            break;
        case RequiredStrategy::GroupedXCycle:
            try_grouped_xcycle();
            break;
        case RequiredStrategy::ContinuousNiceLoop:
            try_continuous_nice_loop();
            break;
        case RequiredStrategy::XChain:
            try_xchain();
            break;
        case RequiredStrategy::XYChain:
            try_xychain();
            break;
        case RequiredStrategy::Swordfish:
            try_large_fish(PatternKind::SwordfishLike, 3, false);
            break;
        case RequiredStrategy::Jellyfish:
            try_large_fish(PatternKind::JellyfishLike, 4, false);
            break;
        case RequiredStrategy::XWing:
            try_large_fish(PatternKind::SwordfishLike, 2, false);
            break;
        case RequiredStrategy::FinnedXWingSashimi:
            try_large_fish(PatternKind::FinnedFishLike, 2, true);
            break;
        case RequiredStrategy::FinnedSwordfishJellyfish:
            try_large_fish(PatternKind::FinnedFishLike, 3, true);
            break;
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
            try_xchain();
            break;
        case RequiredStrategy::EmptyRectangle:
            try_empty_rectangle();
            break;
        case RequiredStrategy::RemotePairs:
            try_remote_pairs();
            break;
        case RequiredStrategy::SimpleColoring:
            try_medusa();
            break;
        default:
            break;
    }

    if (!strict_exact_family && forcing_level >= 8) {
        try_exocet(false);
        try_exocet(true);
        try_msls();
        try_overlay();
        try_sk(true);
        try_kraken();
        try_franken();
        try_mutant();
        try_squirm();
        try_death_blossom();
        try_medusa();
        try_suedecoq();
        try_als_xy();
        try_als_xz();
        try_wxyz();
        try_als_chain(true);
        try_exclusion(true);
        try_grouped_xcycle();
        try_continuous_nice_loop();
        try_grouped_aic();
        try_aic();
        try_xchain();
        try_xychain();
        try_empty_rectangle();
        try_remote_pairs();
        try_large_fish(PatternKind::SwordfishLike, 3, false);
        try_large_fish(PatternKind::SwordfishLike, 2, false);
        try_large_fish(PatternKind::JellyfishLike, 4, false);
        try_large_fish(PatternKind::FinnedFishLike, 3, true);
        try_forcing(true);
    } else if (!strict_exact_family && forcing_level >= 7) {
        try_msls();
        try_overlay();
        try_sk(true);
        try_death_blossom();
        try_medusa();
        try_suedecoq();
        try_franken();
        try_mutant();
        try_squirm();
        try_als_xy();
        try_als_xz();
        try_wxyz();
        try_als_chain(false);
        try_exclusion(false);
        try_grouped_xcycle();
        try_continuous_nice_loop();
        try_grouped_aic();
        try_aic();
        try_xchain();
        try_xychain();
        try_empty_rectangle();
        try_remote_pairs();
        try_large_fish(PatternKind::SwordfishLike, 3, false);
        try_large_fish(PatternKind::SwordfishLike, 2, false);
        try_large_fish(PatternKind::FinnedFishLike, 2, true);
        try_medusa();
        try_forcing(false);
        try_exocet(false);
    }

    if (best_family_score >= 0) {
        exact_plan = best_family_plan;
        out_kind = best_family_kind;
        best_score = best_family_score;
    } else {
        exact_plan = best_any_plan;
        out_kind = best_any_kind;
    }

    if (out_score != nullptr) *out_score = best_score;
    return best_score >= 0 && exact_plan.valid;
}

} // namespace sudoku_hpc::pattern_forcing
