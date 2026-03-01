#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "../config/run_config.h"

namespace sudoku_hpc {

struct ParseArgsResult {
    GenerateRunConfig cfg;

    bool list_geometries = false;
    bool validate_geometry = false;
    bool validate_geometry_catalog = false;
    bool run_regression_tests = false;

    bool run_geometry_gate = false;
    std::string geometry_gate_report = "geometry_gate_report.txt";

    bool run_quality_benchmark = false;
    std::string quality_benchmark_report = "quality_benchmark_report.txt";
    uint64_t quality_benchmark_max_cases = 0;

    bool run_pre_difficulty_gate = false;
    std::string pre_difficulty_gate_report = "pre_difficulty_gate_report.txt";

    bool run_asym_pair_benchmark = false;
    std::string asym_pair_benchmark_report = "asym_pair_benchmark_report.txt";

    bool run_vip_benchmark = false;
    std::string vip_benchmark_report = "vip_benchmark_report.txt";

    bool run_vip_gate = false;
    std::string vip_gate_report = "vip_gate_report.txt";

    bool explain_profile = false;
    bool benchmark_mode = false;
};

inline bool parse_i64(const char* s, long long& out) {
    if (s == nullptr) return false;
    char* end = nullptr;
    const long long v = std::strtoll(s, &end, 10);
    if (end == s || (end != nullptr && *end != '\0')) return false;
    out = v;
    return true;
}

inline bool parse_u64(const char* s, uint64_t& out) {
    if (s == nullptr) return false;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(s, &end, 10);
    if (end == s || (end != nullptr && *end != '\0')) return false;
    out = static_cast<uint64_t>(v);
    return true;
}

inline bool parse_i32(const char* s, int& out) {
    long long v = 0;
    if (!parse_i64(s, v)) return false;
    out = static_cast<int>(v);
    return true;
}

inline bool parse_f64(const char* s, double& out) {
    if (s == nullptr) return false;
    char* end = nullptr;
    const double v = std::strtod(s, &end);
    if (end == s || (end != nullptr && *end != '\0')) return false;
    out = v;
    return true;
}

inline ParseArgsResult parse_args(int argc, char** argv) {
    ParseArgsResult r{};

    for (int i = 1; i < argc; ++i) {
        const std::string_view a(argv[i] != nullptr ? argv[i] : "");
        auto next = [&](const char*& out) -> bool {
            if (i + 1 >= argc) return false;
            out = argv[++i];
            return out != nullptr;
        };

        const char* v = nullptr;
        if (a == "--box-rows" && next(v)) { parse_i32(v, r.cfg.box_rows); continue; }
        if (a == "--box-cols" && next(v)) { parse_i32(v, r.cfg.box_cols); continue; }
        if (a == "--difficulty" && next(v)) { parse_i32(v, r.cfg.difficulty_level_required); continue; }
        if (a == "--required-strategy" && next(v)) {
            RequiredStrategy rs{};
            if (parse_required_strategy(v, rs)) r.cfg.required_strategy = rs;
            continue;
        }
        if (a == "--target" && next(v)) { parse_u64(v, r.cfg.target_puzzles); continue; }
        if (a == "--threads" && next(v)) { parse_i32(v, r.cfg.threads); continue; }
        if (a == "--seed" && next(v)) { parse_u64(v, r.cfg.seed); continue; }

        if (a == "--min-clues" && next(v)) { parse_i32(v, r.cfg.min_clues); continue; }
        if (a == "--max-clues" && next(v)) { parse_i32(v, r.cfg.max_clues); continue; }

        if (a == "--output-folder" && next(v)) { r.cfg.output_folder = v; continue; }
        if (a == "--output-file" && next(v)) { r.cfg.output_file = v; continue; }
        if (a == "--single-file-only") { r.cfg.write_individual_files = false; continue; }

        if (a == "--reseed-interval-s" && next(v)) { parse_i32(v, r.cfg.reseed_interval_s); continue; }
        if (a == "--force-new-seed") { r.cfg.force_new_seed_per_attempt = true; continue; }
        if (a == "--no-force-new-seed") { r.cfg.force_new_seed_per_attempt = false; continue; }

        if (a == "--attempt-time-budget-s" && next(v)) { parse_f64(v, r.cfg.attempt_time_budget_s); continue; }
        if (a == "--attempt-node-budget" && next(v)) { parse_u64(v, r.cfg.attempt_node_budget); continue; }
        if (a == "--max-attempts" && next(v)) { parse_u64(v, r.cfg.max_attempts); continue; }
        if (a == "--max-total-time-s" && next(v)) { parse_u64(v, r.cfg.max_total_time_s); continue; }

        if (a == "--symmetry-center") { r.cfg.symmetry_center = true; continue; }
        if (a == "--no-symmetry-center") { r.cfg.symmetry_center = false; continue; }

        if (a == "--pattern-forcing") { r.cfg.pattern_forcing_enabled = true; continue; }
        if (a == "--pattern-forcing-tries" && next(v)) { parse_i32(v, r.cfg.pattern_forcing_tries); continue; }
        if (a == "--pattern-anchor-count" && next(v)) { parse_i32(v, r.cfg.pattern_forcing_anchor_count); continue; }
        if (a == "--no-pattern-lock-anchors") { r.cfg.pattern_forcing_lock_anchors = false; continue; }

        if (a == "--mcts-digger") { r.cfg.mcts_digger_enabled = true; continue; }
        if (a == "--no-mcts-digger") { r.cfg.mcts_digger_enabled = false; continue; }
        if (a == "--mcts-profile" && next(v)) { r.cfg.mcts_tuning_profile = v; continue; }
        if (a == "--mcts-p7") { r.cfg.mcts_digger_enabled = true; r.cfg.mcts_tuning_profile = "p7"; continue; }
        if (a == "--mcts-p8") { r.cfg.mcts_digger_enabled = true; r.cfg.mcts_tuning_profile = "p8"; continue; }
        if (a == "--mcts-iterations" && next(v)) { parse_i32(v, r.cfg.mcts_digger_iterations); continue; }
        if (a == "--mcts-ucb-c" && next(v)) { parse_f64(v, r.cfg.mcts_ucb_c); continue; }
        if (a == "--mcts-fail-cap" && next(v)) { parse_i32(v, r.cfg.mcts_fail_cap); continue; }
        if (a == "--mcts-basic-level" && next(v)) { parse_i32(v, r.cfg.mcts_basic_logic_level); continue; }
        if (a == "--max-pattern-depth" && next(v)) { parse_i32(v, r.cfg.max_pattern_depth); continue; }

        if (a == "--strict-logical") { r.cfg.strict_logical = true; continue; }
        if (a == "--strict-canonical-strategies") { r.cfg.strict_canonical_strategies = true; continue; }
        if (a == "--allow-proxy-advanced") { r.cfg.allow_proxy_advanced = true; continue; }
        if (a == "--no-proxy-advanced") { r.cfg.allow_proxy_advanced = false; continue; }
        if (a == "--no-quality-contract") { r.cfg.enable_quality_contract = false; continue; }
        if (a == "--distribution-filter") { r.cfg.enable_distribution_filter = true; continue; }
        if (a == "--replay-validation") { r.cfg.enable_replay_validation = true; continue; }

        if (a == "--vip-grade-target" && next(v)) { r.cfg.vip_grade_target = v; continue; }
        if (a == "--vip-min-grade-by-geometry" && next(v)) { r.cfg.vip_min_grade_by_geometry_path = v; continue; }
        if (a == "--vip-score-profile" && next(v)) { r.cfg.vip_score_profile = v; continue; }

        if (a == "--cpu-backend" && next(v)) { r.cfg.cpu_backend = v; continue; }

        if (a == "--list-geometries") { r.list_geometries = true; continue; }
        if (a == "--validate-geometry") { r.validate_geometry = true; continue; }
        if (a == "--validate-geometry-catalog") { r.validate_geometry_catalog = true; continue; }
        if (a == "--run-regression-tests") { r.run_regression_tests = true; continue; }

        if (a == "--run-geometry-gate") { r.run_geometry_gate = true; if (i + 1 < argc && argv[i + 1][0] != '-') r.geometry_gate_report = argv[++i]; continue; }
        if (a == "--run-quality-benchmark") { r.run_quality_benchmark = true; if (i + 1 < argc && argv[i + 1][0] != '-') r.quality_benchmark_report = argv[++i]; continue; }
        if (a == "--quality-benchmark-max-cases" && next(v)) { parse_u64(v, r.quality_benchmark_max_cases); continue; }
        if (a == "--run-pre-difficulty-gate") { r.run_pre_difficulty_gate = true; if (i + 1 < argc && argv[i + 1][0] != '-') r.pre_difficulty_gate_report = argv[++i]; continue; }
        if (a == "--run-asym-pair-benchmark") { r.run_asym_pair_benchmark = true; if (i + 1 < argc && argv[i + 1][0] != '-') r.asym_pair_benchmark_report = argv[++i]; continue; }
        if (a == "--run-vip-benchmark") { r.run_vip_benchmark = true; if (i + 1 < argc && argv[i + 1][0] != '-') r.vip_benchmark_report = argv[++i]; continue; }
        if (a == "--run-vip-gate") { r.run_vip_gate = true; if (i + 1 < argc && argv[i + 1][0] != '-') r.vip_gate_report = argv[++i]; continue; }
        if (a == "--explain-profile") { r.explain_profile = true; continue; }

        if (a == "--benchmark-mode") { r.benchmark_mode = true; continue; }
        if (a == "--benchmark-output-file" && next(v)) { r.cfg.benchmark_output_file = v; continue; }
        if (a == "--fast-test") { r.cfg.fast_test_mode = true; continue; }
        if (a == "--no-fast-test") { r.cfg.fast_test_mode = false; continue; }

        if (a == "--stage-start") { r.cfg.stage_start = true; continue; }
        if (a == "--stage-end") { r.cfg.stage_end = true; continue; }
        if (a == "--perf-ab-suite") { r.cfg.perf_ab_suite = true; continue; }
    }

    r.cfg.difficulty_level_required = std::clamp(r.cfg.difficulty_level_required, 1, 9);
    r.cfg.max_pattern_depth = std::clamp(r.cfg.max_pattern_depth, 0, 8);
    if (r.cfg.min_clues < 0) r.cfg.min_clues = 0;
    if (r.cfg.max_clues < 0) r.cfg.max_clues = 0;

    if (r.cfg.target_puzzles == 0) {
        r.cfg.target_puzzles = 1;
    }

    return r;
}

} // namespace sudoku_hpc
