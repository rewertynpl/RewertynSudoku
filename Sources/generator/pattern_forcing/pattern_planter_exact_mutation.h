//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include "pattern_planter_exact_scoring.h"

namespace sudoku_hpc::pattern_forcing {

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

inline bool mutate_msls_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 6) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    uint64_t core = ~0ULL;
    for (int i = 0; i < plan.anchor_count; ++i) {
        core &= plan.anchor_masks[static_cast<size_t>(i)];
    }
    if (std::popcount(core) < 2) {
        core = plan.anchor_masks[0];
        if (std::popcount(core) > 2) {
            uint64_t first = pick_random_bit_from_mask(core, rng);
            uint64_t second = pick_random_bit_from_mask(core & ~first, rng);
            core = first | second;
        }
    }

    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        const int cell = plan.anchor_idx[static_cast<size_t>(idx)];
        uint64_t mutated = core;
        const bool row_box = topo.cell_box[static_cast<size_t>(cell)] ==
                             topo.cell_box[static_cast<size_t>(plan.anchor_idx[0])];
        mutated |= random_extra_digit(full, mutated, rng);
        if (row_box || ((rng() & 1ULL) != 0ULL)) {
            mutated |= random_extra_digit(full, mutated, rng);
        }
        mutated &= full;
        if (std::popcount(mutated) < 3) {
            mutated |= random_extra_digit(full, mutated, rng);
        }
        if (mutated != 0ULL && mutated != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mutated;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_medusa_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)];
        if (std::popcount(mask) > 2 && (rng() & 1ULL) == 0ULL) {
            mask = pick_random_bit_from_mask(mask, rng) |
                   pick_random_bit_from_mask(mask & ~pick_random_bit_from_mask(mask, rng), rng);
        } else {
            mask |= random_extra_digit(full, mask, rng);
        }
        mask &= full;
        if (std::popcount(mask) < 2) mask |= random_extra_digit(full, mask, rng);
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_death_blossom_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)];
        if (idx == 0) {
            mask |= random_extra_digit(full, mask, rng);
        } else if (std::popcount(mask) > 2) {
            mask = pick_random_bit_from_mask(mask, rng) |
                   random_extra_digit(full, pick_random_bit_from_mask(mask, rng), rng);
        } else {
            mask |= random_extra_digit(full, mask, rng);
        }
        mask &= full;
        if (std::popcount(mask) < ((idx == 0) ? 3 : 2)) {
            mask |= random_extra_digit(full, mask, rng);
        }
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_suedecoq_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 5) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)] | random_extra_digit(full, plan.anchor_masks[static_cast<size_t>(idx)], rng);
        if (std::popcount(mask) > 4 && (rng() & 1ULL) == 0ULL) {
            mask = pick_random_bit_from_mask(mask, rng) |
                   pick_random_bit_from_mask(mask & ~pick_random_bit_from_mask(mask, rng), rng);
            mask |= random_extra_digit(full, mask, rng);
        }
        mask &= full;
        if (std::popcount(mask) < 2) mask |= random_extra_digit(full, mask, rng);
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_kraken_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 4) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    uint64_t core = plan.anchor_masks[0] & plan.anchor_masks[1];
    if (std::popcount(core) < 2) core = plan.anchor_masks[0];
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = core;
        if (idx >= 4 || (rng() & 1ULL) == 0ULL) {
            mask |= random_extra_digit(full, mask, rng);
        }
        mask &= full;
        if (std::popcount(mask) < 2) mask |= random_extra_digit(full, mask, rng);
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_als_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    bool aic_mode,
    int strength) {
    if (!plan.valid || plan.anchor_count < 3) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)];
        mask |= random_extra_digit(full, mask, rng);
        if (aic_mode || (rng() & 1ULL) == 0ULL) {
            mask |= random_extra_digit(full, mask, rng);
        }
        mask &= full;
        if (std::popcount(mask) < 2) mask |= random_extra_digit(full, mask, rng);
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_als_xz_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    int strength) {
    if (!plan.valid || plan.anchor_count < 5) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    const uint64_t x = plan.anchor_masks[0] & plan.anchor_masks[2];
    const uint64_t z = plan.anchor_masks[1] & plan.anchor_masks[3];
    const uint64_t shared = (x | z) & full;
    if (shared == 0ULL) return false;

    bool changed = false;
    const int aux_begin = std::min(4, plan.anchor_count);
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int span = std::max(1, plan.anchor_count - aux_begin);
        const int idx = aux_begin + static_cast<int>(rng() % static_cast<uint64_t>(span));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)] | shared;
        const bool prefer_z = ((idx - aux_begin) & 1) != 0;
        mask |= prefer_z ? z : x;
        mask |= random_extra_digit(full, mask, rng);
        if ((rng() & 3ULL) == 0ULL) {
            mask |= random_extra_digit(full, mask, rng);
        }
        mask &= full;
        if (std::popcount(mask) < 2) mask |= shared;
        if (std::popcount(mask) > 4) {
            mask = shared | random_extra_digit(full, shared, rng);
            if ((rng() & 1ULL) != 0ULL) {
                mask |= random_extra_digit(full, mask, rng);
            }
        }
        mask &= full;
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
            plan.add_skeleton(plan.anchor_idx[static_cast<size_t>(idx)], mask);
            changed = true;
        }
    }
    return changed;
}

inline bool mutate_exclusion_template_plan(
    const GenericTopology& topo,
    ExactPatternTemplatePlan& plan,
    std::mt19937_64& rng,
    bool triple_mode,
    int strength) {
    if (!plan.valid || plan.anchor_count < (triple_mode ? 3 : 2)) return false;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    bool changed = false;
    for (int rep = 0; rep < std::max(1, strength); ++rep) {
        const int idx = static_cast<int>(rng() % static_cast<uint64_t>(plan.anchor_count));
        uint64_t mask = plan.anchor_masks[static_cast<size_t>(idx)] | random_extra_digit(full, plan.anchor_masks[static_cast<size_t>(idx)], rng);
        mask &= full;
        if (std::popcount(mask) < 2) mask |= random_extra_digit(full, mask, rng);
        if (mask != 0ULL && mask != plan.anchor_masks[static_cast<size_t>(idx)]) {
            plan.anchor_masks[static_cast<size_t>(idx)] = mask;
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
    case RequiredStrategy::MSLS:
        return mutate_msls_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::Medusa3D:
        return mutate_medusa_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::DeathBlossom:
        return mutate_death_blossom_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::SueDeCoq:
        return mutate_suedecoq_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::KrakenFish:
        return mutate_kraken_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::FrankenFish:
        return mutate_kraken_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::MutantFish:
        return mutate_kraken_template_plan(topo, plan, rng, strength + 1);
    case RequiredStrategy::Squirmbag:
        return mutate_kraken_template_plan(topo, plan, rng, strength + 2);
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::XWing:
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
        return mutate_kraken_template_plan(topo, plan, rng, strength + 1);
    case RequiredStrategy::SimpleColoring:
        return mutate_medusa_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::WXYZWing:
        return mutate_als_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::ALSXZ:
        return mutate_als_xz_template_plan(topo, plan, rng, strength + 1);
    case RequiredStrategy::ALSChain:
        return mutate_als_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::ALSAIC:
        return mutate_als_template_plan(topo, plan, rng, true, strength);
    case RequiredStrategy::AlignedPairExclusion:
        return mutate_exclusion_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::AlignedTripleExclusion:
        return mutate_exclusion_template_plan(topo, plan, rng, true, strength);
    case RequiredStrategy::AIC:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::GroupedAIC:
        return mutate_forcing_template_plan(topo, plan, rng, true, strength);
    case RequiredStrategy::GroupedXCycle:
        return mutate_skloop_template_plan(topo, plan, rng, strength);
    case RequiredStrategy::ContinuousNiceLoop:
        return mutate_skloop_template_plan(topo, plan, rng, strength + 1);
    case RequiredStrategy::XChain:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength + 2);
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength + 1);
    case RequiredStrategy::EmptyRectangle:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength + 3);
    case RequiredStrategy::XYChain:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength + 1);
    case RequiredStrategy::RemotePairs:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength + 3);
    case RequiredStrategy::ForcingChains:
        return mutate_forcing_template_plan(topo, plan, rng, false, strength);
    case RequiredStrategy::DynamicForcingChains:
        return mutate_forcing_template_plan(topo, plan, rng, true, strength);
    default:
        return false;
    }
}

inline int adaptive_mutation_strength(
    RequiredStrategy required_strategy,
    int zero_use_streak,
    int failure_streak,
    PatternMutationSource source) {
    const int zero_term = std::max(0, zero_use_streak);
    const int fail_term = std::max(0, failure_streak);
    int strength = 1;
    switch (required_strategy) {
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        strength += zero_term / 2 + fail_term / 3;
        break;
    case RequiredStrategy::SKLoop:
        strength += zero_term / 2 + fail_term / 2;
        break;
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::Squirmbag:
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::XWing:
    case RequiredStrategy::Jellyfish:
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
    case RequiredStrategy::DeathBlossom:
        strength += zero_term / 2 + fail_term / 2;
        break;
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::SueDeCoq:
        strength += zero_term / 3 + fail_term / 2;
        break;
    case RequiredStrategy::ALSXYWing:
        strength += zero_term / 2 + fail_term / 2;
        break;
    case RequiredStrategy::ALSXZ:
        strength += zero_term + fail_term / 2;
        break;
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
        strength += zero_term / 2 + fail_term / 2;
        break;
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
    case RequiredStrategy::XYChain:
        strength += zero_term / 2 + fail_term;
        break;
    case RequiredStrategy::XChain:
    case RequiredStrategy::EmptyRectangle:
    case RequiredStrategy::RemotePairs:
        strength += zero_term + fail_term;
        break;
    case RequiredStrategy::SimpleColoring:
        strength += zero_term / 3 + fail_term / 2;
        break;
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        strength += zero_term + fail_term / 2;
        break;
    default:
        strength += zero_term / 2 + fail_term / 2;
        break;
    }
    if (source == PatternMutationSource::Best) {
        strength += std::max(1, zero_term / 3);
    }
    return std::clamp(strength, 1, 16);
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

} // namespace sudoku_hpc::pattern_forcing
