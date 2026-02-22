// ============================================================================
// SUDOKU HPC - LEVEL 1 GENERATION CATALOG (4x4..36x36, sym+asym)
// Cel: wygenerowac po 2 plansze dla geometrii (domyslnie z globalnym limitem 5 minut).
// Kolejnosc: od najmniejszych sudoku do najwiekszych.
// Timeout: konfigurowalny globalny/per-geometria z CLI.
// Wyniki: TESTS_cpp/testowe_plansze + checklista txt z checkboxami.
// ============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
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

struct CaseStatus {
    geometria::GeometrySpec geo{};
    std::string class_name;
    std::string output_file;
    std::string file_prefix;
    bool attempted = false;
    bool success = false;
    bool case_timeout = false;
    uint64_t accepted = 0;
    uint64_t written = 0;
    uint64_t attempts = 0;
    double elapsed_s = 0.0;
    std::string note;
};

constexpr int kDefaultGlobalTimeoutMinutes = 5;

std::string build_output_file_name(const geometria::GeometrySpec& g) {
    std::ostringstream oss;
    oss << "__tmp_batch_" << g.n << "x" << g.n
        << "_" << g.box_rows << "x" << g.box_cols
        << "_" << (g.is_symmetric ? "sym" : "asym") << ".txt";
    return oss.str();
}

std::string build_case_file_prefix(const geometria::GeometrySpec& g) {
    std::ostringstream oss;
    oss << "sudoku_" << g.n << "x" << g.n
        << "_" << g.box_rows << "x" << g.box_cols
        << "_" << (g.is_symmetric ? "sym" : "asym") << "_lvl1";
    return oss.str();
}

bool is_generic_individual_file_name(const std::string& name) {
    // Matches "sudoku_000001.txt"
    if (name.size() != 17) {
        return false;
    }
    if (name.rfind("sudoku_", 0) != 0) {
        return false;
    }
    if (name.substr(13) != ".txt") {
        return false;
    }
    for (size_t i = 7; i < 13; ++i) {
        if (std::isdigit(static_cast<unsigned char>(name[i])) == 0) {
            return false;
        }
    }
    return true;
}

int generic_file_index(const std::string& name) {
    if (!is_generic_individual_file_name(name)) {
        return -1;
    }
    try {
        return std::stoi(name.substr(7, 6));
    } catch (...) {
        return -1;
    }
}

} // namespace

int main(int argc, char** argv) {
    int timeout_minutes = kDefaultGlobalTimeoutMinutes;  // 0 => no limit
    int case_timeout_s = 0;  // 0 => no limit
    int only_n = 0;  // 0 => all sizes
    double attempt_time_budget_s = 35.0;  // domyslny budzet czasu na probe

    auto print_help = []() {
        std::cout << "Usage:\n";
        std::cout << "  sudoku_test_level1_generate_catalog_5min.exe [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --help, -h, /?                Show this help\n";
        std::cout << "  --timeout-min <int>           Global timeout in minutes (0 = no limit, default = 5)\n";
        std::cout << "  --case-timeout-s <int>        Per-geometry timeout in seconds (0 = no limit)\n";
        std::cout << "  --attempt-time-budget-s <num> Per-attempt time budget in seconds (default = 35)\n";
        std::cout << "  --only-n <int>                Run only geometries with this size NxN\n\n";
        std::cout << "Behavior:\n";
        std::cout << "  - Generates up to 2 level-1 boards per geometry (4x4..36x36)\n";
        std::cout << "  - Ascending geometry order (small to large)\n";
        std::cout << "  - Writes checklist/progress and prints live progress to console\n";
        std::cout << "  - Saves individual puzzles directly in TESTS_cpp/testowe_plansze (no subfolders)\n";
        std::cout << "  - No batch file with all puzzles is kept\n";
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            print_help();
            return 0;
        } else if ((arg == "--timeout-min" || arg == "--timeout_min") && i + 1 < argc) {
            try {
                timeout_minutes = std::max(0, std::stoi(std::string(argv[++i])));
            } catch (...) {
                timeout_minutes = kDefaultGlobalTimeoutMinutes;
            }
        } else if ((arg == "--case-timeout-s" || arg == "--case_timeout_s") && i + 1 < argc) {
            try {
                case_timeout_s = std::max(0, std::stoi(std::string(argv[++i])));
            } catch (...) {
                case_timeout_s = 0;
            }
        } else if ((arg == "--attempt-time-budget-s" || arg == "--attempt_time_budget_s") && i + 1 < argc) {
            try {
                attempt_time_budget_s = std::max(0.0, std::stod(std::string(argv[++i])));
            } catch (...) {
                attempt_time_budget_s = 35.0;
            }
        } else if ((arg == "--only-n" || arg == "--only_n") && i + 1 < argc) {
            try {
                only_n = std::max(0, std::stoi(std::string(argv[++i])));
            } catch (...) {
                only_n = 0;
            }
        }
    }

    const std::filesystem::path out_dir = std::filesystem::path("TESTS_cpp") / "testowe_plansze";
    const std::filesystem::path out_dir_abs = std::filesystem::absolute(out_dir);
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        std::cerr << "Cannot create output dir: " << out_dir.string() << " err=" << ec.message() << "\n";
        return 2;
    }

    const std::filesystem::path checklist_path = out_dir / "checklista_wygenerowanych_plansz_5min.txt";
    const std::filesystem::path progress_path = out_dir / "checklista_wygenerowanych_plansz_5min_progress.txt";
    std::ofstream checklist(checklist_path, std::ios::out | std::ios::trunc);
    std::ofstream progress(progress_path, std::ios::out | std::ios::trunc);
    if (!checklist) {
        std::cerr << "Cannot open checklist file: " << checklist_path.string() << "\n";
        return 3;
    }
    checklist << std::unitbuf;
    if (progress) {
        progress << std::unitbuf;
    }

    std::vector<CaseStatus> cases;
    for (const auto& g : geometria::all_geometries()) {
        if (g.n < 4 || g.n > 36) {
            continue;
        }
        if (only_n > 0 && g.n != only_n) {
            continue;
        }
        CaseStatus s;
        s.geo = g;
        s.class_name = geometria::geometry_class_name(geometria::classify_geometry(g.box_rows, g.box_cols));
        s.output_file = build_output_file_name(g);
        s.file_prefix = build_case_file_prefix(g);
        cases.push_back(std::move(s));
    }

    std::sort(cases.begin(), cases.end(), [](const CaseStatus& a, const CaseStatus& b) {
        if (a.geo.n != b.geo.n) return a.geo.n < b.geo.n;
        if (a.geo.box_rows != b.geo.box_rows) return a.geo.box_rows < b.geo.box_rows;
        return a.geo.box_cols < b.geo.box_cols;
    });

    if (cases.empty()) {
        std::cout << "No geometries selected (only_n=" << only_n << ").\n";
        if (progress) {
            progress << "FINISH completed=0 success=0 timeout=0 elapsed_total_s=0.00\n";
        }
        checklist << "No geometries selected.\n";
        return 0;
    }

    const int thread_count = std::max(1u, std::thread::hardware_concurrency());
    const int total_timeout_s = timeout_minutes == 0 ? 0 : timeout_minutes * 60;
    const bool global_timeout_enabled = total_timeout_s > 0;
    const bool case_timeout_enabled = case_timeout_s > 0;
    constexpr uint64_t kTargetPerGeometry = 2ULL;
    constexpr int kDifficultyLevel = 1;
    constexpr RequiredStrategy kRequiredStrategy = RequiredStrategy::None;
    constexpr int kReseedIntervalS = 1;
    constexpr uint64_t kAttemptNodeBudget = 0ULL;
    constexpr bool kWriteIndividualFiles = true;
    auto fmt_double_1 = [](double v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << v;
        return oss.str();
    };
    const std::string global_params_line =
        "timeout_min=" + std::to_string(timeout_minutes) +
        ", global_timeout_s=" + (global_timeout_enabled ? std::to_string(total_timeout_s) : std::string("none")) +
        ", case_timeout_s=" + (case_timeout_enabled ? std::to_string(case_timeout_s) : std::string("none")) +
        ", attempt_time_budget_s=" + fmt_double_1(attempt_time_budget_s) +
        ", only_n=" + (only_n > 0 ? std::to_string(only_n) : std::string("all")) +
        ", threads=" + std::to_string(thread_count) +
        ", target_per_geometry=" + std::to_string(kTargetPerGeometry) +
        ", difficulty=" + std::to_string(kDifficultyLevel) +
        ", required_strategy=" + to_string(kRequiredStrategy) +
        ", reseed_interval_s=" + std::to_string(kReseedIntervalS) +
        ", attempt_node_budget=" + std::to_string(kAttemptNodeBudget) +
        ", write_individual_files=" + std::string(kWriteIndividualFiles ? "1" : "0") +
        ", output_dir=" + out_dir.string() +
        ", output_dir_abs=" + out_dir_abs.string();
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = global_timeout_enabled
        ? (start + std::chrono::seconds(total_timeout_s))
        : std::chrono::steady_clock::time_point::max();
    std::atomic<bool> global_cancel{false};
    std::atomic<bool> global_pause{false};
    std::jthread timeout_thread([&](std::stop_token st) {
        while (!st.stop_requested()) {
            if (global_timeout_enabled && std::chrono::steady_clock::now() >= deadline) {
                global_cancel.store(true, std::memory_order_relaxed);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    std::cout << "PARAMS_GLOBAL: " << global_params_line << "\n";

    checklist << "LEVEL 1 GENERATION CHECKLIST (4x4..36x36, sym+asym)\n";
    checklist << "=====================================================\n";
    checklist << "PARAMS_GLOBAL: " << global_params_line << "\n";
    checklist << "Global timeout: " << (global_timeout_enabled ? (std::to_string(total_timeout_s) + "s") : std::string("none")) << "\n";
    checklist << "Per-case timeout: " << (case_timeout_enabled ? (std::to_string(case_timeout_s) + "s") : std::string("none")) << "\n";
    checklist << "Attempt time budget per try: " << std::fixed << std::setprecision(1) << attempt_time_budget_s << "s\n";
    checklist << "Only size N: " << (only_n > 0 ? std::to_string(only_n) : std::string("all")) << "\n";
    checklist << "Threads: " << thread_count << "\n";
    checklist << "Target per geometry: 2 boards\n";
    checklist << "Output dir: " << out_dir.string() << "\n";
    checklist << "Output dir abs: " << out_dir_abs.string() << "\n";
    checklist << "Progress file: " << progress_path.string() << "\n";
    checklist << "Cases: " << cases.size() << "\n\n";

    if (progress) {
        progress << "PARAMS_GLOBAL " << global_params_line << "\n";
        progress << "START global_timeout_s=" << std::to_string(total_timeout_s)
                 << " case_timeout_s=" << (case_timeout_enabled ? std::to_string(case_timeout_s) : std::string("none"))
                 << " attempt_budget_s=" << std::fixed << std::setprecision(1) << attempt_time_budget_s
                 << " threads=" << thread_count
                 << " cases=" << cases.size() << "\n";
    }

    int completed = 0;
    int success_count = 0;
    bool global_timeout_reached = false;
    uint64_t seed_state =
        static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    for (size_t i = 0; i < cases.size(); ++i) {
        CaseStatus& cs = cases[i];
        const auto now = std::chrono::steady_clock::now();
        if (global_cancel.load(std::memory_order_relaxed) || now >= deadline) {
            global_timeout_reached = true;
            break;
        }

        if (progress) {
            progress << "START case=" << (i + 1) << "/" << cases.size()
                     << " n=" << cs.geo.n
                     << " box=" << cs.geo.box_rows << "x" << cs.geo.box_cols
                     << " class=" << cs.class_name
                     << " case_limit_s=" << (case_timeout_enabled ? std::to_string(case_timeout_s) : std::string("none"))
                     << "\n";
        }
        std::cout << "START [" << (i + 1) << "/" << cases.size() << "] "
                  << cs.geo.n << "x" << cs.geo.n
                  << " (" << cs.geo.box_rows << "x" << cs.geo.box_cols << ", " << cs.class_name << ")"
                  << " case_timeout_s=" << (case_timeout_enabled ? std::to_string(case_timeout_s) : std::string("none"))
                  << "\n";
        // Cleanup leftovers from interrupted run (generic per-puzzle names).
        for (const auto& entry : std::filesystem::directory_iterator(out_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string fname = entry.path().filename().string();
            if (!is_generic_individual_file_name(fname)) {
                continue;
            }
            std::error_code rm_ec;
            std::filesystem::remove(entry.path(), rm_ec);
        }

        GenerateRunConfig cfg;
        cfg.box_rows = cs.geo.box_rows;
        cfg.box_cols = cs.geo.box_cols;
        cfg.target_puzzles = kTargetPerGeometry;
        cfg.difficulty_level_required = kDifficultyLevel;
        cfg.required_strategy = kRequiredStrategy;
        const ClueRange auto_clues =
            resolve_auto_clue_range(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy);
        cfg.min_clues = auto_clues.min_clues;
        cfg.max_clues = auto_clues.max_clues;
        cfg.threads = thread_count;
        const uint64_t geo_mix =
            (static_cast<uint64_t>(cs.geo.n) << 32) ^
            (static_cast<uint64_t>(cs.geo.box_rows) << 16) ^
            static_cast<uint64_t>(cs.geo.box_cols);
        seed_state ^= geo_mix;
        cfg.seed = bounded_positive_seed_i64(splitmix64(seed_state));
        cfg.output_folder = out_dir.string();
        cfg.output_file = cs.output_file;
        cfg.write_individual_files = true;
        cfg.pause_on_exit_windows = false;
        cfg.max_total_time_s = case_timeout_enabled ? static_cast<uint64_t>(case_timeout_s) : 0ULL;
        cfg.max_attempts_s = 0ULL;
        cfg.reseed_interval_s = kReseedIntervalS;
        cfg.attempt_time_budget_s = attempt_time_budget_s;
        cfg.attempt_node_budget = kAttemptNodeBudget;
        const std::string case_params_line =
            "case=" + std::to_string(i + 1) + "/" + std::to_string(cases.size()) +
            ", n=" + std::to_string(cs.geo.n) +
            ", box=" + std::to_string(cs.geo.box_rows) + "x" + std::to_string(cs.geo.box_cols) +
            ", class=" + cs.class_name +
            ", seed=" + std::to_string(cfg.seed) +
            ", target=" + std::to_string(cfg.target_puzzles) +
            ", difficulty=" + std::to_string(cfg.difficulty_level_required) +
            ", required_strategy=" + to_string(cfg.required_strategy) +
            ", clues=" + std::to_string(cfg.min_clues) + "-" + std::to_string(cfg.max_clues) +
            ", threads=" + std::to_string(cfg.threads) +
            ", max_total_time_s=" + std::to_string(cfg.max_total_time_s) +
            ", max_attempts_s=" + std::to_string(cfg.max_attempts_s) +
            ", reseed_interval_s=" + std::to_string(cfg.reseed_interval_s) +
            ", attempt_time_budget_s=" + fmt_double_1(cfg.attempt_time_budget_s) +
            ", attempt_node_budget=" + std::to_string(cfg.attempt_node_budget) +
            ", output_file=" + cfg.output_file +
            ", output_folder=" + cfg.output_folder;
        std::cout << "PARAMS_CASE: " << case_params_line << "\n";
        if (progress) {
            progress << "PARAMS_CASE " << case_params_line << "\n";
        }

        const auto case_start = std::chrono::steady_clock::now();
        GenerateRunResult run{};
        std::string run_error;
        try {
            run = run_generic_sudoku(cfg, nullptr, &global_cancel, &global_pause, nullptr, nullptr);
        } catch (const std::exception& ex) {
            run_error = std::string("exception: ") + ex.what();
        } catch (...) {
            run_error = "unknown exception";
        }
        const auto case_end = std::chrono::steady_clock::now();

        cs.attempted = true;
        cs.elapsed_s = std::chrono::duration<double>(case_end - case_start).count();
        if (!run_error.empty()) {
            cs.note = run_error;
            cs.success = false;
            cs.case_timeout = false;
            ++completed;
            if (progress) {
                progress << "DONE  case=" << (i + 1) << "/" << cases.size()
                         << " n=" << cs.geo.n
                         << " box=" << cs.geo.box_rows << "x" << cs.geo.box_cols
                         << " ok=0 err=\"" << run_error << "\"\n";
            }
            continue;
        }
        cs.accepted = run.accepted;
        cs.written = run.written;
        cs.attempts = run.attempts;
        cs.case_timeout =
            (run.timeout_global > 0 || run.timeout_per_attempt > 0 || global_cancel.load(std::memory_order_relaxed));
        cs.success = (run.written >= 2);

        // Remove batch file - keep only individual puzzle files.
        {
            std::error_code rm_batch_ec;
            std::filesystem::remove(out_dir / cs.output_file, rm_batch_ec);
        }

        // Rename generated generic files (sudoku_000001.txt) to geometry-specific names.
        for (const auto& entry : std::filesystem::directory_iterator(out_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string fname = entry.path().filename().string();
            const int idx_num = generic_file_index(fname);
            if (idx_num <= 0) {
                continue;
            }
            std::ostringstream renamed;
            renamed << cs.file_prefix << "_" << std::setw(6) << std::setfill('0') << idx_num << ".txt";
            const std::filesystem::path dst = out_dir / renamed.str();
            std::error_code mv_ec;
            if (std::filesystem::exists(dst)) {
                std::error_code rm_old_ec;
                std::filesystem::remove(dst, rm_old_ec);
            }
            std::filesystem::rename(entry.path(), dst, mv_ec);
            if (mv_ec) {
                // Best effort fallback: copy then remove source.
                std::error_code cp_ec;
                std::filesystem::copy_file(entry.path(), dst, std::filesystem::copy_options::overwrite_existing, cp_ec);
                if (!cp_ec) {
                    std::error_code rm_src_ec;
                    std::filesystem::remove(entry.path(), rm_src_ec);
                }
            }
        }

        std::ostringstream note;
        note << "written=" << run.written << "/2"
             << ", accepted=" << run.accepted
             << ", attempts=" << run.attempts
             << ", elapsed_s=" << std::fixed << std::setprecision(2) << cs.elapsed_s
             << ", case_limit_s=" << (case_timeout_enabled ? std::to_string(case_timeout_s) : std::string("none"))
             << ", attempt_budget_s=" << std::fixed << std::setprecision(1) << attempt_time_budget_s
             << ", seed=" << cfg.seed
             << ", timeout=" << (cs.case_timeout ? "1" : "0");
        cs.note = note.str();

        ++completed;
        if (cs.success) {
            ++success_count;
        }

        if (progress) {
            progress << "DONE  case=" << (i + 1) << "/" << cases.size()
                     << " n=" << cs.geo.n
                     << " box=" << cs.geo.box_rows << "x" << cs.geo.box_cols
                     << " ok=" << (cs.success ? "1" : "0")
                     << " " << cs.note << "\n";
        }
        std::cout << "DONE  [" << (i + 1) << "/" << cases.size() << "] "
                  << (cs.success ? "OK" : "FAIL")
                  << " " << cs.geo.n << "x" << cs.geo.n
                  << " (" << cs.geo.box_rows << "x" << cs.geo.box_cols << ") "
                  << cs.note
                  << ", files_prefix=" << cs.file_prefix << "\n";
    }

    const double elapsed_total_s =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    checklist << "CHECKLIST:\n";
    for (size_t i = 0; i < cases.size(); ++i) {
        const CaseStatus& cs = cases[i];
        const std::string mark = (cs.attempted && cs.success) ? "[x]" : "[ ]";
        checklist << mark
                  << " sudoku " << cs.geo.n << "x" << cs.geo.n
                  << " (" << cs.geo.box_rows << "x" << cs.geo.box_cols
                  << ", " << cs.class_name << ")";
        if (!cs.attempted) {
            checklist << " -> not attempted (global timeout)";
        } else {
            checklist << " -> " << cs.note
                      << ", files_prefix=" << cs.file_prefix;
        }
        checklist << "\n";
    }

    checklist << "\nSUMMARY:\n";
    checklist << "completed_cases=" << completed << "/" << cases.size() << "\n";
    checklist << "success_cases_2of2=" << success_count << "\n";
    checklist << "global_timeout_reached=" << (global_timeout_reached ? "yes" : "no") << "\n";
    checklist << "elapsed_total_s=" << std::fixed << std::setprecision(2) << elapsed_total_s << "\n";
    if (global_timeout_reached) {
        checklist << "TEST STOPPED BY TIME LIMIT\n";
    }

    if (progress) {
        progress << "FINISH completed=" << completed
                 << " success=" << success_count
                 << " timeout=" << (global_timeout_reached ? "1" : "0")
                 << " elapsed_total_s=" << std::fixed << std::setprecision(2) << elapsed_total_s << "\n";
    }

    std::cout << "Checklist: " << checklist_path.string() << "\n";
    std::cout << "Progress: " << progress_path.string() << "\n";
    std::cout << "Output dir: " << out_dir.string() << "\n";
    std::cout << "Output dir abs: " << out_dir_abs.string() << "\n";
    std::cout << "Completed: " << completed << "/" << cases.size()
              << ", success(2/2): " << success_count << "\n";

    return 0;
}
