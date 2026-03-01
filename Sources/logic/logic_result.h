// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// File: logic_result.h
// Description: Result and telemetry structures for the logical solver.
// ============================================================================

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace sudoku_hpc::logic {

enum class ApplyResult : uint8_t {
    NoProgress = 0,
    Progress = 1,
    Contradiction = 2
};

enum class StrategyImplTier : uint8_t {
    Full = 0,
    Hybrid = 1,
    Proxy = 2,
    Disabled = 3,
};

inline constexpr size_t impl_tier_index(StrategyImplTier tier) {
    return static_cast<size_t>(tier);
}

inline constexpr size_t kStrategySlotCount = 61;
inline constexpr size_t kMaxStepTrace = 4096;

struct StrategyStats {
    uint64_t use_count = 0;
    uint64_t hit_count = 0;
    uint64_t placements = 0;
    uint64_t elapsed_ns = 0;
};

struct StepTrace {
    uint16_t slot = 0;
    StrategyImplTier impl_tier = StrategyImplTier::Full;
    ApplyResult result = ApplyResult::NoProgress;
    uint16_t placements_delta = 0;
    uint16_t hit_delta = 0;
    uint32_t step_index = 0;
    std::string_view proof_tag{};
};

struct GenericLogicCertifyResult {
    bool solved = false;
    bool timed_out = false;

    // Per-strategy usage flags.
    bool used_naked_single = false;
    bool used_hidden_single = false;
    bool used_pointing_pairs = false;
    bool used_box_line = false;
    bool used_naked_pair = false;
    bool used_hidden_pair = false;
    bool used_naked_triple = false;
    bool used_hidden_triple = false;
    bool used_naked_quad = false;
    bool used_hidden_quad = false;
    bool used_x_wing = false;
    bool used_y_wing = false;
    bool used_skyscraper = false;
    bool used_two_string_kite = false;
    bool used_empty_rectangle = false;
    bool used_remote_pairs = false;
    bool used_swordfish = false;
    bool used_finned_x_wing_sashimi = false;
    bool used_simple_coloring = false;
    bool used_bug_plus_one = false;
    bool used_unique_rectangle = false;
    bool used_xyz_wing = false;
    bool used_w_wing = false;
    bool used_jellyfish = false;
    bool used_x_chain = false;
    bool used_xy_chain = false;
    bool used_wxyz_wing = false;
    bool used_finned_swordfish_jellyfish = false;
    bool used_als_xz = false;
    bool used_unique_loop = false;
    bool used_avoidable_rectangle = false;
    bool used_bivalue_oddagon = false;
    bool used_medusa_3d = false;
    bool used_aic = false;
    bool used_grouped_aic = false;
    bool used_grouped_x_cycle = false;
    bool used_continuous_nice_loop = false;
    bool used_als_xy_wing = false;
    bool used_als_chain = false;
    bool used_sue_de_coq = false;
    bool used_death_blossom = false;
    bool used_franken_fish = false;
    bool used_mutant_fish = false;
    bool used_kraken_fish = false;
    bool used_msls = false;
    bool used_exocet = false;
    bool used_senior_exocet = false;
    bool used_sk_loop = false;
    bool used_pattern_overlay_method = false;
    bool used_forcing_chains = false;
    bool used_squirmbag = false;
    bool used_ur_extended = false;
    bool used_hidden_ur = false;
    bool used_bug_type2 = false;
    bool used_bug_type3 = false;
    bool used_bug_type4 = false;
    bool used_borescoper_qiu_deadly_pattern = false;
    bool used_aligned_pair_exclusion = false;
    bool used_aligned_triple_exclusion = false;
    bool used_als_aic = false;
    bool used_dynamic_forcing_chains = false;

    bool naked_single_scanned = false;
    bool hidden_single_scanned = false;

    int steps = 0;
    std::vector<uint16_t> solved_grid; // kept for replay validation

    // Fixed-size strategy stats matching GenericLogicCertify::StrategySlot.
    std::array<StrategyStats, kStrategySlotCount> strategy_stats{};

    // Telemetry: number of successful progress events grouped by impl tier.
    std::array<uint64_t, 4> impl_tier_hits{};

    // Telemetry: bounded step trace (no dynamic allocation in hot path).
    std::array<StepTrace, kMaxStepTrace> step_trace{};
    uint32_t step_trace_count = 0;
    uint32_t step_trace_dropped = 0;

    inline void record_step(
        uint16_t slot,
        StrategyImplTier tier,
        ApplyResult result_kind,
        uint16_t placements_delta,
        uint16_t hit_delta,
        std::string_view proof_tag) {
        if (result_kind == ApplyResult::Progress) {
            ++impl_tier_hits[impl_tier_index(tier)];
        }
        if (step_trace_count >= step_trace.size()) {
            ++step_trace_dropped;
            return;
        }
        StepTrace& row = step_trace[step_trace_count++];
        row.slot = slot;
        row.impl_tier = tier;
        row.result = result_kind;
        row.placements_delta = placements_delta;
        row.hit_delta = hit_delta;
        row.step_index = static_cast<uint32_t>(steps);
        row.proof_tag = proof_tag;
    }
};

} // namespace sudoku_hpc::logic
