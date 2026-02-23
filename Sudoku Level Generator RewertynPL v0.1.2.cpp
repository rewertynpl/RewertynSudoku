
//Author copyright Marcin Matysek (Rewertyn)

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <functional>
#include <exception>
#include <memory>
#include <mutex>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <ctime>
#include <vector>
#include <cstdio>
#include <cwctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <psapi.h>
#include <io.h>
#include <conio.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#ifdef _MSC_VER
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#endif
#endif

// Move generic implementations to board.h
#include "Sources/strategieSource/strategie_types.h"
#include "Sources/config.h"
#include "Sources/utils.h"
#include "Sources/board.h"
#include "Sources/kernels.h"
#include "Sources/logic_certify.h"
#include "Sources/dlx.h"
#include "Sources/benchmark.h"
#include "Sources/profiler.h"
#include "Sources/monitor.h"
#include "Sources/generator.h"
#include "Sources/generator_main.h"
#include "Sources/benchmark_profiles.h"
#include "Sources/gui.h"

// StrategieSource (zaawansowane strategie) są kompilowane osobno w testach/regresji.
// Rdzeń GUI/generatora korzysta wyłącznie z generycznego solvera logiki.

// Include test regression for --run-regression-tests support
#include "Sources/TESTS_cpp/test_regression.cpp"

namespace sudoku_hpc {
    extern int run_gui_winapi(HINSTANCE hinst);
    
    // Funkcje pomocnicze dla main
    inline void run_benchmark_mode(const GenerateRunConfig& cfg) {
        log_info("benchmark", "Starting benchmark mode");
        std::cout << "Running benchmark profiles...\n";
        
        const auto report = run_benchmark_profiles_40s(cfg);
        
        // Save report to file
        std::ofstream out(cfg.benchmark_output_file);
        if (out) {
            out << report.to_text();
            out.close();
            std::cout << "Report saved to: " << cfg.benchmark_output_file << "\n";
        }
        
        // Also print to console
        std::cout << "\n" << report.to_text() << "\n";
    }
    
    inline void handle_result(const GenerateRunResult& result, const GenerateRunConfig& cfg) {
        std::cout << "\n=== Generation Summary ===\n";
        std::cout << "Accepted: " << result.accepted << "\n";
        std::cout << "Written: " << result.written << "\n";
        std::cout << "Attempts: " << result.attempts << "\n";
        std::cout << "Rejected: " << result.rejected << "\n";
        std::cout << "  - Prefilter: " << result.reject_prefilter << "\n";
        std::cout << "  - Logic: " << result.reject_logic << "\n";
        std::cout << "  - Uniqueness: " << result.reject_uniqueness << "\n";
        std::cout << "  - Strategy: " << result.reject_strategy << "\n";
        std::cout << "  - Replay: " << result.reject_replay << "\n";
        std::cout << "  - DistributionBias: " << result.reject_distribution_bias << "\n";
        std::cout << "  - UniquenessBudget: " << result.reject_uniqueness_budget << "\n";
        std::cout << "Uniqueness calls: " << result.uniqueness_calls << "\n";
        std::cout << "Uniqueness nodes: " << result.uniqueness_nodes << "\n";
        std::cout << "Uniqueness total: " << std::fixed << std::setprecision(3) << result.uniqueness_elapsed_ms << " ms\n";
        std::cout << "Uniqueness avg: " << std::fixed << std::setprecision(3) << result.uniqueness_avg_ms << " ms/call\n";
        std::cout << "CPU backend: " << result.cpu_backend_selected << "\n";
        std::cout << "Kernel time: " << std::fixed << std::setprecision(3) << result.kernel_time_ms << " ms\n";
        std::cout << "Kernel calls: " << result.kernel_calls << "\n";
        std::cout << "Backend efficiency score: " << std::fixed << std::setprecision(3) << result.backend_efficiency_score << "\n";
        std::cout << "Asymmetry efficiency index: " << std::fixed << std::setprecision(3) << result.asymmetry_efficiency_index << "\n";
        std::cout << "Logic steps total: " << result.logic_steps_total << "\n";
        std::cout << "Naked hit/use: " << result.strategy_naked_hit << "/" << result.strategy_naked_use << "\n";
        std::cout << "Hidden hit/use: " << result.strategy_hidden_hit << "/" << result.strategy_hidden_use << "\n";
        std::cout << "VIP score: " << std::fixed << std::setprecision(3) << result.vip_score << "\n";
        std::cout << "VIP grade: " << result.vip_grade << "\n";
        std::cout << "VIP contract: " << (result.vip_contract_ok ? "ok" : "fail") << "\n";
        std::cout << "VIP contract reason: " << result.vip_contract_fail_reason << "\n";
        std::cout << "Premium signature: " << result.premium_signature << "\n";
        std::cout << "Premium signature v2: " << result.premium_signature_v2 << "\n";
        std::cout << "Time: " << std::fixed << std::setprecision(2) << result.elapsed_s << "s\n";
        std::cout << "Rate: " << std::fixed << std::setprecision(2) << result.accepted_per_sec << " puzzles/s\n";
        
        if (cfg.pause_on_exit_windows) {
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
        }
    }

    inline void print_production_help(std::ostream& out) {
        out << "Sudoku Generator (production)\n";
        out << "Usage:\n";
        out << "  sudoku_gen.exe [options]\n\n";
        out << "Common options:\n";
        out << "  --help, -h                      Show this help\n";
        out << "  --box-rows <int>                Box rows\n";
        out << "  --box-cols <int>                Box cols\n";
        out << "  --difficulty <1..9>             Difficulty level\n";
        out << "  --required-strategy <name>      none|nakedsingle|hiddensingle|backtracking\n";
        out << "  --target <uint64>               Target puzzles to generate\n";
        out << "  --threads <int>                 Worker threads (0=auto)\n";
        out << "  --seed <int64>                  RNG seed (0=random)\n";
        out << "  --output-folder <path>          Output directory\n";
        out << "  --output-file <name>            Output batch file name\n";
        out << "  --single-file-only              Disable per-puzzle files\n";
        out << "  --time-limit-s <uint64>         Runtime budget (legacy alias)\n";
        out << "  --max-total-time-s <uint64>     Global runtime timeout (0=none)\n";
        out << "  --list-geometries               Print supported geometries\n";
        out << "  --validate-geometry             Validate current --box-rows/--box-cols\n";
        out << "  --validate-geometry-catalog     Validate full geometry catalog\n";
        out << "  --run-regression-tests          Run regression tests and exit\n";
        out << "  --gui                           Force GUI mode (Windows)\n";
        out << "  --cli                           Force CLI mode (Windows)\n\n";
        out << "Examples:\n";
        out << "  sudoku_gen.exe --box-rows 3 --box-cols 3 --difficulty 1 --target 100 --threads 16\n";
        out << "  sudoku_gen.exe --list-geometries\n";
        out << "  sudoku_gen.exe --validate-geometry --box-rows 4 --box-cols 3\n";
    }

#ifdef _WIN32
    inline bool can_use_cli_hotkeys() {
        return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
    }

    inline const char* cli_state_label(bool cancel_requested, bool paused) {
        if (cancel_requested) {
            return "cancel_requested";
        }
        if (paused) {
            return "paused";
        }
        return "running";
    }

    inline void print_cli_hotkeys_help() {
        std::cout << "CLI controls: [P] pause/resume, [C] cancel, [Q] cancel\n";
    }

    inline std::jthread start_cli_hotkeys_thread(std::atomic<bool>& cancel_flag, std::atomic<bool>& pause_flag) {
        return std::jthread([&cancel_flag, &pause_flag](std::stop_token st) {
            while (!st.stop_requested()) {
                if (_kbhit() == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }
                int ch = _getch();
                if (ch == 0 || ch == 224) {
                    (void)_getch();  // consume extended key code
                    continue;
                }
                const char key = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (key == 'p') {
                    const bool now_paused = !pause_flag.load(std::memory_order_relaxed);
                    pause_flag.store(now_paused, std::memory_order_relaxed);
                    std::cout << "\n[CLI] " << (now_paused ? "paused" : "resumed") << "\n" << std::flush;
                    log_info("main.cli_hotkeys", now_paused ? "paused" : "resumed");
                } else if (key == 'c' || key == 'q') {
                    pause_flag.store(false, std::memory_order_relaxed);
                    cancel_flag.store(true, std::memory_order_relaxed);
                    std::cout << "\n[CLI] cancel requested\n" << std::flush;
                    log_info("main.cli_hotkeys", "cancel requested");
                } else if (key == 'h' || key == '?') {
                    print_cli_hotkeys_help();
                }
            }
        });
    }

    inline std::jthread start_cli_status_thread(std::atomic<bool>& cancel_flag, std::atomic<bool>& pause_flag) {
        return std::jthread([&cancel_flag, &pause_flag](std::stop_token st) {
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if (st.stop_requested()) {
                    break;
                }
                const bool cancel_requested = cancel_flag.load(std::memory_order_relaxed);
                const bool paused = pause_flag.load(std::memory_order_relaxed);
                std::cout << "[CLI] state=" << cli_state_label(cancel_requested, paused) << "\n" << std::flush;
            }
        });
    }
#endif
}

int main(int argc, char** argv) {
    using namespace sudoku_hpc;
    log_info("main", "program start");
    std::cout << "Debug log file: " << debug_logger().path() << "\n";

    if (has_arg(argc, argv, "--help") || has_arg(argc, argv, "-h") || has_arg(argc, argv, "/?")) {
        print_production_help(std::cout);
        return 0;
    }

#ifdef _WIN32
    const bool force_gui = has_arg(argc, argv, "--gui");
    const bool force_cli = has_arg(argc, argv, "--cli");
    const bool force_console = has_arg(argc, argv, "--force-console");
    if ((argc == 1 || force_gui) && !force_cli) {
        log_info("main", "starting GUI mode");
        if (force_console) {
            ensure_console_attached();
        }
        return run_gui_winapi(GetModuleHandleW(nullptr));
    }
#endif

    ParseArgsResult parse_result = parse_args(argc, argv);
    GenerateRunConfig cfg = parse_result.cfg;
    if (parse_result.list_geometries) {
        std::cout << supported_geometries_text();
        return 0;
    }
    if (parse_result.validate_geometry) {
        const bool ok = print_geometry_validation(cfg.box_rows, cfg.box_cols, std::cout);
        return ok ? 0 : 1;
    }
    if (parse_result.validate_geometry_catalog) {
        const bool ok = print_geometry_catalog_validation(std::cout);
        return ok ? 0 : 1;
    }
    if (cfg.stage_start) {
        return run_stage_start_cli(cfg);
    }
    if (cfg.stage_end) {
        return run_stage_end_cli(cfg);
    }
    if (cfg.perf_ab_suite) {
        return run_perf_ab_suite_cli(cfg);
    }
    if (parse_result.run_geometry_gate) {
        return run_geometry_gate_cli(parse_result.geometry_gate_report, &cfg);
    }
    if (parse_result.run_quality_benchmark) {
        return run_quality_benchmark_cli(
            parse_result.quality_benchmark_report,
            parse_result.quality_benchmark_max_cases,
            &cfg);
    }
    if (parse_result.run_pre_difficulty_gate) {
        return run_pre_difficulty_gate_cli(
            parse_result.pre_difficulty_gate_report,
            cfg,
            parse_result.quality_benchmark_max_cases);
    }
    if (parse_result.run_asym_pair_benchmark) {
        return run_asym_pair_benchmark_cli(
            parse_result.asym_pair_benchmark_report,
            parse_result.quality_benchmark_max_cases,
            &cfg);
    }
    if (parse_result.run_vip_benchmark) {
        return run_vip_benchmark_cli(
            parse_result.vip_benchmark_report,
            cfg,
            parse_result.quality_benchmark_max_cases);
    }
    if (parse_result.run_vip_gate) {
        return run_vip_gate_cli(
            parse_result.vip_gate_report,
            cfg,
            parse_result.quality_benchmark_max_cases);
    }
    if (parse_result.explain_profile) {
        std::cout << explain_generation_profile_text(cfg);
        return 0;
    }
    if (parse_result.benchmark_mode) {
        run_benchmark_mode(cfg);
        return 0;
    }
    
    // Default Run
    std::atomic<bool> cancel_flag{false};
    std::atomic<bool> pause_flag{false};
#ifdef _WIN32
    std::jthread cli_hotkeys_thread;
    std::jthread cli_status_thread;
    if (can_use_cli_hotkeys()) {
        print_cli_hotkeys_help();
        cli_hotkeys_thread = start_cli_hotkeys_thread(cancel_flag, pause_flag);
        cli_status_thread = start_cli_status_thread(cancel_flag, pause_flag);
    }
#endif
    ConsoleStatsMonitor monitor;
    monitor.start_ui_thread(5000);
    auto result = run_generic_sudoku(cfg, &monitor, &cancel_flag, &pause_flag, nullptr, nullptr);
    monitor.stop_ui_thread();
#ifdef _WIN32
    if (cli_hotkeys_thread.joinable()) {
        cli_hotkeys_thread.request_stop();
        cli_hotkeys_thread.join();
    }
    if (cli_status_thread.joinable()) {
        cli_status_thread.request_stop();
        cli_status_thread.join();
    }
#endif
    handle_result(result, cfg);
    return 0;
}
