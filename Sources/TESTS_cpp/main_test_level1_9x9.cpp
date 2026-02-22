// ============================================================================
// SUDOKU HPC - TEST STRATEGII POZIOMU 1 DLA 9x9 (3x3)
// ============================================================================

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <string>

#include "../strategieSource/strategie_types.h"
#include "test_level1_asymmetric.cpp"

namespace {

bool has_help_arg(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            return true;
        }
    }
    return false;
}

void print_help(std::ostream& out) {
    out << "Usage:\n";
    out << "  sudoku_test_level1_9x9.exe [options]\n\n";
    out << "Options:\n";
    out << "  --help, -h, /?                Show this help\n";
    out << "  --timeout-min <int>           Global timeout in minutes\n";
    out << "                                0 = no limit, -1 = env/default fallback\n";
    out << "\nEnvironment fallback:\n";
    out << "  SUDOKU_LEVEL1_9X9_TIMEOUT_S   Timeout in seconds (when --timeout-min is not set)\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (has_help_arg(argc, argv)) {
        print_help(std::cout);
        return 0;
    }

    using sudoku_testy::TestResult;

    std::cout << "============================================================================\n";
    std::cout << "SUDOKU HPC - LEVEL 1 9x9 (3x3) TEST SUITE\n";
    std::cout << "============================================================================\n\n";

    const int box_rows = 3;
    const int box_cols = 3;
    const uint64_t seed = 9001;
    int timeout_minutes = -1;  // -1: fallback env; 0: no limit; >0: global X min
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--timeout-min" || arg == "--timeout_min") && i + 1 < argc) {
            try {
                timeout_minutes = std::stoi(std::string(argv[++i]));
            } catch (...) {
                timeout_minutes = -1;
            }
        }
    }

    const auto suite_start = std::chrono::steady_clock::now();
    int timeout_s = 0;
    if (timeout_minutes >= 0) {
        timeout_s = (timeout_minutes == 0) ? 0 : (std::max(1, timeout_minutes) * 60);
    } else if (const char* raw = std::getenv("SUDOKU_LEVEL1_9X9_TIMEOUT_S")) {
        try {
            timeout_s = std::max(0, std::stoi(std::string(raw)));
        } catch (...) {
            timeout_s = 0;
        }
    }
    const bool timeout_enabled = timeout_s > 0;
    const auto deadline = suite_start + std::chrono::seconds(timeout_enabled ? timeout_s : 0);
    auto timeout_reached = [&]() -> bool {
        return timeout_enabled && std::chrono::steady_clock::now() >= deadline;
    };

    std::vector<TestResult> results;
    results.reserve(5);
    int planned_tests = 5;
    bool timed_out = false;

    std::ofstream report("level1_9x9_report.txt");
    if (report) {
        report << std::unitbuf;
        report << "LEVEL 1 9x9 (3x3) TEST REPORT\n";
        report << "==============================\n\n";
        report << "Timeout limit: " << (timeout_enabled ? (std::to_string(timeout_s) + "s") : std::string("none")) << "\n\n";

        auto run_case = [&](const std::string& name, const std::function<TestResult()>& fn) {
            if (timeout_reached()) {
                timed_out = true;
                report << "[TIMEOUT] " << name << "\n";
                return;
            }
            TestResult r;
            try {
                r = fn();
            } catch (const std::exception& ex) {
                r.passed = false;
                r.name = name;
                r.message = std::string("Exception: ") + ex.what();
            } catch (...) {
                r.passed = false;
                r.name = name;
                r.message = "Unknown exception";
            }
            results.push_back(r);
            report << (r.passed ? "[PASS] " : "[FAIL] ") << r.name
                   << " - " << r.message
                   << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
        };

        run_case("GeometryCatalogRecognition", [&]() { return sudoku_testy::test_geometry_recognition_catalog(); });
        run_case("AsymmetricGen 9x9", [&]() { return sudoku_testy::test_asymmetric_generation<9>(box_rows, box_cols, seed); });
        run_case("NakedSingle 9x9", [&]() { return sudoku_testy::test_naked_single_asymmetric<9>(box_rows, box_cols, seed); });
        run_case("HiddenSingle 9x9", [&]() { return sudoku_testy::test_hidden_single_asymmetric<9>(box_rows, box_cols, seed); });
        run_case("Level1Full 9x9", [&]() { return sudoku_testy::test_level1_full_solve<9>(box_rows, box_cols, seed); });

        int passed = 0;
        for (const auto& r : results) {
            if (r.passed) {
                ++passed;
            }
        }
        report << "\nSUMMARY: " << passed << "/" << results.size() << " passed\n";
        report << "Completed/planned: " << results.size() << "/" << planned_tests << "\n";
        report << "Timeout reached: " << (timed_out ? "yes" : "no") << "\n";
        if (timed_out) {
            report << "TEST STOPPED BY TIME LIMIT\n";
        }
    }

    int passed = 0;
    for (const auto& r : results) {
        if (r.passed) {
            ++passed;
        }
    }

    std::cout << "Total: " << results.size()
              << ", Passed: " << passed
              << ", Failed: " << (static_cast<int>(results.size()) - passed) << "\n";
    std::cout << "Report: level1_9x9_report.txt\n";
    if (timed_out) {
        return 2;
    }
    return (passed == static_cast<int>(results.size())) ? 0 : 1;
}
