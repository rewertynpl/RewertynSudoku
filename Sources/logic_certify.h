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

#include "geometry.h"
#include "strategieSource/strategylevels1_runtime.h"

namespace sudoku_hpc {

enum class RuntimeStrategyId : uint8_t {
    NakedSingle = 0,
    HiddenSingle = 1,
    PointingPairs = 2,
    BoxLineReduction = 3,
    XWing = 4,
    Swordfish = 5,
    Chain = 6,
    Medusa = 7
};

struct RuntimeStrategyMeta {
    RuntimeStrategyId id{};
    const char* name = "";
    int level = 1;
    bool runtime_ready = false;
};

inline const std::vector<RuntimeStrategyMeta>& runtime_strategy_registry() {
    static const std::vector<RuntimeStrategyMeta> registry = {
        {RuntimeStrategyId::NakedSingle, "NakedSingle", 1, true},
        {RuntimeStrategyId::HiddenSingle, "HiddenSingle", 1, true},
        {RuntimeStrategyId::PointingPairs, "PointingPairs", 2, false},
        {RuntimeStrategyId::BoxLineReduction, "BoxLineReduction", 2, false},
        {RuntimeStrategyId::XWing, "XWing", 3, false},
        {RuntimeStrategyId::Swordfish, "Swordfish", 4, false},
        {RuntimeStrategyId::Chain, "Chain", 5, false},
        {RuntimeStrategyId::Medusa, "Medusa", 6, false},
    };
    return registry;
}

struct StrategyStats {
    uint64_t use_count = 0;
    uint64_t hit_count = 0;
    uint64_t placements = 0;
    uint64_t elapsed_ns = 0;
};

struct GenericLogicCertifyResult {
    bool solved = false;
    bool timed_out = false;
    bool used_naked_single = false;
    bool used_hidden_single = false;
    bool naked_single_scanned = false;
    bool hidden_single_scanned = false;
    int steps = 0;
    std::vector<uint16_t> solved_grid;
    std::array<StrategyStats, 2> strategy_stats{};
};

struct GenericLogicCertify {
    enum class ApplyResult : uint8_t {
        NoProgress = 0,
        Progress = 1,
        Contradiction = 2
    };

    static ApplyResult apply_naked_single(GenericBoard& board, StrategyStats& stats) {
        const auto r = sudoku_strategie::level1_runtime::apply_naked_single(board, stats);
        switch (r) {
        case sudoku_strategie::level1_runtime::ApplyResult::Progress:
            return ApplyResult::Progress;
        case sudoku_strategie::level1_runtime::ApplyResult::Contradiction:
            return ApplyResult::Contradiction;
        case sudoku_strategie::level1_runtime::ApplyResult::NoProgress:
        default:
            return ApplyResult::NoProgress;
        }
    }

    static ApplyResult apply_hidden_single(GenericBoard& board, const GenericTopology& topo, StrategyStats& stats) {
        const auto r = sudoku_strategie::level1_runtime::apply_hidden_single(board, topo, stats);
        switch (r) {
        case sudoku_strategie::level1_runtime::ApplyResult::Progress:
            return ApplyResult::Progress;
        case sudoku_strategie::level1_runtime::ApplyResult::Contradiction:
            return ApplyResult::Contradiction;
        case sudoku_strategie::level1_runtime::ApplyResult::NoProgress:
        default:
            return ApplyResult::NoProgress;
        }
    }

    GenericLogicCertifyResult certify(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        GenericLogicCertifyResult result{};
        const bool has_budget = (budget != nullptr);
        StrategyStats& naked_stats = result.strategy_stats[0];
        StrategyStats& hidden_stats = result.strategy_stats[1];

        static thread_local GenericBoard board;
        board.topo = &topo;
        if (!board.init_from_puzzle(puzzle, false)) {
            result.solved = false;
            return result;
        }

        while (board.empty_cells != 0) {
            if (has_budget && !budget->step()) {
                result.timed_out = true;
                result.solved = false;
                return result;
            }
            const ApplyResult naked = apply_naked_single(board, naked_stats);
            if (naked == ApplyResult::Contradiction) {
                result.solved = false;
                return result;
            }
            if (naked == ApplyResult::Progress) {
                ++result.steps;
                continue;
            }

            const ApplyResult hidden = apply_hidden_single(board, topo, hidden_stats);
            if (hidden == ApplyResult::Contradiction) {
                result.solved = false;
                return result;
            }
            if (hidden == ApplyResult::Progress) {
                ++result.steps;
                continue;
            }
            break;
        }

        result.solved = (board.empty_cells == 0);
        if (capture_solution_grid) {
            result.solved_grid = board.values;
        }
        result.naked_single_scanned = naked_stats.use_count > 0;
        result.hidden_single_scanned = hidden_stats.use_count > 0;
        result.used_naked_single = naked_stats.hit_count > 0;
        result.used_hidden_single = hidden_stats.hit_count > 0;
        return result;
    }
};

} // namespace sudoku_hpc
