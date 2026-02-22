#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

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
#include "../generator.h"

namespace sudoku_testy {
inline void run_all_regression_tests(const std::string& /*report_path*/) {}
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            std::cout << "Usage:\n";
            std::cout << "  pause_cancel_control_smoke.exe\n\n";
            std::cout << "Options:\n";
            std::cout << "  --help, -h, /?                Show this help\n\n";
            std::cout << "Behavior:\n";
            std::cout << "  Runs a smoke test for pause/cancel control on 36x36 generation.\n";
            std::cout << "  Exits non-zero if cancel path does not stop quickly.\n";
            return 0;
        }
    }

    using namespace sudoku_hpc;

    GenerateRunConfig cfg;
    cfg.box_rows = 6;  // 36x36
    cfg.box_cols = 6;
    cfg.target_puzzles = 1;
    cfg.difficulty_level_required = 1;
    cfg.required_strategy = RequiredStrategy::NakedSingle;
    const ClueRange auto_clues =
        resolve_auto_clue_range(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy);
    cfg.min_clues = auto_clues.min_clues;
    cfg.max_clues = auto_clues.max_clues;
    cfg.threads = 1;
    cfg.pause_on_exit_windows = false;
    cfg.output_folder = "generated_sudoku_files";
    cfg.output_file = "test_pause_cancel_runtime.txt";

    std::atomic<bool> cancel_flag{false};
    std::atomic<bool> pause_flag{false};

    std::jthread ctl([&](std::stop_token st) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (st.stop_requested()) return;
        pause_flag.store(true, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        pause_flag.store(false, std::memory_order_relaxed);
        cancel_flag.store(true, std::memory_order_relaxed);
    });

    const auto t0 = std::chrono::steady_clock::now();
    ConsoleStatsMonitor monitor;
    const auto result = run_generic_sudoku(cfg, &monitor, &cancel_flag, &pause_flag, nullptr, nullptr);
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed_s = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "elapsed_s=" << elapsed_s
              << " accepted=" << result.accepted
              << " attempts=" << result.attempts
              << " rejected=" << result.rejected << "\n";

    if (elapsed_s > 8.0) {
        std::cerr << "FAIL: pause/cancel control did not stop quickly\n";
        return 2;
    }
    return 0;
}
