#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <random>
#include <sstream>
#include <thread>
#include <optional>

namespace sudoku_hpc {

    constexpr int kN = 9;
    constexpr int kNN = 81;
    constexpr int kBoxRows = 3;
    constexpr int kBoxCols = 3;
    constexpr uint64_t kFullMask = static_cast<uint64_t>((1ULL << kN) - 1ULL);
            
enum class RequiredStrategy {
    None,
    NakedSingle,
    HiddenSingle,
    PointingPairs,
    BoxLineReduction,
    NakedPair,
    HiddenPair,
    NakedTriple,
    HiddenTriple,
    NakedQuad,
    HiddenQuad,
    XWing,
    YWing,
    Skyscraper,
    TwoStringKite,
    EmptyRectangle,
    RemotePairs,
    Swordfish,
    FinnedXWingSashimi,
    SimpleColoring,
    BUGPlusOne,
    UniqueRectangle,
    XYZWing,
    WWing,
    Jellyfish,
    XChain,
    XYChain,
    WXYZWing,
    FinnedSwordfishJellyfish,
    ALSXZ,
    UniqueLoop,
    AvoidableRectangle,
    BivalueOddagon,
    Medusa3D,
    AIC,
    GroupedAIC,
    GroupedXCycle,
    ContinuousNiceLoop,
    ALSXYWing,
    ALSChain,
    SueDeCoq,
    DeathBlossom,
    FrankenFish,
    MutantFish,
    KrakenFish,
    MSLS,
    Exocet,
    SeniorExocet,
    SKLoop,
    PatternOverlayMethod,
    ForcingChains,
    Backtracking
};

std::string to_string(RequiredStrategy value) {
    switch (value) {
    case RequiredStrategy::None:
        return "(none)";
    case RequiredStrategy::NakedSingle:
        return "NakedSingle";
    case RequiredStrategy::HiddenSingle:
        return "HiddenSingle";
    case RequiredStrategy::PointingPairs:
        return "PointingPairs";
    case RequiredStrategy::BoxLineReduction:
        return "BoxLineReduction";
    case RequiredStrategy::NakedPair:
        return "NakedPair";
    case RequiredStrategy::HiddenPair:
        return "HiddenPair";
    case RequiredStrategy::NakedTriple:
        return "NakedTriple";
    case RequiredStrategy::HiddenTriple:
        return "HiddenTriple";
    case RequiredStrategy::NakedQuad:
        return "NakedQuad";
    case RequiredStrategy::HiddenQuad:
        return "HiddenQuad";
    case RequiredStrategy::XWing:
        return "XWing";
    case RequiredStrategy::YWing:
        return "YWing";
    case RequiredStrategy::Skyscraper:
        return "Skyscraper";
    case RequiredStrategy::TwoStringKite:
        return "TwoStringKite";
    case RequiredStrategy::EmptyRectangle:
        return "EmptyRectangle";
    case RequiredStrategy::RemotePairs:
        return "RemotePairs";
    case RequiredStrategy::Swordfish:
        return "Swordfish";
    case RequiredStrategy::FinnedXWingSashimi:
        return "FinnedXWingSashimi";
    case RequiredStrategy::SimpleColoring:
        return "SimpleColoring";
    case RequiredStrategy::BUGPlusOne:
        return "BUGPlusOne";
    case RequiredStrategy::UniqueRectangle:
        return "UniqueRectangle";
    case RequiredStrategy::XYZWing:
        return "XYZWing";
    case RequiredStrategy::WWing:
        return "WWing";
    case RequiredStrategy::Jellyfish:
        return "Jellyfish";
    case RequiredStrategy::XChain:
        return "XChain";
    case RequiredStrategy::XYChain:
        return "XYChain";
    case RequiredStrategy::WXYZWing:
        return "WXYZWing";
    case RequiredStrategy::FinnedSwordfishJellyfish:
        return "FinnedSwordfishJellyfish";
    case RequiredStrategy::ALSXZ:
        return "ALSXZ";
    case RequiredStrategy::UniqueLoop:
        return "UniqueLoop";
    case RequiredStrategy::AvoidableRectangle:
        return "AvoidableRectangle";
    case RequiredStrategy::BivalueOddagon:
        return "BivalueOddagon";
    case RequiredStrategy::Medusa3D:
        return "Medusa3D";
    case RequiredStrategy::AIC:
        return "AIC";
    case RequiredStrategy::GroupedAIC:
        return "GroupedAIC";
    case RequiredStrategy::GroupedXCycle:
        return "GroupedXCycle";
    case RequiredStrategy::ContinuousNiceLoop:
        return "ContinuousNiceLoop";
    case RequiredStrategy::ALSXYWing:
        return "ALSXYWing";
    case RequiredStrategy::ALSChain:
        return "ALSChain";
    case RequiredStrategy::SueDeCoq:
        return "SueDeCoq";
    case RequiredStrategy::DeathBlossom:
        return "DeathBlossom";
    case RequiredStrategy::FrankenFish:
        return "FrankenFish";
    case RequiredStrategy::MutantFish:
        return "MutantFish";
    case RequiredStrategy::KrakenFish:
        return "KrakenFish";
    case RequiredStrategy::MSLS:
        return "MSLS";
    case RequiredStrategy::Exocet:
        return "Exocet";
    case RequiredStrategy::SeniorExocet:
        return "SeniorExocet";
    case RequiredStrategy::SKLoop:
        return "SKLoop";
    case RequiredStrategy::PatternOverlayMethod:
        return "PatternOverlayMethod";
    case RequiredStrategy::ForcingChains:
        return "ForcingChains";
    case RequiredStrategy::Backtracking:
        return "Backtracking";
    default:
        return "Unknown";
    }
}

struct GenerateRunConfig {
    int box_rows = 3;
    int box_cols = 3;
    uint64_t target_puzzles = 100;
    int min_clues = 24;
    int max_clues = 40;
    int difficulty_level_required = 1;
    RequiredStrategy required_strategy = RequiredStrategy::None;
    bool require_unique = true;
    bool strict_logical = true;
    int threads = 0;
    long long seed = 0;
    int reseed_interval_s = 1;
    double attempt_time_budget_s = 0.0;
    uint64_t attempt_node_budget = 0;
    uint64_t max_attempts = 0;
    uint64_t max_attempts_s = 0;
    uint64_t max_total_time_s = 0;  // Globalny limit czasu na całe uruchomienie (0=bez limitu)
    bool symmetry_center = false;
    bool pause_on_exit_windows = true;
    bool write_individual_files = true;
    bool benchmark_profiles_40s = false;
    int benchmark_seconds_per_profile = 40;
    bool enable_quality_contract = true;
    bool enable_replay_validation = true;
    bool enable_distribution_filter = true;
    double uniqueness_confirm_budget_s = 1.5;
    uint64_t uniqueness_confirm_budget_nodes = 1500000;
    std::string profile_mode_policy = "adaptive";  // adaptive|full|legacy
    int full_for_n_ge = 25;  // For adaptive policy: full pipeline for n>=full_for_n_ge
    bool vip_mode = false;
    bool vip_contract_strict = false;
    std::string cpu_backend_policy = "auto";  // auto|scalar|avx2|avx512
    bool cpu_dispatch_report = false;
    std::string asym_heuristics_mode = "balanced";  // off|balanced|aggressive
    bool adaptive_budget = false;
    std::string difficulty_engine = "standard";  // standard|vip
    std::string vip_score_profile = "standard";  // standard|strict|ultra
    std::string vip_trace_level = "basic";  // basic|full
    std::string vip_min_grade_by_geometry_path = "";
    std::string vip_grade_target = "gold";  // bronze|silver|gold|platinum
    std::string difficulty_trace_out = "";
    std::string vip_signature_out = "";
    bool perf_ab_suite = false;
    std::string perf_report_out = "plikiTMP/porownania/perf_ab_suite.txt";
    std::string perf_csv_out = "plikiTMP/porownania/perf_ab_suite.csv";
    std::string perf_baseline_csv = "";
    bool stage_start = false;
    bool stage_end = false;
    std::string stage_name = "";
    std::string stage_report_out = "plikiTMP/porownania/stage_summary.txt";
    std::string benchmark_output_file = "plikiTMP/porownanie.txt";
    std::string output_folder = "generated_sudoku_files";
    std::string output_file = "generated_sudoku.txt";
};

struct GenerateRunResult {
    uint64_t accepted = 0;
    uint64_t written = 0;
    uint64_t attempts = 0;
    uint64_t attempts_total = 0;
    uint64_t analyzed_required_strategy = 0;
    uint64_t required_strategy_hits = 0;
    uint64_t written_required_strategy = 0;
    uint64_t rejected = 0;
    uint64_t reject_prefilter = 0;
    uint64_t reject_logic = 0;
    uint64_t reject_uniqueness = 0;
    uint64_t reject_strategy = 0;
    uint64_t reject_replay = 0;
    uint64_t reject_distribution_bias = 0;
    uint64_t reject_uniqueness_budget = 0;
    uint64_t timeout_global = 0;  // Liczba timeoutów globalnych
    uint64_t timeout_per_attempt = 0;  // Liczba timeoutów per-attempt
    uint64_t uniqueness_calls = 0;
    uint64_t uniqueness_nodes = 0;
    double uniqueness_elapsed_ms = 0.0;
    double uniqueness_avg_ms = 0.0;
    uint64_t logic_steps_total = 0;
    uint64_t strategy_naked_use = 0;
    uint64_t strategy_naked_hit = 0;
    uint64_t strategy_hidden_use = 0;
    uint64_t strategy_hidden_hit = 0;
    std::string cpu_backend_selected = "scalar";
    double kernel_time_ms = 0.0;
    uint64_t kernel_calls = 0;
    double backend_efficiency_score = 0.0;
    double asymmetry_efficiency_index = 0.0;
    double vip_score = 0.0;
    std::string vip_grade = "none";
    bool vip_contract_ok = true;
    std::string vip_contract_fail_reason = "";
    std::string vip_score_breakdown_json = "{}";
    uint64_t premium_signature = 0;
    uint64_t premium_signature_v2 = 0;
    double elapsed_s = 0.0;
    double accepted_per_sec = 0.0;
    double avg_clues = 0.0;
    std::array<uint64_t, 10> histogram_levels{};
    std::array<uint64_t, 64> histogram_strategies{};
};

} // namespace sudoku_hpc
