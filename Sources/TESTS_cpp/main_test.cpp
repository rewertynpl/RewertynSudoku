// ============================================================================
// SUDOKU HPC - TEST REGRESSION RUNNER (CLI)
// Plik: main_test.cpp
// ============================================================================

#include <iostream>
#include <cstdint>
#include <string>

#include "../strategieSource/strategie_types.h"
#include "test_regression.cpp"

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
    out << "  sudoku_test.exe [options]\n\n";
    out << "Options:\n";
    out << "  --help, -h, /?                Show this help\n";
    out << "  --timeout-min <int>           Global timeout in minutes\n";
    out << "                                0 = no limit, -1 = env/default fallback\n";
    out << "\nEnvironment fallback:\n";
    out << "  SUDOKU_REGRESSION_TIMEOUT_S   Timeout in seconds (when --timeout-min is not set)\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (has_help_arg(argc, argv)) {
        print_help(std::cout);
        return 0;
    }

    std::cout << "============================================================================\n";
    std::cout << "SUDOKU HPC - REGRESSION TEST SUITE\n";
    std::cout << "============================================================================\n\n";

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

    std::cout << "Seed corpus: " << sudoku_testy::get_seed_corpus_size() << " entries\n";
    std::cout << "Running tests...\n";
    std::cout << "Global timeout [min]: "
              << (timeout_minutes >= 0 ? std::to_string(timeout_minutes) : std::string("env/default"))
              << "\n\n";

    sudoku_testy::run_all_regression_tests("regression_report.txt", timeout_minutes);

    std::cout << "\nDone. Check regression_report.txt\n";
    return 0;
}

