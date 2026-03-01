// ============================================================================
// SUDOKU HPC - MCTS DIGGER
// Moduł: bottleneck_digger.h
// Opis: Główny silnik kopacza (Digger) używający Monte Carlo Tree Search.
//       Szuka logicznych "wąskich gardeł" (bottlenecks) poprzez symulacje usuwania.
//       Zero-Allocation w gorącej pętli.
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "mcts_node.h"
#include "mcts_ucb_policy.h"

// Zależności do głównych silników (zostaną dostarczone w fazach 3 i 5)
#include "../../core/board.h"
#include "../../config/run_config.h"
#include "../core_engines/dlx_solver.h" // GenericUniquenessCounter
#include "../../logic/sudoku_logic_engine.h" // GenericLogicCertify i GenericLogicCertifyResult

namespace sudoku_hpc::mcts_digger {

using core_engines::GenericUniquenessCounter;
using core_engines::SearchAbortControl;
using logic::GenericLogicCertify;
using logic::GenericLogicCertifyResult;

// Helper: mapuje enum RequiredStrategy na indeks tablicy statystyk w Certyfikatorze
inline bool mcts_required_strategy_slot(RequiredStrategy rs, size_t& out_slot) {
    return GenericLogicCertify::slot_from_required_strategy(rs, out_slot);
}

class GenericMctsBottleneckDigger {
public:
    struct RunStats {
        bool used = false;
        bool bottleneck_hit = false;
        int accepted_removals = 0;
        int rejected_uniqueness = 0;
        int rejected_logic_timeout = 0;
        int iterations = 0;
        int advanced_evals = 0;
        int advanced_p7_hits = 0;
        int advanced_p8_hits = 0;
        int required_strategy_hits = 0;
    };

    // Przeprowadza proces "kopania" na gotowej planszy (solved)
    bool dig_into(
        const std::vector<uint16_t>& solved,
        const GenericTopology& topo,
        const GenerateRunConfig& cfg,
        std::mt19937_64& rng,
        const GenericUniquenessCounter& uniq,
        const GenericLogicCertify& logic,
        std::vector<uint16_t>& out_puzzle,
        int& out_clues,
        const uint8_t* protected_cells = nullptr,
        SearchAbortControl* budget = nullptr,
        RunStats* stats = nullptr) const {

        if (stats != nullptr) {
            *stats = {};
            stats->used = true;
        }

        // Kopia startowa planszy
        out_puzzle.resize(solved.size());
        std::copy(solved.begin(), solved.end(), out_puzzle.begin());

        int min_clues = std::clamp(cfg.min_clues, 0, topo.nn);
        int max_clues = std::clamp(cfg.max_clues, min_clues, topo.nn);
        std::uniform_int_distribution<int> pick_target(min_clues, max_clues);
        const int target_clues = pick_target(rng);

        // Reset bufora MCTS (Zero-Allocation)
        MctsNodeScratch& sc = tls_mcts_node_scratch();
        sc.reset(topo.nn);

        // Aktywacja wszystkich niechronionych komórek
        for (int idx = 0; idx < topo.nn; ++idx) {
            if (protected_cells != nullptr && protected_cells[static_cast<size_t>(idx)] != 0) {
                continue;
            }
            sc.activate(idx);
        }

        int clues = topo.nn;
        int fail_streak = 0;
        
        const int fail_cap = std::max(16, cfg.mcts_fail_cap);
        const int iter_cap = (cfg.mcts_digger_iterations > 0) ? cfg.mcts_digger_iterations : std::max(256, topo.nn * 8);
        const int basic_level = std::clamp(cfg.mcts_basic_logic_level, 1, 5);
        const double ucb_c = std::clamp(cfg.mcts_ucb_c, 0.1, 4.0);
        
        const MctsAdvancedTuning tuning = resolve_mcts_advanced_tuning(cfg, topo);
        int advanced_level = std::clamp(std::max(6, cfg.difficulty_level_required), 6, 8);
        if (cfg.max_pattern_depth > 0) {
            advanced_level = std::min(advanced_level, cfg.max_pattern_depth);
            advanced_level = std::clamp(advanced_level, 6, 8);
        }
        const bool wants_p8 = (cfg.difficulty_level_required >= 8) || mcts_is_level8_strategy(cfg.required_strategy);
        
        size_t required_slot = 0;
        const bool has_required_slot = mcts_required_strategy_slot(cfg.required_strategy, required_slot);

        // Główna pętla MCTS
        for (int iter = 0; iter < iter_cap; ++iter) {
            if (stats != nullptr) stats->iterations = iter + 1;
            if (budget != nullptr && !budget->step()) return false;
            if (clues <= target_clues || sc.active_count <= 0 || fail_streak >= fail_cap) break;

            // Faza 1: Wybór akcji wg. UCB1
            const int idx = select_ucb_action(sc, rng, ucb_c);
            if (idx < 0) break;
            
            // Pusta komórka? Wyłącz ją.
            if (out_puzzle[static_cast<size_t>(idx)] == 0) {
                sc.disable(idx);
                continue;
            }

            int sym_idx = -1;
            bool remove_pair = false;
            
            // Sprawdzenie symetrii
            if (cfg.symmetry_center) {
                sym_idx = topo.cell_center_sym[static_cast<size_t>(idx)];
                if (sym_idx >= 0 && sym_idx != idx && out_puzzle[static_cast<size_t>(sym_idx)] != 0) {
                    if (!(protected_cells != nullptr && protected_cells[static_cast<size_t>(sym_idx)] != 0)) {
                        remove_pair = true;
                    }
                }
            }

            const int removal = remove_pair ? 2 : 1;
            if (clues - removal < target_clues) {
                sc.disable(idx);
                if (remove_pair) sc.disable(sym_idx);
                continue;
            }

            // Symulacja usunięcia
            const uint16_t old_a = out_puzzle[static_cast<size_t>(idx)];
            const uint16_t old_b = remove_pair ? out_puzzle[static_cast<size_t>(sym_idx)] : 0;
            
            out_puzzle[static_cast<size_t>(idx)] = 0;
            if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = 0;

            // Odrzucenie: brak unikalności (wielokrotne rozwiązania)
            const int solutions = uniq.count_solutions_limit2(out_puzzle, topo, budget);
            if (solutions < 0) { // Timeout w DLX
                out_puzzle[static_cast<size_t>(idx)] = old_a;
                if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                if (stats != nullptr) ++stats->rejected_logic_timeout;
                return false;
            }
            if (solutions != 1) { // Strata unikalności -> kara dla węzła
                out_puzzle[static_cast<size_t>(idx)] = old_a;
                if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                
                sc.update(idx, -6.0); // Mocna kara za popsucie planszy
                sc.disable(idx);
                if (remove_pair) {
                    sc.update(sym_idx, -6.0);
                    sc.disable(sym_idx);
                }
                
                ++fail_streak;
                if (stats != nullptr) ++stats->rejected_uniqueness;
                continue;
            }

            // Faza 2: Ocena stanu Basic (Poziomy 1-5)
            const GenericLogicCertifyResult basic = logic.certify_up_to_level(out_puzzle, topo, basic_level, budget, false);
            if (basic.timed_out) {
                out_puzzle[static_cast<size_t>(idx)] = old_a;
                if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                if (stats != nullptr) ++stats->rejected_logic_timeout;
                return false;
            }

            const bool basic_solved = basic.solved;
            
            // Faza 3: Wyznaczanie Nagrody (Reward Function)
            // Jeśli basic logic potrafi to rozwiązać, to nagroda jest mała (exploit).
            // Jeśli utknie, to nagroda jest gigantyczna, bo stworzyliśmy BOTTLENECK.
            double reward = basic_solved 
                ? (1.0 + 0.002 * static_cast<double>(std::max(0, basic.steps))) 
                : (18.0 + 0.005 * static_cast<double>(std::max(0, basic.steps)));

            int p7_hits = 0;
            int p8_hits = 0;
            int required_hits = 0;
            bool advanced_signal = false;
            bool stopping_signal = !basic_solved;

            // Faza 4: Certyfikacja zaawansowana (Opcjonalne dla wyższych poziomów MCTS Tuning)
            const bool do_advanced_eval = 
                tuning.enabled && 
                ((!basic_solved) || 
                 ((clues - removal) <= (target_clues + tuning.near_window)) || 
                 ((iter % tuning.eval_stride) == 0));

            if (do_advanced_eval) {
                const GenericLogicCertifyResult adv = logic.certify_up_to_level(out_puzzle, topo, advanced_level, budget, false);
                if (adv.timed_out) {
                    out_puzzle[static_cast<size_t>(idx)] = old_a;
                    if (remove_pair) out_puzzle[static_cast<size_t>(sym_idx)] = old_b;
                    if (stats != nullptr) ++stats->rejected_logic_timeout;
                    return false;
                }

                // Analiza wyników P7 (Sloty Medusa3D do KrakenFish / ALSAIC)
                for (size_t slot = GenericLogicCertify::SlotMedusa3D; slot <= GenericLogicCertify::SlotALSAIC; ++slot) {
                    p7_hits += static_cast<int>(adv.strategy_stats[slot].hit_count);
                }
                
                // Analiza wyników P8 (Sloty MSLS do DynamicForcingChains)
                for (size_t slot = GenericLogicCertify::SlotMSLS; slot <= GenericLogicCertify::SlotDynamicForcingChains; ++slot) {
                    p8_hits += static_cast<int>(adv.strategy_stats[slot].hit_count);
                }

                if (has_required_slot) {
                    required_hits = static_cast<int>(adv.strategy_stats[required_slot].hit_count);
                }

                // Kalkulacja zysku (Backpropagation value)
                reward += tuning.p7_hit_weight * static_cast<double>(p7_hits);
                reward += tuning.p8_hit_weight * static_cast<double>(p8_hits);
                reward += tuning.required_hit_weight * static_cast<double>(required_hits);
                
                if (wants_p8 && p8_hits == 0 && required_hits == 0) {
                    reward -= tuning.p8_miss_penalty;
                }
                
                reward = std::max(tuning.min_reward, reward);
                advanced_signal = (p7_hits + p8_hits + required_hits) > 0;
                
                if (tuning.require_p8_signal_for_stop) {
                    stopping_signal = (required_hits > 0 || p8_hits > 0 || (!basic_solved && p7_hits > 0));
                } else {
                    stopping_signal = (!basic_solved || advanced_signal);
                }

                if (stats != nullptr) {
                    ++stats->advanced_evals;
                    stats->advanced_p7_hits += p7_hits;
                    stats->advanced_p8_hits += p8_hits;
                    stats->required_strategy_hits += required_hits;
                }
            }

            // Aktualizacja węzła MCTS
            sc.update(idx, reward);
            if (remove_pair) {
                sc.update(sym_idx, reward);
            }
            
            // Zatwierdzenie modyfikacji
            clues -= removal;
            fail_streak = 0;
            
            if (stats != nullptr) {
                stats->accepted_removals += removal;
                if (stopping_signal) {
                    stats->bottleneck_hit = true;
                }
            }

            // Osiągnięcie celu - wcześniejsze wyjście dla optymalizacji czasowej
            if (stopping_signal && clues <= max_clues && clues >= min_clues) {
                break;
            }
        }

        out_clues = clues;
        return true;
    }
};

} // namespace sudoku_hpc::mcts_digger
