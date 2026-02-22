// ============================================================================
// SUDOKU HPC - PROSTE TESTY REGRESYJNE (64-bit, asymetryczne geometrie)
// Plik: test_regression.cpp
// Uwaga: brak include guard - ten plik jest wciągany jako unity-include
// ============================================================================

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <random>
#include <cstdint>
#include <bit>
#include <cstdlib>

#include "../strategieSource/strategie_types.h"

namespace sudoku_testy {

// ============================================================================
// WYNIKI TESTÓW
// ============================================================================

struct TestResult {
    bool passed = false;
    std::string name;
    std::string message;
    double elapsed_ms = 0.0;
};

// ============================================================================
// SEED CORPUS - znane dobre ziarna
// ============================================================================

struct SeedEntry {
    int box_rows;
    int box_cols;
    uint64_t seed;
    const char* desc;
};

inline const std::vector<SeedEntry>& get_seed_corpus() {
    static std::vector<SeedEntry> corpus = {
        {2, 3, 1001, "6x6 basic"},
        {3, 2, 1002, "6x6 alt"},
        {2, 4, 2001, "8x8 basic"},
        {3, 3, 3001, "9x9 basic"},
        {3, 3, 12345, "9x9 seed12345"},
        {3, 3, 54321, "9x9 seed54321"},
        {4, 4, 6001, "16x16 basic"},
        {2, 8, 6002, "16x16 alt"},
    };
    return corpus;
}

inline int get_seed_corpus_size() {
    return static_cast<int>(get_seed_corpus().size());
}

inline int regression_timeout_seconds_from_env() {
    const char* raw = std::getenv("SUDOKU_REGRESSION_TIMEOUT_S");
    if (raw == nullptr || *raw == '\0') {
        return 0;
    }
    try {
        return std::max(0, std::stoi(std::string(raw)));
    } catch (...) {
        return 0;
    }
}

inline int timeout_seconds_from_minutes_or_env(int timeout_minutes, const char* env_name) {
    if (timeout_minutes >= 0) {
        if (timeout_minutes == 0) {
            return 0;
        }
        return std::max(1, timeout_minutes) * 60;
    }
    const char* raw = std::getenv(env_name);
    if (raw == nullptr || *raw == '\0') {
        return 0;
    }
    try {
        return std::max(0, std::stoi(std::string(raw)));
    } catch (...) {
        return 0;
    }
}

// ============================================================================
// TEST 1: Poprawność generowania (basic sanity check)
// ============================================================================

// Prosty backtracking na BoardSoA<N> z 64-bitowymi maskami
template<int N>
bool generate_solved_board(sudoku_strategie::BoardSoA<N>& board,
                           const sudoku_strategie::TopologyCache<N>& topo,
                           std::mt19937_64& rng) {
    int idx = -1;
    for (int i = 0; i < N * N; ++i) {
        if (board.values[static_cast<size_t>(i)] == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return true;

    uint64_t mask = board.candidate_mask_for_idx(idx);
    if (mask == 0ULL) return false;

    std::vector<int> candidates;
    candidates.reserve(N);
    for (int d = 1; d <= N; ++d) {
        if (mask & (1ULL << (d - 1))) {
            candidates.push_back(d);
        }
    }

    std::shuffle(candidates.begin(), candidates.end(), rng);

    for (int d : candidates) {
        board.place(idx, d);
        if (generate_solved_board<N>(board, topo, rng)) {
            return true;
        }
        board.unplace(idx, d);
    }
    return false;
}

template<int N>
TestResult test_generation_sanity(int box_rows, int box_cols, uint64_t seed, int count = 3) {
    TestResult result;
    result.name = "Sanity " + std::to_string(N) + "x" + std::to_string(N) +
                  " seed=" + std::to_string(seed);

    const auto start = std::chrono::steady_clock::now();

    using namespace sudoku_strategie;

    TopologyCache<N> topo;
    topo.build(box_rows, box_cols);

    BoardSoA<N> board;
    board.init_geometry(box_rows, box_cols);

    std::mt19937_64 rng(seed);
    int passed = 0;

    for (int i = 0; i < count; ++i) {
        board.clear();

        bool ok = generate_solved_board<N>(board, topo, rng);

        if (ok && board.is_full()) {
            bool valid = true;
            for (int h = 0; h < topo.NUM_HOUSES; ++h) {
                uint64_t seen = 0ULL;
                for (int k = 0; k < N; ++k) {
                    int idx = topo.houses[h][k];
                    uint8_t v = board.values[static_cast<size_t>(idx)];
                    if (v == 0) { valid = false; break; }
                    uint64_t bit = (1ULL << (v - 1));
                    if (seen & bit) { valid = false; break; }
                    seen |= bit;
                }
                if (!valid) break;
            }
            if (valid) ++passed;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = (passed == count);
    result.message = std::to_string(passed) + "/" + std::to_string(count) + " generated grids valid";
    return result;
}

// ============================================================================
// TEST 2: Unikalność candidates (no contradictions)
// ============================================================================

template<int N>
TestResult test_no_contradictions(int box_rows, int box_cols, uint64_t seed, int count = 5) {
    TestResult result;
    result.name = "NoContradictions " + std::to_string(N) + "x" + std::to_string(N);

    const auto start = std::chrono::steady_clock::now();

    using namespace sudoku_strategie;

    TopologyCache<N> topo;
    topo.build(box_rows, box_cols);

    BoardSoA<N> board;
    board.init_geometry(box_rows, box_cols);

    std::mt19937_64 rng(seed);
    int passed = 0;

    for (int i = 0; i < count; ++i) {
        board.clear();
        bool contradiction = false;

        std::vector<int> cells(N * N);
        for (int j = 0; j < N * N; ++j) cells[static_cast<size_t>(j)] = j;
        std::shuffle(cells.begin(), cells.end(), rng);

        for (int idx : cells) {
            uint64_t mask = board.candidate_mask_for_idx(idx);
            if (mask == 0ULL) {
                contradiction = true;
                break;
            }
            int d = std::countr_zero(mask) + 1;
            board.place(idx, d);
        }

        if (!contradiction && board.is_full()) {
            ++passed;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = true;  // Test informacyjny
    result.message = std::to_string(passed) + "/" + std::to_string(count) + " no contradictions (info only)";
    return result;
}

// ============================================================================
// URUCHOMIENIE WSZYSTKICH TESTÓW
// ============================================================================

inline void run_all_regression_tests(
    const std::string& report_path,
    int timeout_minutes) {
    std::cerr << " Starting regression tests..." << std::endl;
    std::cerr << " Report path: " << report_path << std::endl;

    std::ofstream report(report_path);
    if (!report) {
        std::cerr << " Cannot open: " << report_path << std::endl;
        std::cerr << " Trying current dir..." << std::endl;
        report.open("regression_report.txt");
        if (!report) {
            std::cerr << " Cannot open regression_report.txt either!" << std::endl;
            return;
        }
    }
    std::cerr << " Report file opened successfully" << std::endl;
    report << std::unitbuf;

    std::atomic<int> total_tests{0};
    std::atomic<int> passed_tests{0};
    int completed_tests = 0;
    int planned_tests = 0;
    bool timed_out = false;
    const auto suite_start = std::chrono::steady_clock::now();
    const int timeout_s = timeout_seconds_from_minutes_or_env(timeout_minutes, "SUDOKU_REGRESSION_TIMEOUT_S");
    const bool timeout_enabled = timeout_s > 0;
    const auto deadline = suite_start + std::chrono::seconds(timeout_enabled ? timeout_s : 0);
    auto timeout_reached = [&]() -> bool {
        return timeout_enabled && std::chrono::steady_clock::now() >= deadline;
    };
    auto elapsed_ms = [&]() -> double {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - suite_start).count();
    };

    const auto& corpus = get_seed_corpus();
    std::vector<SeedEntry> ordered_corpus(corpus.begin(), corpus.end());
    std::sort(ordered_corpus.begin(), ordered_corpus.end(), [](const SeedEntry& a, const SeedEntry& b) {
        const int an = a.box_rows * a.box_cols;
        const int bn = b.box_rows * b.box_cols;
        if (an != bn) return an < bn;
        if (a.box_rows != b.box_rows) return a.box_rows < b.box_rows;
        if (a.box_cols != b.box_cols) return a.box_cols < b.box_cols;
        return a.seed < b.seed;
    });
    auto is_supported_n = [](int n) -> bool {
        return n == 6 || n == 8 || n == 9 || n == 16;
    };
    for (const auto& entry : ordered_corpus) {
        const int n = entry.box_rows * entry.box_cols;
        if (is_supported_n(n)) {
            planned_tests += 2;  // sanity + no contradictions
        }
    }
    report << "SEED CORPUS: " << corpus.size() << " entries\n\n";
    report << "Timeout limit: " << (timeout_enabled ? (std::to_string(timeout_s) + "s") : std::string("none")) << "\n";
    report << "Order: rosnaco po N (male -> duze)\n";
    report << "Planned tests: " << planned_tests << "\n\n";

    // TEST 1: GENERATION SANITY
    report << "=== TEST 1: GENERATION SANITY ===\n\n";

    for (const auto& entry : ordered_corpus) {
        if (timeout_reached()) {
            timed_out = true;
            report << "TIMEOUT reached before seed=" << entry.seed << " in TEST 1\n";
            break;
        }
        const int N = entry.box_rows * entry.box_cols;
        TestResult r;
        try {
            if (N == 6)      r = test_generation_sanity<6>(entry.box_rows, entry.box_cols, entry.seed, 3);
            else if (N == 8) r = test_generation_sanity<8>(entry.box_rows, entry.box_cols, entry.seed, 3);
            else if (N == 9) r = test_generation_sanity<9>(entry.box_rows, entry.box_cols, entry.seed, 5);
            else if (N == 16) r = test_generation_sanity<16>(entry.box_rows, entry.box_cols, entry.seed, 2);
            else continue;
        } catch (const std::exception& e) {
            r.passed = false;
            r.message = std::string("Exception: ") + e.what();
        } catch (...) {
            r.passed = false;
            r.message = "Unknown exception";
        }

        if (r.passed) ++passed_tests;
        ++total_tests;
        ++completed_tests;

        report << " " << r.name << " (" << entry.desc << ")\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
    }

    // TEST 2: NO CONTRADICTIONS
    report << "\n=== TEST 2: NO CONTRADICTIONS ===\n\n";

    if (!timed_out) for (const auto& entry : ordered_corpus) {
        if (timeout_reached()) {
            timed_out = true;
            report << "TIMEOUT reached before seed=" << entry.seed << " in TEST 2\n";
            break;
        }
        const int N = entry.box_rows * entry.box_cols;
        TestResult r;
        try {
            if (N == 6)       r = test_no_contradictions<6>(entry.box_rows, entry.box_cols, entry.seed, 5);
            else if (N == 8)  r = test_no_contradictions<8>(entry.box_rows, entry.box_cols, entry.seed, 5);
            else if (N == 9)  r = test_no_contradictions<9>(entry.box_rows, entry.box_cols, entry.seed, 5);
            else if (N == 16) r = test_no_contradictions<16>(entry.box_rows, entry.box_cols, entry.seed, 3);
            else continue;
        } catch (const std::exception& e) {
            r.passed = false;
            r.message = std::string("Exception: ") + e.what();
        } catch (...) {
            r.passed = false;
            r.message = "Unknown exception";
        }

        if (r.passed) ++passed_tests;
        ++total_tests;
        ++completed_tests;

        report << " " << r.name << " (" << entry.desc << ")\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
    }

    report << "\n============================================================================\n";
    report << "SUMMARY\n";
    report << "============================================================================\n";
    report << "Total:  " << total_tests.load() << "\n";
    report << "Passed: " << passed_tests.load() << "\n";
    report << "Failed: " << (total_tests.load() - passed_tests.load()) << "\n";
    report << "Rate:   " << std::fixed << std::setprecision(1)
           << (100.0 * passed_tests.load() / std::max(1, total_tests.load())) << "%\n";
    report << "Completed/planned: " << completed_tests << "/" << planned_tests << "\n";
    report << "Suite elapsed: " << std::fixed << std::setprecision(2) << elapsed_ms() << "ms\n";
    report << "Timeout reached: " << (timed_out ? "yes" : "no") << "\n";

    const bool all_passed = (passed_tests.load() == total_tests.load());
    if (timed_out) {
        report << "\nTEST STOPPED BY TIME LIMIT\n";
    } else {
        report << "\n" << (all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    }

    report.close();

    std::cout << "\n=== REGRESSION TEST SUMMARY ===\n";
    std::cout << "Total: " << total_tests.load()
              << ", Passed: " << passed_tests.load()
              << ", Failed: " << (total_tests.load() - passed_tests.load()) << "\n";
    std::cout << "Report: " << report_path << "\n";
}

inline void run_all_regression_tests(const std::string& report_path = "regression_report.txt") {
    run_all_regression_tests(report_path, -1);
}

} // namespace sudoku_testy

