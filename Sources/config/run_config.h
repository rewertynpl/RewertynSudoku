//Author copyright Marcin Matysek (Rewertyn)
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

namespace sudoku_hpc {

enum class RequiredStrategy : int {
    None = 0,
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
    UniqueRectangleExtended,
    HiddenUniqueRectangle,
    BUGType2,
    BUGType3,
    BUGType4,
    BorescoperQiuDeadlyPattern,
    Medusa3D,
    AIC,
    GroupedAIC,
    GroupedXCycle,
    ContinuousNiceLoop,
    ALSXYWing,
    ALSChain,
    AlignedPairExclusion,
    AlignedTripleExclusion,
    ALSAIC,
    SueDeCoq,
    DeathBlossom,
    FrankenFish,
    MutantFish,
    KrakenFish,
    Squirmbag,
    MSLS,
    Exocet,
    SeniorExocet,
    SKLoop,
    PatternOverlayMethod,
    ForcingChains,
    DynamicForcingChains,
    Backtracking,
};

enum class RejectReason : uint8_t {
    None = 0,
    Prefilter,
    Logic,
    Uniqueness,
    Strategy,
    Replay,
    DistributionBias,
    UniquenessBudget,
};

struct RequiredStrategyAttemptInfo {
    bool analyzed_required_strategy = false;
    bool required_strategy_use_confirmed = false;
    bool required_strategy_hit_confirmed = false;
    bool matched_required_strategy = false;
};

struct ClueRange {
    int min_clues = 0;
    int max_clues = 0;
};

struct GenerateRunConfig {
    int box_rows = 3;
    int box_cols = 3;

    uint64_t target_puzzles = 100;
    int min_clues = 0;
    int max_clues = 0;

    int difficulty_level_required = 1;
    RequiredStrategy required_strategy = RequiredStrategy::None;

    int threads = 0;
    uint64_t seed = 0;

    int reseed_interval_s = 0;
    bool force_new_seed_per_attempt = true;

    double attempt_time_budget_s = 0.0;
    uint64_t attempt_node_budget = 0;
    uint64_t max_attempts = 0;
    uint64_t max_total_time_s = 0;

    bool symmetry_center = false;
    bool require_unique = true;
    bool write_individual_files = true;
    bool pause_on_exit_windows = false;

    std::string output_folder = "generated_sudoku_files";
    std::string output_file = "generated_sudoku.txt";

    bool pattern_forcing_enabled = false;
    int pattern_forcing_tries = 6;
    int pattern_forcing_anchor_count = 0;
    bool pattern_forcing_lock_anchors = true;

    bool mcts_digger_enabled = true;
    std::string mcts_tuning_profile = "auto";
    int mcts_digger_iterations = 0;
    double mcts_ucb_c = 1.41;
    int mcts_fail_cap = 0;
    int mcts_basic_logic_level = 5;
    int max_pattern_depth = 0;

    bool strict_logical = false;
    bool strict_canonical_strategies = false;
    bool allow_proxy_advanced = true;
    bool enable_quality_contract = true;
    bool enable_distribution_filter = false;
    bool enable_replay_validation = false;

    std::string vip_grade_target = "gold";
    std::string vip_min_grade_by_geometry_path;
    std::string vip_score_profile = "standard";

    std::string cpu_backend = "scalar";

    bool stage_start = false;
    bool stage_end = false;
    bool perf_ab_suite = false;
    bool fast_test_mode = false;
    std::string benchmark_output_file = "benchmark_report.txt";
};

struct GenerateRunResult {
    uint64_t accepted = 0;
    uint64_t written = 0;
    uint64_t attempts = 0;
    uint64_t rejected = 0;

    uint64_t reject_prefilter = 0;
    uint64_t reject_logic = 0;
    uint64_t reject_uniqueness = 0;
    uint64_t reject_strategy = 0;
    uint64_t reject_replay = 0;
    uint64_t reject_distribution_bias = 0;
    uint64_t reject_uniqueness_budget = 0;

    uint64_t uniqueness_calls = 0;
    uint64_t uniqueness_nodes = 0;
    double uniqueness_elapsed_ms = 0.0;
    double uniqueness_avg_ms = 0.0;

    std::string cpu_backend_selected = "scalar";
    double kernel_time_ms = 0.0;
    uint64_t kernel_calls = 0;
    double backend_efficiency_score = 0.0;
    double asymmetry_efficiency_index = 0.0;

    uint64_t logic_steps_total = 0;
    uint64_t strategy_naked_use = 0;
    uint64_t strategy_naked_hit = 0;
    uint64_t strategy_hidden_use = 0;
    uint64_t strategy_hidden_hit = 0;

    double vip_score = 0.0;
    std::string vip_grade = "none";
    bool vip_contract_ok = false;
    std::string vip_contract_fail_reason;

    std::string premium_signature;
    std::string premium_signature_v2;

    double elapsed_s = 0.0;
    double accepted_per_sec = 0.0;
};

inline std::string normalize_token(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (const unsigned char ch : in) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

inline std::string to_string(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::None: return "none";
        case RequiredStrategy::NakedSingle: return "nakedsingle";
        case RequiredStrategy::HiddenSingle: return "hiddensingle";
        case RequiredStrategy::PointingPairs: return "pointingpairs";
        case RequiredStrategy::BoxLineReduction: return "boxlinereduction";
        case RequiredStrategy::NakedPair: return "nakedpair";
        case RequiredStrategy::HiddenPair: return "hiddenpair";
        case RequiredStrategy::NakedTriple: return "nakedtriple";
        case RequiredStrategy::HiddenTriple: return "hiddentriple";
        case RequiredStrategy::NakedQuad: return "nakedquad";
        case RequiredStrategy::HiddenQuad: return "hiddenquad";
        case RequiredStrategy::XWing: return "xwing";
        case RequiredStrategy::YWing: return "ywing";
        case RequiredStrategy::Skyscraper: return "skyscraper";
        case RequiredStrategy::TwoStringKite: return "twostringkite";
        case RequiredStrategy::EmptyRectangle: return "emptyrectangle";
        case RequiredStrategy::RemotePairs: return "remotepairs";
        case RequiredStrategy::Swordfish: return "swordfish";
        case RequiredStrategy::FinnedXWingSashimi: return "finnedxwingsashimi";
        case RequiredStrategy::SimpleColoring: return "simplecoloring";
        case RequiredStrategy::BUGPlusOne: return "bugplusone";
        case RequiredStrategy::UniqueRectangle: return "uniquerectangle";
        case RequiredStrategy::XYZWing: return "xyzwing";
        case RequiredStrategy::WWing: return "wwing";
        case RequiredStrategy::Jellyfish: return "jellyfish";
        case RequiredStrategy::XChain: return "xchain";
        case RequiredStrategy::XYChain: return "xychain";
        case RequiredStrategy::WXYZWing: return "wxyzwing";
        case RequiredStrategy::FinnedSwordfishJellyfish: return "finnedswordfishjellyfish";
        case RequiredStrategy::ALSXZ: return "alsxz";
        case RequiredStrategy::UniqueLoop: return "uniqueloop";
        case RequiredStrategy::AvoidableRectangle: return "avoidablerectangle";
        case RequiredStrategy::BivalueOddagon: return "bivalueoddagon";
        case RequiredStrategy::UniqueRectangleExtended: return "uniquerectangleextended";
        case RequiredStrategy::HiddenUniqueRectangle: return "hiddenuniquerectangle";
        case RequiredStrategy::BUGType2: return "bugtype2";
        case RequiredStrategy::BUGType3: return "bugtype3";
        case RequiredStrategy::BUGType4: return "bugtype4";
        case RequiredStrategy::BorescoperQiuDeadlyPattern: return "borescoperqiudeadlypattern";
        case RequiredStrategy::Medusa3D: return "medusa3d";
        case RequiredStrategy::AIC: return "aic";
        case RequiredStrategy::GroupedAIC: return "groupedaic";
        case RequiredStrategy::GroupedXCycle: return "groupedxcycle";
        case RequiredStrategy::ContinuousNiceLoop: return "continuousniceloop";
        case RequiredStrategy::ALSXYWing: return "alsxywing";
        case RequiredStrategy::ALSChain: return "alschain";
        case RequiredStrategy::AlignedPairExclusion: return "alignedpairexclusion";
        case RequiredStrategy::AlignedTripleExclusion: return "alignedtripleexclusion";
        case RequiredStrategy::ALSAIC: return "alsaic";
        case RequiredStrategy::SueDeCoq: return "suedecoq";
        case RequiredStrategy::DeathBlossom: return "deathblossom";
        case RequiredStrategy::FrankenFish: return "frankenfish";
        case RequiredStrategy::MutantFish: return "mutantfish";
        case RequiredStrategy::KrakenFish: return "krakenfish";
        case RequiredStrategy::Squirmbag: return "squirmbag";
        case RequiredStrategy::MSLS: return "msls";
        case RequiredStrategy::Exocet: return "exocet";
        case RequiredStrategy::SeniorExocet: return "seniorexocet";
        case RequiredStrategy::SKLoop: return "skloop";
        case RequiredStrategy::PatternOverlayMethod: return "patternoverlaymethod";
        case RequiredStrategy::ForcingChains: return "forcingchains";
        case RequiredStrategy::DynamicForcingChains: return "dynamicforcingchains";
        case RequiredStrategy::Backtracking: return "backtracking";
    }
    return "none";
}

inline bool parse_required_strategy(std::string_view raw, RequiredStrategy& out) {
    const std::string key = normalize_token(raw);
    if (key.empty() || key == "none") { out = RequiredStrategy::None; return true; }

    static const std::array<std::pair<std::string_view, RequiredStrategy>, 63> map = {{
        {"nakedsingle", RequiredStrategy::NakedSingle},
        {"hiddensingle", RequiredStrategy::HiddenSingle},
        {"pointingpairs", RequiredStrategy::PointingPairs},
        {"boxlinereduction", RequiredStrategy::BoxLineReduction},
        {"nakedpair", RequiredStrategy::NakedPair},
        {"hiddenpair", RequiredStrategy::HiddenPair},
        {"nakedtriple", RequiredStrategy::NakedTriple},
        {"hiddentriple", RequiredStrategy::HiddenTriple},
        {"nakedquad", RequiredStrategy::NakedQuad},
        {"hiddenquad", RequiredStrategy::HiddenQuad},
        {"xwing", RequiredStrategy::XWing},
        {"ywing", RequiredStrategy::YWing},
        {"skyscraper", RequiredStrategy::Skyscraper},
        {"twostringkite", RequiredStrategy::TwoStringKite},
        {"emptyrectangle", RequiredStrategy::EmptyRectangle},
        {"remotepairs", RequiredStrategy::RemotePairs},
        {"swordfish", RequiredStrategy::Swordfish},
        {"finnedxwingsashimi", RequiredStrategy::FinnedXWingSashimi},
        {"simplecoloring", RequiredStrategy::SimpleColoring},
        {"bugplusone", RequiredStrategy::BUGPlusOne},
        {"uniquerectangle", RequiredStrategy::UniqueRectangle},
        {"xyzwing", RequiredStrategy::XYZWing},
        {"wwing", RequiredStrategy::WWing},
        {"jellyfish", RequiredStrategy::Jellyfish},
        {"xchain", RequiredStrategy::XChain},
        {"xychain", RequiredStrategy::XYChain},
        {"wxyzwing", RequiredStrategy::WXYZWing},
        {"finnedswordfishjellyfish", RequiredStrategy::FinnedSwordfishJellyfish},
        {"alsxz", RequiredStrategy::ALSXZ},
        {"uniqueloop", RequiredStrategy::UniqueLoop},
        {"avoidablerectangle", RequiredStrategy::AvoidableRectangle},
        {"bivalueoddagon", RequiredStrategy::BivalueOddagon},
        {"uniquerectangleextended", RequiredStrategy::UniqueRectangleExtended},
        {"hiddenuniquerectangle", RequiredStrategy::HiddenUniqueRectangle},
        {"bugtype2", RequiredStrategy::BUGType2},
        {"bugtype3", RequiredStrategy::BUGType3},
        {"bugtype4", RequiredStrategy::BUGType4},
        {"borescoperqiudeadlypattern", RequiredStrategy::BorescoperQiuDeadlyPattern},
        {"medusa3d", RequiredStrategy::Medusa3D},
        {"aic", RequiredStrategy::AIC},
        {"groupedaic", RequiredStrategy::GroupedAIC},
        {"groupedxcycle", RequiredStrategy::GroupedXCycle},
        {"continuousniceloop", RequiredStrategy::ContinuousNiceLoop},
        {"alsxywing", RequiredStrategy::ALSXYWing},
        {"alschain", RequiredStrategy::ALSChain},
        {"alignedpairexclusion", RequiredStrategy::AlignedPairExclusion},
        {"alignedtripleexclusion", RequiredStrategy::AlignedTripleExclusion},
        {"alsaic", RequiredStrategy::ALSAIC},
        {"suedecoq", RequiredStrategy::SueDeCoq},
        {"deathblossom", RequiredStrategy::DeathBlossom},
        {"frankenfish", RequiredStrategy::FrankenFish},
        {"mutantfish", RequiredStrategy::MutantFish},
        {"krakenfish", RequiredStrategy::KrakenFish},
        {"squirmbag", RequiredStrategy::Squirmbag},
        {"msls", RequiredStrategy::MSLS},
        {"exocet", RequiredStrategy::Exocet},
        {"seniorexocet", RequiredStrategy::SeniorExocet},
        {"skloop", RequiredStrategy::SKLoop},
        {"patternoverlaymethod", RequiredStrategy::PatternOverlayMethod},
        {"forcingchains", RequiredStrategy::ForcingChains},
        {"dynamicforcingchains", RequiredStrategy::DynamicForcingChains},
        {"backtracking", RequiredStrategy::Backtracking},
        {"bruteforce", RequiredStrategy::Backtracking},
    }};

    for (const auto& [name, value] : map) {
        if (key == name) {
            out = value;
            return true;
        }
    }
    return false;
}

inline bool is_geometry_size_supported(int box_rows, int box_cols) {
    if (box_rows <= 0 || box_cols <= 0) {
        return false;
    }
    const int n = box_rows * box_cols;
    return n >= 4 && n <= 64;
}

inline bool difficulty_level_selectable_for_geometry(int level, int box_rows, int box_cols) {
    if (!is_geometry_size_supported(box_rows, box_cols)) {
        return false;
    }
    return level >= 1 && level <= 9;
}

inline int strategy_min_level(RequiredStrategy rs) {
    switch (rs) {
        case RequiredStrategy::None: return 1;
        case RequiredStrategy::NakedSingle:
        case RequiredStrategy::HiddenSingle:
            return 1;
        case RequiredStrategy::PointingPairs:
        case RequiredStrategy::BoxLineReduction:
        case RequiredStrategy::NakedPair:
        case RequiredStrategy::HiddenPair:
        case RequiredStrategy::NakedTriple:
        case RequiredStrategy::HiddenTriple:
            return 2;
        case RequiredStrategy::NakedQuad:
        case RequiredStrategy::HiddenQuad:
        case RequiredStrategy::XWing:
        case RequiredStrategy::YWing:
        case RequiredStrategy::Skyscraper:
        case RequiredStrategy::TwoStringKite:
        case RequiredStrategy::EmptyRectangle:
        case RequiredStrategy::RemotePairs:
            return 4;
        case RequiredStrategy::Swordfish:
        case RequiredStrategy::FinnedXWingSashimi:
        case RequiredStrategy::SimpleColoring:
        case RequiredStrategy::BUGPlusOne:
        case RequiredStrategy::UniqueRectangle:
        case RequiredStrategy::XYZWing:
        case RequiredStrategy::WWing:
            return 5;
        case RequiredStrategy::Jellyfish:
        case RequiredStrategy::XChain:
        case RequiredStrategy::XYChain:
        case RequiredStrategy::WXYZWing:
        case RequiredStrategy::FinnedSwordfishJellyfish:
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
            return 6;
        case RequiredStrategy::Medusa3D:
        case RequiredStrategy::AIC:
        case RequiredStrategy::GroupedAIC:
        case RequiredStrategy::GroupedXCycle:
        case RequiredStrategy::ContinuousNiceLoop:
        case RequiredStrategy::ALSXYWing:
        case RequiredStrategy::ALSChain:
        case RequiredStrategy::AlignedPairExclusion:
        case RequiredStrategy::AlignedTripleExclusion:
        case RequiredStrategy::ALSAIC:
        case RequiredStrategy::SueDeCoq:
        case RequiredStrategy::DeathBlossom:
        case RequiredStrategy::FrankenFish:
        case RequiredStrategy::MutantFish:
        case RequiredStrategy::KrakenFish:
        case RequiredStrategy::Squirmbag:
            return 7;
        case RequiredStrategy::MSLS:
        case RequiredStrategy::Exocet:
        case RequiredStrategy::SeniorExocet:
        case RequiredStrategy::SKLoop:
        case RequiredStrategy::PatternOverlayMethod:
        case RequiredStrategy::ForcingChains:
        case RequiredStrategy::DynamicForcingChains:
            return 8;
        case RequiredStrategy::Backtracking:
            return 9;
    }
    return 9;
}

inline bool required_strategy_selectable_for_geometry(RequiredStrategy rs, int box_rows, int box_cols) {
    if (!is_geometry_size_supported(box_rows, box_cols)) {
        return false;
    }
    const int n = box_rows * box_cols;
    if (n < 5 && strategy_min_level(rs) >= 6) {
        return false;
    }
    return true;
}

inline double suggest_time_budget_s(int box_rows, int box_cols, int difficulty_level) {
    const int n = std::max(1, box_rows) * std::max(1, box_cols);
    const int lvl = std::clamp(difficulty_level, 1, 9);
    const double base = 0.018 * static_cast<double>(n) * static_cast<double>(n);
    const double diff = 0.85 + static_cast<double>(lvl) * 0.55;
    return std::clamp(base + diff, 1.0, 300.0);
}

inline int strategy_adjusted_level(int difficulty_level, RequiredStrategy required) {
    return std::max(std::clamp(difficulty_level, 1, 9), strategy_min_level(required));
}

inline int suggest_reseed_interval_s(int box_rows, int box_cols, int effective_level) {
    const int n = std::max(1, box_rows) * std::max(1, box_cols);
    const int lvl = std::clamp(effective_level, 1, 9);
    const int base = std::max(1, n / 3);
    return std::clamp(base + lvl, 2, 90);
}

inline int suggest_attempt_time_budget_seconds(int box_rows, int box_cols, int effective_level) {
    return static_cast<int>(std::ceil(suggest_time_budget_s(box_rows, box_cols, effective_level)));
}

inline uint64_t suggest_attempt_node_budget(int box_rows, int box_cols, int effective_level) {
    const uint64_t n = static_cast<uint64_t>(std::max(1, box_rows) * std::max(1, box_cols));
    const uint64_t lvl = static_cast<uint64_t>(std::clamp(effective_level, 1, 9));
    return std::clamp<uint64_t>(n * n * (200 + 60 * lvl), 50'000ULL, 20'000'000ULL);
}

inline ClueRange resolve_auto_clue_range(int box_rows, int box_cols, int difficulty_level, RequiredStrategy required) {
    const int n = std::max(1, box_rows) * std::max(1, box_cols);
    const int nn = n * n;
    const int lvl = std::max(std::clamp(difficulty_level, 1, 9), strategy_min_level(required));

    const double ratio_hi = std::clamp(0.62 - 0.035 * static_cast<double>(lvl - 1), 0.18, 0.70);
    const double ratio_lo = std::clamp(ratio_hi - 0.10, 0.12, ratio_hi);

    int min_clues = static_cast<int>(ratio_lo * static_cast<double>(nn));
    int max_clues = static_cast<int>(ratio_hi * static_cast<double>(nn));

    min_clues = std::max(min_clues, std::max(4, n));
    max_clues = std::max(max_clues, min_clues + std::max(2, n / 4));

    min_clues = std::clamp(min_clues, 0, nn);
    max_clues = std::clamp(max_clues, min_clues, nn);

    return {min_clues, max_clues};
}

inline std::string explain_generation_profile_text(const GenerateRunConfig& cfg) {
    std::ostringstream out;
    out << "Generation profile\n";
    out << "geometry=" << cfg.box_rows << "x" << cfg.box_cols << " (n=" << (cfg.box_rows * cfg.box_cols) << ")\n";
    out << "difficulty=" << cfg.difficulty_level_required << " required_strategy=" << to_string(cfg.required_strategy) << "\n";
    out << "mcts=" << (cfg.mcts_digger_enabled ? "on" : "off") << " profile=" << cfg.mcts_tuning_profile << "\n";
    out << "pattern_forcing=" << (cfg.pattern_forcing_enabled ? "on" : "off") << " tries=" << cfg.pattern_forcing_tries << "\n";
    out << "strict_canonical=" << (cfg.strict_canonical_strategies ? "on" : "off")
        << " allow_proxy_advanced=" << (cfg.allow_proxy_advanced ? "on" : "off")
        << " max_pattern_depth=" << cfg.max_pattern_depth << "\n";
    out << "fast_test_mode=" << (cfg.fast_test_mode ? "on" : "off") << "\n";
    out << "quality_contract=" << (cfg.enable_quality_contract ? "on" : "off") << " replay=" << (cfg.enable_replay_validation ? "on" : "off") << "\n";
    return out.str();
}

} // namespace sudoku_hpc
