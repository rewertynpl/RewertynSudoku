//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include "pattern_planter_types.h"

namespace sudoku_hpc::pattern_forcing {

struct PatternStrategyPolicy {
    PatternKind kind = PatternKind::None;
    PatternGeneratorPolicy generator_policy = PatternGeneratorPolicy::Unsupported;
};

inline bool pattern_policy_supports_exact(PatternGeneratorPolicy policy) {
    return policy == PatternGeneratorPolicy::ExactRequired ||
           policy == PatternGeneratorPolicy::ExactPreferredFamilyFallback;
}

inline bool pattern_policy_requires_exact(PatternGeneratorPolicy policy) {
    return policy == PatternGeneratorPolicy::ExactRequired;
}

inline bool pattern_policy_allows_family_fallback(PatternGeneratorPolicy policy) {
    return policy == PatternGeneratorPolicy::ExactPreferredFamilyFallback ||
           policy == PatternGeneratorPolicy::FamilyOnly;
}

inline PatternStrategyPolicy pattern_strategy_policy(RequiredStrategy required, int level) {
    switch (required) {
    case RequiredStrategy::Exocet:
        return {PatternKind::ExocetLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::SeniorExocet:
        return {PatternKind::ExocetLike, PatternGeneratorPolicy::ExactRequired};
    case RequiredStrategy::MSLS:
        return {PatternKind::LoopLike, PatternGeneratorPolicy::ExactRequired};
    case RequiredStrategy::PatternOverlayMethod:
        return {PatternKind::LoopLike, PatternGeneratorPolicy::ExactRequired};
    case RequiredStrategy::SKLoop:
        return {PatternKind::LoopLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::Medusa3D:
        return {PatternKind::ColorLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::DeathBlossom:
        return {PatternKind::PetalLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::SueDeCoq:
        return {PatternKind::IntersectionLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::KrakenFish:
        return {PatternKind::FishLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::FrankenFish:
        return {PatternKind::FrankenLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::MutantFish:
        return {PatternKind::MutantLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::Squirmbag:
        return {PatternKind::SquirmLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
        return {PatternKind::AlsLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
        return {PatternKind::ExclusionLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::AIC:
        return {PatternKind::AicLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::GroupedAIC:
        return {PatternKind::GroupedAicLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::GroupedXCycle:
        return {PatternKind::GroupedCycleLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::ContinuousNiceLoop:
        return {PatternKind::NiceLoopLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::XChain:
        return {PatternKind::XChainLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::XYChain:
        return {PatternKind::XYChainLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return {PatternKind::ForcingLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::Swordfish:
        return {PatternKind::SwordfishLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::XWing:
        return {PatternKind::SwordfishLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::Jellyfish:
        return {PatternKind::JellyfishLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
        return {PatternKind::FinnedFishLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
        return {PatternKind::XChainLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::EmptyRectangle:
        return {PatternKind::EmptyRectangleLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::RemotePairs:
        return {PatternKind::RemotePairsLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::SimpleColoring:
        return {PatternKind::ColorLike, PatternGeneratorPolicy::ExactPreferredFamilyFallback};
    case RequiredStrategy::None:
        break;
    default:
        return {};
    }
    if (level >= 8) return {PatternKind::ExocetLike, PatternGeneratorPolicy::FamilyOnly};
    if (level >= 7) return {PatternKind::LoopLike, PatternGeneratorPolicy::FamilyOnly};
    if (level >= 6) return {PatternKind::Chain, PatternGeneratorPolicy::FamilyOnly};
    return {};
}

inline PatternKind pick_kind(RequiredStrategy required, int level) {
    return pattern_strategy_policy(required, level).kind;
}

inline bool pattern_dispatch_wired(RequiredStrategy required, int level) {
    return pick_kind(required, level) != PatternKind::None;
}

inline bool pattern_exact_template_dispatch_wired(RequiredStrategy required, int /*level*/) {
    switch (required) {
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::MSLS:
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::XChain:
    case RequiredStrategy::XYChain:
    case RequiredStrategy::XWing:
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
    case RequiredStrategy::SimpleColoring:
        return true;
    default:
        return false;
    }
}

inline bool pattern_family_fallback_dispatch_wired(RequiredStrategy required, int level) {
    const PatternStrategyPolicy policy = pattern_strategy_policy(required, level);
    return pattern_dispatch_wired(required, level) &&
           pattern_policy_allows_family_fallback(policy.generator_policy);
}

inline const char* pattern_kind_label(PatternKind kind) {
    switch (kind) {
    case PatternKind::Chain: return "chain";
    case PatternKind::ExocetLike: return "exocet";
    case PatternKind::LoopLike: return "loop";
    case PatternKind::ForcingLike: return "forcing";
    case PatternKind::ColorLike: return "color";
    case PatternKind::PetalLike: return "petal";
    case PatternKind::IntersectionLike: return "intersection";
    case PatternKind::FishLike: return "fish";
    case PatternKind::FrankenLike: return "franken";
    case PatternKind::MutantLike: return "mutant";
    case PatternKind::SquirmLike: return "squirm";
    case PatternKind::AlsLike: return "als";
    case PatternKind::ExclusionLike: return "exclusion";
    case PatternKind::AicLike: return "aic";
    case PatternKind::GroupedAicLike: return "groupedaic";
    case PatternKind::GroupedCycleLike: return "groupedxcycle";
    case PatternKind::NiceLoopLike: return "continuousniceloop";
    case PatternKind::XChainLike: return "xchain";
    case PatternKind::XYChainLike: return "xychain";
    case PatternKind::EmptyRectangleLike: return "emptyrectangle";
    case PatternKind::RemotePairsLike: return "remotepairs";
    case PatternKind::SwordfishLike: return "swordfish";
    case PatternKind::JellyfishLike: return "jellyfish";
    case PatternKind::FinnedFishLike: return "finnedfish";
    case PatternKind::None:
    default:
        return "none";
    }
}

inline const char* pattern_mutation_source_label(PatternMutationSource source) {
    switch (source) {
    case PatternMutationSource::Last: return "last";
    case PatternMutationSource::Best: return "best";
    case PatternMutationSource::Random:
    default:
        return "random";
    }
}


} // namespace sudoku_hpc::pattern_forcing
