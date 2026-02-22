//Author copyright Marcin Matysek (Rewertyn)

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

inline std::string grid_size_label(int box_rows, int box_cols) {
    const int n = std::max(1, box_rows) * std::max(1, box_cols);
    return std::to_string(n) + "x" + std::to_string(n);
}

inline BenchmarkReportData run_benchmark_profiles_40s(const GenerateRunConfig& base_cfg) {
    BenchmarkReportData data;
    data.title = "Porownanie strategii Sudoku (poziomy 1-8)";
    data.probe_per_level = std::to_string(base_cfg.benchmark_seconds_per_profile) + "s";
    data.benchmark_mode = "profiles_generic";
    data.cpu_model = detect_cpu_model();
    data.ram_info = detect_ram_info();
    data.os_info = detect_os_info();
    data.runtime_info = detect_runtime_info();

    const int thread_count = base_cfg.threads > 0 ? base_cfg.threads : std::max(1u, std::thread::hardware_concurrency());
    data.threads_info = std::to_string(thread_count);

    const auto total_start = std::chrono::steady_clock::now();

    for (int lvl = 1; lvl <= 8; ++lvl) {
        GenerateRunConfig cfg = base_cfg;
        cfg.difficulty_level_required = lvl;
        cfg.required_strategy = default_required_strategy_for_level(lvl);
        cfg.target_puzzles = std::numeric_limits<uint64_t>::max() / 2;
        cfg.max_attempts_s = static_cast<uint64_t>(std::max(1, base_cfg.benchmark_seconds_per_profile));
        cfg.pause_on_exit_windows = false;
        cfg.write_individual_files = false;
        cfg.output_folder = base_cfg.output_folder;
        cfg.output_file = "bench_lvl_" + std::to_string(lvl) + "_" + grid_size_label(cfg.box_rows, cfg.box_cols) + ".txt";

        GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, {}, {});

        BenchmarkTableARow row_a;
        row_a.lvl = lvl;
        row_a.solved_ok = static_cast<int>(result.accepted);
        row_a.analyzed = static_cast<int>(result.attempts);
        row_a.required_use = result.analyzed_required_strategy;
        row_a.required_hit = result.required_strategy_hits;
        row_a.reject_strategy = result.reject_strategy;
        row_a.avg_solved_gen_ms = result.accepted > 0 ? (result.elapsed_s * 1000.0) / static_cast<double>(result.accepted) : 0.0;
        row_a.avg_dig_ms = 0.0;
        row_a.avg_analyze_ms = result.attempts > 0 ? (result.elapsed_s * 1000.0) / static_cast<double>(result.attempts) : 0.0;
        row_a.backtracks = result.reject_logic;
        row_a.timeouts = result.elapsed_s + 0.05 >= static_cast<double>(cfg.max_attempts_s) ? 1 : 0;
        row_a.success_rate = result.attempts > 0 ? (100.0 * static_cast<double>(result.accepted) / static_cast<double>(result.attempts)) : 0.0;
        data.table_a.push_back(row_a);

        BenchmarkTableA2Row row_a2;
        row_a2.lvl = lvl;
        row_a2.analyzed = static_cast<int>(result.attempts);
        row_a2.medusa_hit = 0;
        row_a2.medusa_use = result.attempts;
        row_a2.sue_hit = 0;
        row_a2.sue_use = result.attempts;
        row_a2.msls_hit = 0;
        row_a2.msls_use = result.attempts;
        data.table_a2.push_back(row_a2);

        BenchmarkTableCRow row_c;
        row_c.size = grid_size_label(cfg.box_rows, cfg.box_cols);
        row_c.lvl = lvl;
        row_c.est_analyze_s = result.attempts > 0 ? (result.elapsed_s / static_cast<double>(result.attempts)) : 0.0;
        row_c.budget_s = static_cast<double>(cfg.max_attempts_s);
        row_c.peak_ram_mb = process_peak_ram_mb();
        row_c.decision = "RUN";
        data.table_c.push_back(row_c);
    }

    const std::array<RequiredStrategy, 3> strategy_tests = {
        RequiredStrategy::NakedSingle,
        RequiredStrategy::HiddenSingle,
        RequiredStrategy::Backtracking,
    };
    for (RequiredStrategy strategy : strategy_tests) {
        GenerateRunConfig cfg = base_cfg;
        cfg.required_strategy = strategy;
        cfg.difficulty_level_required = (strategy == RequiredStrategy::Backtracking) ? 9 : 1;
        cfg.target_puzzles = std::numeric_limits<uint64_t>::max() / 2;
        cfg.max_attempts_s = static_cast<uint64_t>(std::max(1, base_cfg.benchmark_seconds_per_profile));
        cfg.pause_on_exit_windows = false;
        cfg.write_individual_files = false;
        cfg.output_folder = base_cfg.output_folder;
        cfg.output_file = "bench_required_" + to_string(strategy) + "_" + grid_size_label(cfg.box_rows, cfg.box_cols) + ".txt";

        GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, {}, {});

        BenchmarkTableA3Row row_a3;
        row_a3.strategy = to_string(strategy);
        row_a3.lvl = cfg.difficulty_level_required;
        row_a3.max_attempts = cfg.max_attempts;
        row_a3.analyzed = result.analyzed_required_strategy;
        row_a3.required_strategy_hits = result.required_strategy_hits;
        row_a3.analyzed_per_s = result.elapsed_s > 0.0 ? static_cast<double>(row_a3.analyzed) / result.elapsed_s : 0.0;
        row_a3.est_5min = static_cast<uint64_t>(row_a3.analyzed_per_s * 300.0);
        row_a3.written = result.written_required_strategy;
        data.table_a3.push_back(row_a3);
    }

    BenchmarkTableBRow size_row;
    size_row.size = grid_size_label(base_cfg.box_rows, base_cfg.box_cols);
    for (int lvl = 1; lvl <= 8; ++lvl) {
        const ClueRange cr = clue_range_for_size_level(std::max(1, base_cfg.box_rows) * std::max(1, base_cfg.box_cols), lvl);
        size_row.levels[static_cast<size_t>(lvl - 1)] = std::to_string(cr.min_clues) + "-" + std::to_string(cr.max_clues);
    }
    data.table_b.push_back(size_row);

    data.rules = {
        "Profile uruchamiane na staly budzet czasowy per poziom (RUN).",
        "Unikalnosc: generic DLX limit=2.",
        "Kontrakt required_strategy: use=analyzed_required_strategy, hit=required_strategy_hits, reject_strategy gdy !(use&&hit).",
    };
    data.total_execution_s = static_cast<uint64_t>(std::llround(
        std::chrono::duration<double>(std::chrono::steady_clock::now() - total_start).count()));
    return data;
}

} // namespace sudoku_hpc
