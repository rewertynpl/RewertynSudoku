// ============================================================================
// SUDOKU HPC - MCTS DIGGER
// Moduł: mcts_ucb_policy.h
// Opis: Algorytm wyboru (Selection) oparty na UCB1 oraz dynamiczne
//       profile strojenia nagród (Advanced Tuning Profile).
// ============================================================================

#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <random>
#include <string>

// Moduł węzłów
#include "mcts_node.h"

// Do sprawdzenia required_strategy
#include "../../config/run_config.h"
#include "../../core/geometry.h"
#include "../../logic/logic_result.h" // dla Enum StrategySlot

namespace sudoku_hpc::mcts_digger {

// Profil konfiguracji nagród używany w fazie Rollout / Simulation
struct MctsAdvancedTuning {
    bool enabled = false;
    int eval_stride = 8;
    int near_window = 2;
    double p7_hit_weight = 1.5;
    double p8_hit_weight = 2.5;
    double required_hit_weight = 4.0;
    double p8_miss_penalty = 1.5;
    double min_reward = -8.0;
    bool require_p8_signal_for_stop = false;
};

// Szybkie testy na poziomy trudności (zapobiega twardemu wiązaniu zależności nagłówków)
inline bool mcts_is_level7_strategy(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::AIC:
    case RequiredStrategy::GroupedAIC:
    case RequiredStrategy::GroupedXCycle:
    case RequiredStrategy::ContinuousNiceLoop:
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::SueDeCoq:
    case RequiredStrategy::DeathBlossom:
    case RequiredStrategy::FrankenFish:
    case RequiredStrategy::MutantFish:
    case RequiredStrategy::KrakenFish:
    case RequiredStrategy::AlignedPairExclusion:
    case RequiredStrategy::AlignedTripleExclusion:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::Squirmbag:
        return true;
    default:
        return false;
    }
}

inline bool mcts_is_level8_strategy(RequiredStrategy rs) {
    switch (rs) {
    case RequiredStrategy::MSLS:
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
    case RequiredStrategy::SKLoop:
    case RequiredStrategy::PatternOverlayMethod:
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return true;
    default:
        return false;
    }
}

// Normalizacja ciągu wejściowego profilu
inline std::string mcts_normalize_profile(const std::string& raw) {
    std::string key;
    key.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "off" || key == "none" || key == "disabled") return "off";
    if (key == "p7" || key == "level7" || key == "nightmare") return "p7";
    if (key == "p8" || key == "level8" || key == "theoretical") return "p8";
    return "auto";
}

// Tworzy zestaw wag dla nagród (Reward Function) na podstawie profilu
inline MctsAdvancedTuning resolve_mcts_advanced_tuning(
    const GenerateRunConfig& cfg,
    const GenericTopology& topo) {
    
    const bool wants_p7 =
        (cfg.difficulty_level_required >= 7) ||
        mcts_is_level7_strategy(cfg.required_strategy) ||
        mcts_is_level8_strategy(cfg.required_strategy);
        
    const bool wants_p8 =
        (cfg.difficulty_level_required >= 8) ||
        mcts_is_level8_strategy(cfg.required_strategy);

    std::string profile = mcts_normalize_profile(cfg.mcts_tuning_profile);
    if (profile == "auto") {
        profile = wants_p8 ? "p8" : (wants_p7 ? "p7" : "off");
    }

    MctsAdvancedTuning t{};
    if (profile == "off") {
        t.enabled = false;
        return t;
    }

    t.enabled = true;
    if (profile == "p8") {
        t.eval_stride = 2; // Ocena zaawansowana częściej
        t.near_window = std::max(4, topo.n);
        t.p7_hit_weight = 2.0;
        t.p8_hit_weight = 5.5;
        t.required_hit_weight = 9.0;
        t.p8_miss_penalty = 3.5;
        t.min_reward = -14.0;
        t.require_p8_signal_for_stop = true;
        return t;
    }

    // Default "p7" (Level 7)
    t.eval_stride = 4;
    t.near_window = std::max(3, topo.n / 2);
    t.p7_hit_weight = 2.5;
    t.p8_hit_weight = 3.0;
    t.required_hit_weight = 6.0;
    t.p8_miss_penalty = wants_p8 ? 1.0 : 0.0;
    t.min_reward = -10.0;
    t.require_p8_signal_for_stop = false;
    return t;
}

// Implementacja wyboru węzła wg równania UCB1
inline int select_ucb_action(const MctsNodeScratch& sc, std::mt19937_64& rng, double c_param) {
    if (sc.active_count <= 0) {
        return -1;
    }
    
    // Faza 1: Losowy wybór nieodwiedzonego węzła (Rezerwuar Sampling dla optymalnej losowości)
    int unseen_pick = -1;
    int unseen_seen = 0;
    for (int i = 0; i < sc.active_count; ++i) {
        const int cell = sc.active_cells[static_cast<size_t>(i)];
        if (cell < 0) continue;
        
        if (sc.visits[static_cast<size_t>(cell)] == 0U) {
            ++unseen_seen;
            if ((rng() % static_cast<uint64_t>(unseen_seen)) == 0ULL) {
                unseen_pick = cell;
            }
        }
    }
    
    // Jeśli są węzły, których jeszcze nie testowaliśmy (0 wizyt), wymuś eksplorację
    if (unseen_pick >= 0) {
        return unseen_pick;
    }

    // Faza 2: Górna Granica Ufności (UCB1 formula) - Wszystkie odwiedzone
    // UCB1 = (w_i / n_i) + c * sqrt(ln(N) / n_i)
    const double log_total = std::log(static_cast<double>(std::max<uint64_t>(1ULL, sc.total_visits)));
    
    int best_cell = -1;
    double best_score = -std::numeric_limits<double>::infinity();
    
    for (int i = 0; i < sc.active_count; ++i) {
        const int cell = sc.active_cells[static_cast<size_t>(i)];
        if (cell < 0) continue;
        
        const uint32_t v = sc.visits[static_cast<size_t>(cell)];
        // Safeguard (na wypadek race'a w wywołaniach, choć system jest jednowątkowy logicznie)
        if (v == 0U) {
            return cell;
        }
        
        const double inv_v = 1.0 / static_cast<double>(v);
        const double exploit = sc.reward_sum[static_cast<size_t>(cell)] * inv_v;
        const double explore = c_param * std::sqrt(log_total * inv_v);
        const double score = exploit + explore;
        
        if (score > best_score) {
            best_score = score;
            best_cell = cell;
        }
    }
    
    return best_cell;
}

} // namespace sudoku_hpc::mcts_digger