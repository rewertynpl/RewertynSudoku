// ============================================================================
// SUDOKU HPC - TEST PRODUKCYJNY L8 REQUIRED STRATEGY
// Domyslnie: 9x9 (3x3), 12x12 (3x4), po 2 plansze na strategie L8.
// Zatrzymanie globalne: timeout-min (domyslnie 20 min), z raportem czesciowym.
// ============================================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

#include "../strategieSource/strategie_types.h"
#include "../config.h"
#include "../utils.h"
#include "../board.h"
#include "../kernels.h"
#include "../logic_certify.h"
#include "../dlx.h"
#include "../benchmark.h"
#include "../profiler.h"
#include "../monitor.h"
#include "../geometry.h"
#include "../generator.h"

namespace sudoku_testy {
inline void run_all_regression_tests(const std::string& /*report_path*/) {}
}

namespace {
using namespace sudoku_hpc;

// Debug logs zostawione do szybkiej diagnostyki crashy L8.
// Ustaw na true tylko gdy potrzebna szczegolowa diagnostyka krok po kroku.
constexpr bool kL8VerboseLogs = false;

struct L8CaseResult {
    int box_rows = 0;
    int box_cols = 0;
    RequiredStrategy strategy = RequiredStrategy::None;
    bool attempted = false;
    bool success = false;
    bool timed_out_case = false;
    uint64_t accepted = 0;
    uint64_t written = 0;
    uint64_t attempts = 0;
    uint64_t reject_strategy = 0;
    double elapsed_s = 0.0;
};

std::string fmt2(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

void print_help() {
    std::cout << "Usage:\n";
    std::cout << "  sudoku_test_level8_required.exe [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --timeout-min <int>             Global timeout in minutes (default 20, 0=off)\n";
    std::cout << "  --case-timeout-s <int>          Per-case max_total_time_s (default 180, 0=off)\n";
    std::cout << "  --attempt-time-budget-s <num>   Per-attempt budget (default 15.0)\n";
    std::cout << "  --max-attempts <u64>            Limit attempts per case (default 0)\n";
    std::cout << "  --target <u64>                  Target puzzles per case (default 2)\n";
    std::cout << "  --report <path>                 Report path (default TESTS_cpp/level8_required_report.txt)\n";
    std::cout << "  --only-strategy <name>          Run only one required strategy (debug)\n";
    std::cout << "  --help                          Show this help\n";
}

std::vector<std::pair<int, int>> default_geometries() {
    return {
        {3, 3},  // 9x9
        {3, 4},  // 12x12
    };
}

std::vector<RequiredStrategy> l8_strategies() {
    return {
        RequiredStrategy::MSLS,
        RequiredStrategy::Exocet,
        RequiredStrategy::SeniorExocet,
        RequiredStrategy::SKLoop,
        RequiredStrategy::PatternOverlayMethod,
        RequiredStrategy::ForcingChains,
    };
}

}  // namespace

int main(int argc, char** argv) {
    int timeout_min = 20;
    int case_timeout_s = 180;
    double attempt_time_budget_s = 15.0;
    uint64_t max_attempts = 0;
    uint64_t target = 2;
    std::string report_path = "TESTS_cpp/level8_required_report.txt";
    std::string only_strategy;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](int k) { return (i + k) < argc; };
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            print_help();
            return 0;
        } else if (arg == "--timeout-min" && need(1)) {
            timeout_min = std::max(0, std::stoi(std::string(argv[++i])));
        } else if (arg == "--case-timeout-s" && need(1)) {
            case_timeout_s = std::max(0, std::stoi(std::string(argv[++i])));
        } else if (arg == "--attempt-time-budget-s" && need(1)) {
            attempt_time_budget_s = std::max(0.0, std::stod(std::string(argv[++i])));
        } else if (arg == "--max-attempts" && need(1)) {
            max_attempts = static_cast<uint64_t>(std::stoull(std::string(argv[++i])));
        } else if (arg == "--target" && need(1)) {
            target = std::max<uint64_t>(1, static_cast<uint64_t>(std::stoull(std::string(argv[++i]))));
        } else if (arg == "--report" && need(1)) {
            report_path = argv[++i];
        } else if (arg == "--only-strategy" && need(1)) {
            only_strategy = argv[++i];
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_path).parent_path(), ec);
    std::ofstream report(report_path, std::ios::out | std::ios::trunc);
    if (!report) {
        std::cerr << "Cannot open report: " << report_path << "\n";
        return 2;
    }
    report << std::unitbuf;

    const std::vector<std::pair<int, int>> geos = default_geometries();
    std::vector<RequiredStrategy> strategies = l8_strategies();
    if (!only_strategy.empty()) {
        const RequiredStrategy parsed = parse_required_strategy(only_strategy);
        if (parsed == RequiredStrategy::None) {
            std::cerr << "Unknown --only-strategy: " << only_strategy << "\n";
            return 3;
        }
        strategies = {parsed};
    }

    std::vector<L8CaseResult> results;
    results.reserve(geos.size() * strategies.size());

    const auto t0 = std::chrono::steady_clock::now();
    const bool global_timeout_enabled = timeout_min > 0;
    const auto deadline = global_timeout_enabled
        ? (t0 + std::chrono::minutes(timeout_min))
        : std::chrono::steady_clock::time_point::max();

    report << "LEVEL8 REQUIRED STRATEGY BENCH\n";
    report << "==============================\n";
    report << "target_per_case=" << target
           << ", timeout_min=" << timeout_min
           << ", case_timeout_s=" << case_timeout_s
           << ", attempt_time_budget_s=" << fmt2(attempt_time_budget_s)
           << ", max_attempts=" << max_attempts << "\n\n";

    bool global_timeout_hit = false;
    for (const auto& g : geos) {
        const int box_rows = g.first;
        const int box_cols = g.second;
        const int n = box_rows * box_cols;
        report << "[Geometry " << n << "x" << n << " (" << box_rows << "x" << box_cols << ")]\n";

        for (const auto strategy : strategies) {
            L8CaseResult row{};
            row.box_rows = box_rows;
            row.box_cols = box_cols;
            row.strategy = strategy;
            if (kL8VerboseLogs) {
                std::cerr << "[L8TEST] case_begin n=" << n
                          << " geom=" << box_rows << "x" << box_cols
                          << " strategy=" << to_string(strategy) << "\n";
            }

            if (global_timeout_enabled && std::chrono::steady_clock::now() >= deadline) {
                global_timeout_hit = true;
                results.push_back(row);
                report << "  - " << to_string(strategy) << ": SKIP (global timeout)\n";
                continue;
            }
            if (!required_strategy_selectable_for_geometry(strategy, box_rows, box_cols)) {
                results.push_back(row);
                report << "  - " << to_string(strategy) << ": SKIP (not selectable for geometry)\n";
                continue;
            }

            GenerateRunConfig cfg;
            cfg.box_rows = box_rows;
            cfg.box_cols = box_cols;
            cfg.target_puzzles = target;
            cfg.difficulty_level_required = 8;
            cfg.required_strategy = strategy;
            cfg.require_unique = true;
            cfg.strict_logical = true;
            cfg.threads = 0;
            cfg.seed = 0;
            cfg.reseed_interval_s = (n >= 16 ? 5 : 2);
            cfg.attempt_time_budget_s = attempt_time_budget_s;
            cfg.max_attempts = max_attempts;
            const ClueRange auto_clues = resolve_auto_clue_range(box_rows, box_cols, cfg.difficulty_level_required, cfg.required_strategy);
            cfg.min_clues = auto_clues.min_clues;
            cfg.max_clues = auto_clues.max_clues;
            cfg.pause_on_exit_windows = false;
            cfg.write_individual_files = false;
            cfg.output_folder = "plikiTMP/testy/level8_required";
            cfg.output_file = "__tmp_l8_" + std::to_string(n) + "_" + std::to_string(box_rows) + "x" +
                              std::to_string(box_cols) + "_" + to_string(strategy) + ".txt";
            cfg.max_total_time_s = 0;
            if (case_timeout_s > 0) {
                cfg.max_total_time_s = static_cast<uint64_t>(case_timeout_s);
            }
            if (global_timeout_enabled) {
                const auto now = std::chrono::steady_clock::now();
                if (now < deadline) {
                    const auto remain_s = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::seconds>(deadline - now).count());
                    if (remain_s == 0) {
                        global_timeout_hit = true;
                        results.push_back(row);
                        report << "  - " << to_string(strategy) << ": SKIP (global timeout)\n";
                        continue;
                    }
                    cfg.max_total_time_s = (cfg.max_total_time_s == 0)
                        ? remain_s
                        : std::min<uint64_t>(cfg.max_total_time_s, remain_s);
                } else {
                    global_timeout_hit = true;
                    results.push_back(row);
                    report << "  - " << to_string(strategy) << ": SKIP (global timeout)\n";
                    continue;
                }
            }

            row.attempted = true;
            if (kL8VerboseLogs) {
                std::cerr << "[L8TEST] run_begin strategy=" << to_string(strategy)
                          << " max_total_time_s=" << cfg.max_total_time_s
                          << " target=" << cfg.target_puzzles
                          << " clues=[" << cfg.min_clues << "," << cfg.max_clues << "]\n";
            }
            const auto c0 = std::chrono::steady_clock::now();
            const GenerateRunResult run = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);
            row.elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - c0).count();
            if (kL8VerboseLogs) {
                std::cerr << "[L8TEST] run_end strategy=" << to_string(strategy)
                          << " written=" << run.written
                          << " accepted=" << run.accepted
                          << " attempts=" << run.attempts << "\n";
            }
            row.accepted = run.accepted;
            row.written = run.written;
            row.attempts = run.attempts;
            row.reject_strategy = run.reject_strategy;
            row.success = run.written >= target;
            row.timed_out_case = (cfg.max_total_time_s > 0 && run.written < target);
            results.push_back(row);

            report << "  - " << std::setw(15) << std::left << to_string(strategy)
                   << " written=" << std::setw(3) << std::right << row.written
                   << "/" << target
                   << " accepted=" << std::setw(4) << row.accepted
                   << " attempts=" << std::setw(7) << row.attempts
                   << " reject_strategy=" << std::setw(7) << row.reject_strategy
                   << " elapsed_s=" << fmt2(row.elapsed_s)
                   << " status=" << (row.success ? "OK" : (row.timed_out_case ? "TIMEOUT/PARTIAL" : "PARTIAL"))
                   << "\n";
        }
        report << "\n";
        if (global_timeout_hit) {
            break;
        }
    }

    const auto total_elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    uint64_t attempted = 0;
    uint64_t ok = 0;
    for (const auto& r : results) {
        if (r.attempted) ++attempted;
        if (r.success) ++ok;
    }

    report << "SUMMARY\n";
    report << "=======\n";
    report << "cases_total=" << results.size()
           << ", attempted=" << attempted
           << ", success=" << ok
           << ", global_timeout_hit=" << (global_timeout_hit ? "1" : "0")
           << ", elapsed_total_s=" << fmt2(total_elapsed_s) << "\n";
    if (global_timeout_hit) {
        report << "Partial report saved due to global timeout.\n";
    }
    report.close();

    std::cout << "L8 required-strategy test report: " << report_path << "\n";
    std::cout << "attempted=" << attempted << ", success=" << ok
              << ", global_timeout_hit=" << (global_timeout_hit ? "1" : "0")
              << ", elapsed_s=" << fmt2(total_elapsed_s) << "\n";
    return 0;
}
