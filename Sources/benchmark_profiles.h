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
struct QualityGateResult {
    bool passed = true;
    double throughput_pct = 100.0;
    double reject_strategy_pct = 0.0;
    std::string message;
};

inline BenchmarkReportData run_benchmark_quality_gate_25s(
    const GenerateRunConfig& base_cfg,
    const BenchmarkReportData* baseline_data = nullptr) {

    BenchmarkReportData data;
    data.title = "Quality Gate Benchmark - Sudoku (poziomy 1-8)";
    data.probe_per_level = "25s";
    data.benchmark_mode = "quality_gate_25s";
    data.cpu_model = detect_cpu_model();
    data.ram_info = detect_ram_info();
    data.os_info = detect_os_info();
    data.runtime_info = detect_runtime_info();
    data.quality_gate_enabled = true;
    data.quality_gate_min_throughput_pct = 90.0;
    data.quality_gate_max_reject_strategy_pct = 20.0;

    const int thread_count = base_cfg.threads > 0 ? base_cfg.threads : std::max(1u, std::thread::hardware_concurrency());
    data.threads_info = std::to_string(thread_count);

    const auto total_start = std::chrono::steady_clock::now();

    uint64_t total_accepted = 0;
    uint64_t total_attempts = 0;
    uint64_t total_reject_strategy = 0;

    const int n = std::max(1, base_cfg.box_rows) * std::max(1, base_cfg.box_cols);
    const std::string grid_label = std::to_string(n) + "x" + std::to_string(n);

    std::cout << "\n[QUALITY GATE] size=" << grid_label << ", probe=25s/level\n";

    for (int lvl = 1; lvl <= 8; ++lvl) {
        GenerateRunConfig cfg = base_cfg;
        cfg.difficulty_level_required = lvl;
        cfg.required_strategy = default_required_strategy_for_level(lvl);
        cfg.target_puzzles = std::numeric_limits<uint64_t>::max() / 2;
        cfg.max_attempts_s = 25;
        cfg.pause_on_exit_windows = false;
        cfg.write_individual_files = false;
        cfg.output_folder = base_cfg.output_folder;
        cfg.output_file = "bench_qg_lvl_" + std::to_string(lvl) + "_" + grid_label + ".txt";

        std::cout << "[LEVEL " << lvl << "/8] Running benchmark... " << std::flush;
        const auto level_start = std::chrono::steady_clock::now();

        GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, {}, {});

        const auto level_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - level_start).count();
        (void)level_elapsed;

        total_accepted += result.accepted;
        total_attempts += result.attempts;
        total_reject_strategy += result.reject_strategy;

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

        double throughput_vs_baseline = 100.0;
        if (baseline_data != nullptr && !baseline_data->table_a.empty()) {
            const auto& baseline_row = baseline_data->table_a[static_cast<size_t>(lvl - 1)];
            if (baseline_row.solved_ok > 0) {
                throughput_vs_baseline = (100.0 * static_cast<double>(result.accepted)) / static_cast<double>(baseline_row.solved_ok);
            }
        }

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

        BenchmarkTableA3Row row_a3;
        row_a3.strategy = to_string(cfg.required_strategy);
        row_a3.lvl = lvl;
        row_a3.max_attempts = cfg.max_attempts;
        row_a3.analyzed = result.analyzed_required_strategy;
        row_a3.analyzed_per_s = result.analyzed_required_strategy > 0 ? (result.analyzed_required_strategy / result.elapsed_s) : 0.0;
        row_a3.est_5min = static_cast<uint64_t>(row_a3.analyzed_per_s * 300.0);
        row_a3.written = result.written_required_strategy;
        data.table_a3.push_back(row_a3);

        BenchmarkTableCRow row_c;
        row_c.size = grid_label;
        row_c.lvl = lvl;
        row_c.est_analyze_s = result.attempts > 0 ? (result.elapsed_s / static_cast<double>(result.attempts)) : 0.0;
        row_c.budget_s = static_cast<double>(cfg.max_attempts_s);
        row_c.peak_ram_mb = process_peak_ram_mb();
        row_c.decision = "RUN";
        data.table_c.push_back(row_c);

        std::cout << "accepted=" << result.accepted
                  << ", attempts=" << result.attempts
                  << ", throughput=" << std::fixed << std::setprecision(1) << throughput_vs_baseline << "%";
        if (throughput_vs_baseline < data.quality_gate_min_throughput_pct) {
            std::cout << " REGRESJA";
        }
        std::cout << "\n";
    }

    const double total_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - total_start).count();
    const double throughput = total_attempts > 0 ? (total_accepted / total_elapsed) : 0.0;
    const double reject_strategy_pct = total_attempts > 0 ? (100.0 * static_cast<double>(total_reject_strategy) / static_cast<double>(total_attempts)) : 0.0;

    double throughput_vs_baseline = 100.0;
    if (baseline_data != nullptr && !baseline_data->table_a.empty()) {
        uint64_t baseline_total = 0;
        for (const auto& row : baseline_data->table_a) {
            baseline_total += row.solved_ok;
        }
        if (baseline_total > 0) {
            throughput_vs_baseline = (100.0 * static_cast<double>(total_accepted)) / static_cast<double>(baseline_total);
        }
    }

    data.quality_gate_passed = true;
    std::ostringstream gate_msg;
    gate_msg << "Quality Gate: ";

    const bool throughput_ok = throughput_vs_baseline >= data.quality_gate_min_throughput_pct;
    const bool reject_ok = reject_strategy_pct <= data.quality_gate_max_reject_strategy_pct;

    if (!throughput_ok) {
        data.quality_gate_passed = false;
        gate_msg << "FAILED - Throughput " << std::fixed << std::setprecision(1) << throughput_vs_baseline
                 << "% < " << data.quality_gate_min_throughput_pct << "% (baseline)";
    }
    if (!reject_ok) {
        data.quality_gate_passed = false;
        if (!throughput_ok) gate_msg << "; ";
        else gate_msg << "FAILED - ";
        gate_msg << "Reject Strategy " << std::fixed << std::setprecision(1) << reject_strategy_pct
                 << "% > " << data.quality_gate_max_reject_strategy_pct << "%";
    }
    if (data.quality_gate_passed) {
        gate_msg << "PASSED - Throughput " << std::fixed << std::setprecision(1) << throughput_vs_baseline
                 << "%, Reject Strategy " << std::fixed << std::setprecision(1) << reject_strategy_pct << "%";
    }

    data.quality_gate_message = gate_msg.str();

    std::cout << "\n[QUALITY GATE SUMMARY]" << "\n";
    std::cout << "Total accepted: " << total_accepted << "\n";
    std::cout << "Total attempts: " << total_attempts << "\n";
    std::cout << "Total elapsed: " << std::fixed << std::setprecision(1) << total_elapsed << "s\n";
    std::cout << "Throughput: " << throughput << " puzzles/s\n";
    std::cout << "Throughput vs baseline: " << throughput_vs_baseline << "%\n";
    std::cout << "Reject Strategy: " << reject_strategy_pct << "%\n";
    std::cout << "Status: " << (data.quality_gate_passed ? "PASSED" : "FAILED") << "\n";

    data.rules = {
        "Quality Gate: profile 25s per poziom (8 poziomow).",
        "Przeglos regresji jesli: throughput < 90% baseline LUB reject_strategy > 20%.",
        "Unikalnosc: generic DLX limit=2.",
        "Kontrakt required_strategy: use=analyzed_required_strategy, hit=required_strategy_hits.",
    };

    data.total_execution_s = static_cast<uint64_t>(std::llround(total_elapsed));
    return data;
}

} // namespace sudoku_hpc
