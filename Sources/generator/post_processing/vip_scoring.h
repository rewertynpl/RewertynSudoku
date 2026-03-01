// ============================================================================
// SUDOKU HPC - POST PROCESSING
// Moduł: vip_scoring.h
// Opis: Algorytmy heurystyczne oceniające jakość i trudność układu.
//       Kalkulacja VIP Score i przypisywanie rang (Bronze-Platinum).
// ============================================================================

#pragma once

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

#include "../../config/run_config.h"

namespace sudoku_hpc::post_processing {

inline double clamp01(double v) { 
    return std::clamp(v, 0.0, 1.0); 
}

// Szybkie formatowanie liczby zmiennoprzecinkowej bez globalnych alokacji (dla logów/json)
inline std::string format_fixed_vip(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

inline std::string normalize_vip_grade_target(const std::string& grade_raw) {
    std::string key;
    key.reserve(grade_raw.size());
    for (unsigned char ch : grade_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "bronze") return "bronze";
    if (key == "silver") return "silver";
    if (key == "platinum") return "platinum";
    return "gold"; // Default
}

inline std::string normalize_vip_score_profile(const std::string& profile_raw) {
    std::string key;
    key.reserve(profile_raw.size());
    for (unsigned char ch : profile_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "strict") return "strict";
    if (key == "ultra") return "ultra";
    return "standard";
}

inline std::string vip_grade_from_score(double score) {
    if (score >= 700.0) return "platinum";
    if (score >= 500.0) return "gold";
    if (score >= 300.0) return "silver";
    if (score > 0.0) return "bronze";
    return "none";
}

inline int vip_grade_rank(const std::string& grade) {
    const std::string g = normalize_vip_grade_target(grade);
    if (g == "bronze") return 1;
    if (g == "silver") return 2;
    if (g == "gold") return 3;
    if (g == "platinum") return 4;
    return 0;
}

struct VipScoreBreakdown {
    double logic_depth_norm = 0.0;
    double hidden_norm = 0.0;
    double naked_norm = 0.0;
    double uniqueness_norm = 0.0;
    double branching_norm = 0.0;
    double level_norm = 0.0;
    double weighted = 0.0;
    double asym_multiplier = 1.0;
    double final_score = 0.0;
    std::string profile = "standard";

    std::string to_json() const {
        std::ostringstream out;
        out << "{\"profile\":\"" << profile << "\","
            << "\"logic_depth_norm\":" << format_fixed_vip(logic_depth_norm, 6) << ","
            << "\"hidden_norm\":" << format_fixed_vip(hidden_norm, 6) << ","
            << "\"naked_norm\":" << format_fixed_vip(naked_norm, 6) << ","
            << "\"uniqueness_norm\":" << format_fixed_vip(uniqueness_norm, 6) << ","
            << "\"branching_norm\":" << format_fixed_vip(branching_norm, 6) << ","
            << "\"level_norm\":" << format_fixed_vip(level_norm, 6) << ","
            << "\"weighted\":" << format_fixed_vip(weighted, 6) << ","
            << "\"asym_multiplier\":" << format_fixed_vip(asym_multiplier, 6) << ","
            << "\"final_score\":" << format_fixed_vip(final_score, 6) << "}";
        return out.str();
    }
};

inline std::string geometry_key_for_grade_target(int box_rows, int box_cols) {
    return std::to_string(box_rows) + "x" + std::to_string(box_cols);
}

inline std::unordered_map<std::string, std::string> load_vip_grade_target_overrides(const std::string& path_raw) {
    std::unordered_map<std::string, std::string> out_map;
    if (path_raw.empty()) return out_map;
    
    std::ifstream in(path_raw);
    if (!in) return out_map;
    
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::replace(line.begin(), line.end(), ';', ',');
        std::istringstream iss(line);
        std::string n_s, br_s, bc_s, grade_s;
        
        if (std::getline(iss, n_s, ',') && std::getline(iss, br_s, ',') && 
            std::getline(iss, bc_s, ',') && std::getline(iss, grade_s, ',')) {
            try {
                out_map[geometry_key_for_grade_target(std::stoi(br_s), std::stoi(bc_s))] = normalize_vip_grade_target(grade_s);
            } catch (...) {}
        }
    }
    return out_map;
}

inline std::string resolve_vip_grade_target_for_geometry(const GenerateRunConfig& cfg) {
    const std::string default_grade = normalize_vip_grade_target(cfg.vip_grade_target);
    if (cfg.vip_min_grade_by_geometry_path.empty()) return default_grade;
    
    const auto overrides = load_vip_grade_target_overrides(cfg.vip_min_grade_by_geometry_path);
    const auto it = overrides.find(geometry_key_for_grade_target(cfg.box_rows, cfg.box_cols));
    
    return it != overrides.end() ? normalize_vip_grade_target(it->second) : default_grade;
}

// Analiza heurystyczna uwzględniająca statystyki solvera oraz specyfikę geometrii
inline VipScoreBreakdown compute_vip_score_breakdown(
    const GenerateRunResult& result, 
    const GenerateRunConfig& cfg, 
    double asymmetry_ratio) {
    
    VipScoreBreakdown b{};
    b.profile = normalize_vip_score_profile(cfg.vip_score_profile);
    
    const int n = std::max(1, cfg.box_rows) * std::max(1, cfg.box_cols);
    const double attempts = static_cast<double>(std::max<uint64_t>(1, result.attempts));
    
    const double logic_steps_per_attempt = static_cast<double>(result.logic_steps_total) / attempts;
    const double hidden_rate = static_cast<double>(result.strategy_hidden_hit) / attempts;
    const double naked_rate = static_cast<double>(result.strategy_naked_hit) / attempts;
    const double uniqueness_nodes_per_attempt = static_cast<double>(result.uniqueness_nodes) / attempts;
    const double reject_branching_rate = static_cast<double>(result.reject_logic + result.reject_strategy + result.reject_uniqueness) / attempts;
    
    const double asymmetry_norm = clamp01((asymmetry_ratio - 1.0) / 3.0);

    // Normalizacja parametrów względem szacowanych progów trudności dla rozmiaru N
    b.logic_depth_norm = clamp01(logic_steps_per_attempt / std::max(4.0, static_cast<double>(n) * 0.35));
    b.hidden_norm      = clamp01(hidden_rate / 0.60);
    b.naked_norm       = clamp01(naked_rate / 0.80);
    b.uniqueness_norm  = clamp01(uniqueness_nodes_per_attempt / std::max(500.0, static_cast<double>(n * n) * 0.90));
    b.branching_norm   = clamp01(reject_branching_rate / 0.85);
    b.level_norm       = clamp01(static_cast<double>(std::clamp(cfg.difficulty_level_required, 1, 9) - 1) / 8.0);

    // Profil wag
    double w_logic = 0.30, w_hidden = 0.22, w_naked = 0.12, w_uniq = 0.20, w_branch = 0.10, w_level = 0.06;
    
    if (b.profile == "strict") {
        w_logic = 0.34; w_hidden = 0.24; w_naked = 0.08; w_uniq = 0.22; w_branch = 0.08; w_level = 0.04;
    } else if (b.profile == "ultra") {
        w_logic = 0.36; w_hidden = 0.25; w_naked = 0.06; w_uniq = 0.23; w_branch = 0.07; w_level = 0.03;
    }
    
    b.weighted = w_logic * b.logic_depth_norm + 
                 w_hidden * b.hidden_norm + 
                 w_naked * b.naked_norm +
                 w_uniq * b.uniqueness_norm + 
                 w_branch * b.branching_norm + 
                 w_level * b.level_norm;
                 
    // Premia za trudność wniesioną przez geometrię asymetryczną
    b.asym_multiplier = 1.0 + 0.15 * asymmetry_norm;
    if (b.profile == "ultra") {
        b.asym_multiplier += 0.05 * asymmetry_norm;
    }
    
    b.final_score = std::clamp(b.weighted * b.asym_multiplier * 1000.0, 0.0, 1000.0);
    return b;
}

inline double compute_vip_score(const GenerateRunResult& result, const GenerateRunConfig& cfg, double asymmetry_ratio) {
    return compute_vip_score_breakdown(result, cfg, asymmetry_ratio).final_score;
}

inline bool vip_contract_passed(double score, const std::string& target_grade) {
    return vip_grade_rank(vip_grade_from_score(score)) >= vip_grade_rank(target_grade);
}

} // namespace sudoku_hpc::post_processing