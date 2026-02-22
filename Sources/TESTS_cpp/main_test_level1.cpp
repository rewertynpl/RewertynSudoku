// ============================================================================
// SUDOKU HPC - TEST REGRESYJNY POZIOMU 1 (ASYMETRYCZNE GEOMETRIE)
// Plik: main_test_level1.cpp
// ============================================================================

#include <iostream>
#include <cstdint>
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
    out << "  sudoku_test_level1.exe [options]\n\n";
    out << "Options:\n";
    out << "  --help, -h, /?                Show this help\n";
    out << "  --timeout-min <int>           Global timeout in minutes\n";
    out << "                                0 = no limit, -1 = env/default fallback\n";
    out << "\nEnvironment fallback:\n";
    out << "  SUDOKU_LEVEL1_TIMEOUT_S       Timeout in seconds (when --timeout-min is not set)\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (has_help_arg(argc, argv)) {
        print_help(std::cout);
        return 0;
    }

    std::cout << "============================================================================\n";
    std::cout << "SUDOKU HPC - LEVEL 1 ASYMMETRIC REGRESSION TEST SUITE\n";
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

    std::cout << "Testing asymmetric geometries (e.g., 12x12 4x3, 10x10 2x5)...\n";
    std::cout << "Running Level 1 strategy tests...\n";
    std::cout << "Global timeout [min]: "
              << (timeout_minutes >= 0 ? std::to_string(timeout_minutes) : std::string("env/default"))
              << "\n\n";

    sudoku_testy::run_level1_asymmetric_tests("level1_asymmetric_report.txt", timeout_minutes);

    std::cout << "\nDone. Check level1_asymmetric_report.txt\n";
    return 0;
}
