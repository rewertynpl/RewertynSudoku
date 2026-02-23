// ============================================================================
// SUDOKU HPC - TESTY REGRESYJNE STRATEGII POZIOMU 1
// Testy dla geometrii asymetrycznych (np. 12x12 z blokami 4x3, 10x10 z 2x5)
// ============================================================================

#ifndef STRATEGIE_LEVEL1_TESTS_H
#define STRATEGIE_LEVEL1_TESTS_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <random>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <future>

#include "../strategieSource/strategie_types.h"
#include "../strategieSource/strategie_scratch.h"
#include "../strategieSource/strategylevels1.h"
#include "../geometry.h"

namespace sudoku_testy {

using namespace sudoku_strategie;

// ============================================================================
// STRUKTURY WYNIKÓW TESTÓW
// ============================================================================

struct TestResult {
    bool passed = false;
    std::string name;
    std::string message;
    double elapsed_ms = 0.0;
};

// ============================================================================
// GEOMETRIE ASYMETRYCZNE DO TESTÓW
// ============================================================================

struct AsymmetricGeometry {
    int N;
    int box_rows;
    int box_cols;
    std::string desc;
    uint64_t test_seed;
};

struct ExpectedGeometry {
    int N;
    int box_rows;
    int box_cols;
    std::string desc;
};

inline const std::vector<ExpectedGeometry>& get_expected_supported_geometries() {
    static std::vector<ExpectedGeometry> geometries = [] {
        std::vector<ExpectedGeometry> out;
        for (const auto& g : sudoku_hpc::geometria::all_geometries()) {
            out.push_back({g.n, g.box_rows, g.box_cols, g.label});
        }
        return out;
    }();
    return geometries;
}

inline const std::vector<AsymmetricGeometry>& get_asymmetric_geometries() {
    static std::vector<AsymmetricGeometry> geometries = [] {
        std::vector<AsymmetricGeometry> out;
        for (const auto& g : sudoku_hpc::geometria::all_geometries()) {
            if (g.n < 6 || g.n > 20) {
                continue;
            }
            if (g.box_rows == g.box_cols && g.n != 16) {
                continue;
            }
            const uint64_t seed = static_cast<uint64_t>(g.n * 1000 + g.box_rows * 100 + g.box_cols);
            out.push_back({g.n, g.box_rows, g.box_cols, g.label, seed});
        }
        return out;
    }();
    return geometries;
}

inline std::string level1_progress_path_from_report(const std::string& report_path) {
    std::filesystem::path p(report_path);
    const std::string stem = p.has_stem() ? p.stem().string() : std::string("level1_asymmetric_report");
    std::filesystem::path out = p.parent_path() / (stem + "_progress.txt");
    return out.string();
}

inline int level1_timeout_seconds_from_env() {
    const char* raw = std::getenv("SUDOKU_LEVEL1_TIMEOUT_S");
    if (raw == nullptr || *raw == '\0') {
        return 0;
    }
    try {
        int v = std::stoi(std::string(raw));
        return std::max(0, v);
    } catch (...) {
        return 0;
    }
}

inline int level1_case_timeout_seconds_from_env() {
    const char* raw = std::getenv("SUDOKU_LEVEL1_CASE_TIMEOUT_S");
    if (raw == nullptr || *raw == '\0') {
        return 30;
    }
    try {
        int v = std::stoi(std::string(raw));
        return std::max(0, v);
    } catch (...) {
        return 30;
    }
}

inline int level1_timeout_seconds_from_minutes_or_env(int timeout_minutes) {
    if (timeout_minutes >= 0) {
        if (timeout_minutes == 0) {
            return 0;
        }
        return std::max(1, timeout_minutes) * 60;
    }
    return level1_timeout_seconds_from_env();
}

inline int level1_case_timeout_seconds_from_arg_or_env(int case_timeout_s) {
    if (case_timeout_s >= 0) {
        return std::max(0, case_timeout_s);
    }
    return level1_case_timeout_seconds_from_env();
}

inline TestResult test_geometry_recognition_catalog() {
    TestResult result;
    result.name = "GeometryCatalogRecognition";
    const auto start = std::chrono::steady_clock::now();

    sudoku_hpc::geometria::zainicjalizuj_geometrie();
    const auto& expected = get_expected_supported_geometries();
    std::vector<std::string> missing;
    missing.reserve(expected.size());

    for (const auto& g : expected) {
        if (!sudoku_hpc::geometria::czy_obslugiwana(g.N, g.box_rows, g.box_cols)) {
            missing.push_back(g.desc);
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = missing.empty();
    if (missing.empty()) {
        result.message = std::to_string(expected.size()) + "/" + std::to_string(expected.size()) + " geometries recognized";
    } else {
        result.message = std::to_string(expected.size() - missing.size()) + "/" + std::to_string(expected.size()) +
                         " recognized; missing: " + missing.front();
        if (missing.size() > 1) {
            result.message += " (+" + std::to_string(missing.size() - 1) + " more)";
        }
    }
    return result;
}

// ============================================================================
// POMOCNICZE FUNKCJE DO GENEROWANIA I WALIDACJI
// ============================================================================

template<int N>
bool generate_solved_board_level1(
    BoardSoA<N>& board,
    const TopologyCache<N>& topo,
    std::mt19937_64& rng) 
{
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
        if (generate_solved_board_level1<N>(board, topo, rng)) {
            return true;
        }
        board.unplace(idx, d);
    }
    return false;
}

template<int N>
bool validate_solved_board(
    const BoardSoA<N>& board,
    const TopologyCache<N>& topo) 
{
    // Sprawdź czy wszystkie pola są wypełnione
    for (int i = 0; i < N * N; ++i) {
        if (board.values[static_cast<size_t>(i)] == 0) {
            return false;
        }
    }

    // Sprawdź każdy house (wiersz, kolumna, blok)
    for (int h = 0; h < topo.NUM_HOUSES; ++h) {
        uint64_t seen = 0ULL;
        for (int k = 0; k < N; ++k) {
            int idx = topo.houses[h][k];
            uint8_t v = board.values[static_cast<size_t>(idx)];
            if (v == 0 || v > N) {
                return false;
            }
            uint64_t bit = (1ULL << (v - 1));
            if (seen & bit) {
                return false;  // Powtórzenie cyfry
            }
            seen |= bit;
        }
        if (seen != ((1ULL << N) - 1ULL)) {
            return false;  // Brakujące cyfry
        }
    }
    return true;
}

template<int N>
bool test_naked_single_on_puzzle(
    BoardSoA<N>& board,
    const TopologyCache<N>& topo,
    LogicCertifyResult& result) 
{
    using namespace level1;
    
    StrategyLevel1<N> strategy(topo);
    
    // Spróbuj zastosować Naked Single
    ApplyResult r = strategy.apply_strategy(board, result, StrategyId::NakedSingle);

    return (r == ApplyResult::Progress) && strategy.was_strategy_used(result, StrategyId::NakedSingle);
}

template<int N>
bool test_hidden_single_on_puzzle(
    BoardSoA<N>& board,
    const TopologyCache<N>& topo,
    LogicCertifyResult& result) 
{
    using namespace level1;
    
    StrategyLevel1<N> strategy(topo);
    
    // Spróbuj zastosować Hidden Single
    ApplyResult r = strategy.apply_strategy(board, result, StrategyId::HiddenSingle);

    return (r == ApplyResult::Progress) && strategy.was_strategy_used(result, StrategyId::HiddenSingle);
}

// ============================================================================
// TEST 1: Generowanie poprawnej planszy dla asymetrycznych geometrii
// ============================================================================

template<int N>
TestResult test_asymmetric_generation(int box_rows, int box_cols, uint64_t seed) {
    TestResult result;
    result.name = "AsymmetricGen " + std::to_string(N) + "x" + std::to_string(N) + 
                  " (" + std::to_string(box_rows) + "x" + std::to_string(box_cols) + ")";

    const auto start = std::chrono::steady_clock::now();

    TopologyCache<N> topo;
    topo.build(box_rows, box_cols);

    BoardSoA<N> board;
    board.init_geometry(box_rows, box_cols);

    std::mt19937_64 rng(seed);
    int passed = 0;
    int attempts = 3;

    for (int i = 0; i < attempts; ++i) {
        board.clear();

        bool ok = generate_solved_board_level1<N>(board, topo, rng);

        if (ok && validate_solved_board<N>(board, topo)) {
            ++passed;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = (passed == attempts);
    result.message = std::to_string(passed) + "/" + std::to_string(attempts) + " grids valid";
    return result;
}

// ============================================================================
// TEST 2: Naked Single na asymetrycznej planszy
// ============================================================================

template<int N>
TestResult test_naked_single_asymmetric(int box_rows, int box_cols, uint64_t seed) {
    TestResult result;
    result.name = "NakedSingle " + std::to_string(N) + "x" + std::to_string(N) + 
                  " (" + std::to_string(box_rows) + "x" + std::to_string(box_cols) + ")";

    const auto start = std::chrono::steady_clock::now();

    TopologyCache<N> topo;
    topo.build(box_rows, box_cols);

    BoardSoA<N> board;
    board.init_geometry(box_rows, box_cols);

    std::mt19937_64 rng(seed);
    
    // Wygeneruj pełną planszę
    board.clear();
    if (!generate_solved_board_level1<N>(board, topo, rng)) {
        const auto end = std::chrono::steady_clock::now();
        result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.passed = false;
        result.message = "Failed to generate solved board";
        return result;
    }
    
    // Usuń jedną cyfrę tak, aby Naked Single miał deterministyczny ruch.
    std::vector<int> indices(N * N);
    for (int i = 0; i < N * N; ++i) indices[static_cast<size_t>(i)] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    const int idx = indices.front();
    const int digit = board.values[static_cast<size_t>(idx)];
    board.unplace(idx, digit);
    board.fixed[static_cast<size_t>(idx)] = 0;

    LogicCertifyResult logic_result;
    bool ok = test_naked_single_on_puzzle<N>(board, topo, logic_result);

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = ok;
    result.message = ok ? "Naked Single applied correctly" : "Naked Single failed";
    return result;
}

// ============================================================================
// TEST 3: Hidden Single na asymetrycznej planszy
// ============================================================================

template<int N>
TestResult test_hidden_single_asymmetric(int box_rows, int box_cols, uint64_t seed) {
    TestResult result;
    result.name = "HiddenSingle " + std::to_string(N) + "x" + std::to_string(N) + 
                  " (" + std::to_string(box_rows) + "x" + std::to_string(box_cols) + ")";

    const auto start = std::chrono::steady_clock::now();

    TopologyCache<N> topo;
    topo.build(box_rows, box_cols);

    BoardSoA<N> board;
    board.init_geometry(box_rows, box_cols);

    std::mt19937_64 rng(seed);
    
    // Wygeneruj pełną planszę
    board.clear();
    if (!generate_solved_board_level1<N>(board, topo, rng)) {
        const auto end = std::chrono::steady_clock::now();
        result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.passed = false;
        result.message = "Failed to generate solved board";
        return result;
    }
    
    // Usuń jedną cyfrę: Hidden Single powinien mieć unikalne miejsce w house.
    std::vector<int> indices(N * N);
    for (int i = 0; i < N * N; ++i) indices[static_cast<size_t>(i)] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    const int idx = indices.front();
    const int digit = board.values[static_cast<size_t>(idx)];
    board.unplace(idx, digit);
    board.fixed[static_cast<size_t>(idx)] = 0;

    LogicCertifyResult logic_result;
    bool ok = test_hidden_single_on_puzzle<N>(board, topo, logic_result);

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = ok;
    result.message = ok ? "Hidden Single applied correctly" : "Hidden Single failed";
    return result;
}

// ============================================================================
// TEST 4: Pełne rozwiązywanie z użyciem strategii poziomu 1
// ============================================================================

template<int N>
TestResult test_level1_full_solve(int box_rows, int box_cols, uint64_t seed) {
    TestResult result;
    result.name = "Level1Full " + std::to_string(N) + "x" + std::to_string(N) + 
                  " (" + std::to_string(box_rows) + "x" + std::to_string(box_cols) + ")";

    const auto start = std::chrono::steady_clock::now();

    TopologyCache<N> topo;
    topo.build(box_rows, box_cols);

    BoardSoA<N> board;
    board.init_geometry(box_rows, box_cols);

    std::mt19937_64 rng(seed);
    
    // Wygeneruj pełną planszę
    board.clear();
    if (!generate_solved_board_level1<N>(board, topo, rng)) {
        const auto end = std::chrono::steady_clock::now();
        result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.passed = false;
        result.message = "Failed to generate solved board";
        return result;
    }
    
    // Zapisz rozwiązanie
    std::array<uint8_t, N*N> solution = board.values;
    
    // Usuń jedną cyfrę, aby łamigłówka była rozwiązywalna samym Level 1.
    std::vector<int> indices(N * N);
    for (int i = 0; i < N * N; ++i) indices[static_cast<size_t>(i)] = i;
    std::shuffle(indices.begin(), indices.end(), rng);
    const int idx = indices.front();
    const int digit = board.values[static_cast<size_t>(idx)];
    board.unplace(idx, digit);
    board.fixed[static_cast<size_t>(idx)] = 0;
    
    // Rozwiązuj używając strategii poziomu 1
    using namespace level1;
    StrategyLevel1<N> strategy(topo);
    LogicCertifyResult logic_result;
    
    int max_iterations = N * N * 10;  // Zabezpieczenie przed pętlą
    int iterations = 0;
    
    while (!board.is_full() && iterations < max_iterations) {
        ApplyResult r = strategy.apply_all(board, logic_result);
        if (r == ApplyResult::Contradiction) {
            break;
        }
        if (r == ApplyResult::NoProgress) {
            break;  // Konieczne backtracking
        }
        ++iterations;
    }

    bool solved = board.is_full();
    bool valid = solved && validate_solved_board<N>(board, topo);
    bool match_solution = true;
    if (valid) {
        for (int i = 0; i < N * N; ++i) {
            if (board.values[static_cast<size_t>(i)] != solution[static_cast<size_t>(i)]) {
                match_solution = false;
                break;
            }
        }
    } else {
        match_solution = false;
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.passed = valid && match_solution;
    result.message = valid ? 
        ("Solved in " + std::to_string(iterations) + " iterations") : 
        (solved ? "Solved but invalid" : "Not solved");
    return result;
}

// ============================================================================
// URUCHOMIENIE WSZYSTKICH TESTÓW POZIOMU 1
// ============================================================================

inline void run_level1_asymmetric_tests(
    const std::string& report_path = "level1_asymmetric_report.txt",
    int timeout_minutes = -1,
    int case_timeout_s = -1) {
    std::cerr << " Starting Level 1 Asymmetric Regression Tests..." << std::endl;
    std::cerr << " Report path: " << report_path << std::endl;
    const std::string progress_path = level1_progress_path_from_report(report_path);
    std::cerr << " Progress path: " << progress_path << std::endl;

    std::ofstream report(report_path);
    if (!report) {
        std::cerr << " Cannot open: " << report_path << std::endl;
        report.open("level1_asymmetric_report.txt");
        if (!report) {
            std::cerr << " Cannot open level1_asymmetric_report.txt either!" << std::endl;
            return;
        }
    }
    report << std::unitbuf;
    std::ofstream progress(progress_path, std::ios::out | std::ios::trunc);
    if (!progress) {
        std::cerr << " Cannot open progress file: " << progress_path << std::endl;
    } else {
        progress << std::unitbuf;
    }

    std::atomic<int> total_tests{0};
    std::atomic<int> passed_tests{0};

    const auto& geometries = get_asymmetric_geometries();
    auto is_supported_test_n = [](int n) -> bool {
        return n == 6 || n == 8 || n == 10 || n == 12 || n == 15 || n == 16 || n == 18 || n == 20;
    };
    std::vector<const AsymmetricGeometry*> tested_geometries;
    tested_geometries.reserve(geometries.size());
    for (const auto& g : geometries) {
        if (is_supported_test_n(g.N)) {
            tested_geometries.push_back(&g);
        }
    }
    std::sort(tested_geometries.begin(), tested_geometries.end(), [](const AsymmetricGeometry* a, const AsymmetricGeometry* b) {
        if (a->N != b->N) return a->N < b->N;
        if (a->box_rows != b->box_rows) return a->box_rows < b->box_rows;
        return a->box_cols < b->box_cols;
    });
    const int planned_tests = 1 + static_cast<int>(tested_geometries.size()) * 4;
    int completed_tests = 0;
    const auto suite_start = std::chrono::steady_clock::now();
    const int timeout_s = level1_timeout_seconds_from_minutes_or_env(timeout_minutes);
    const int configured_case_timeout_s = level1_case_timeout_seconds_from_arg_or_env(case_timeout_s);
    const bool timeout_enabled = timeout_s > 0;
    const auto deadline = suite_start + std::chrono::seconds(timeout_s > 0 ? timeout_s : 0);

    auto elapsed_ms = [&]() -> double {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - suite_start).count();
    };
    auto timeout_reached = [&]() -> bool {
        return timeout_enabled && std::chrono::steady_clock::now() >= deadline;
    };
    auto progress_log = [&](const std::string& line) {
        if (!progress) {
            return;
        }
        progress << line << "\n";
        progress.flush();
    };
    auto mark_start = [&](const std::string& stage, const std::string& geo, const std::string& name) {
        progress_log(
            "START [" + std::to_string(completed_tests + 1) + "/" + std::to_string(planned_tests) + "]"
            " stage=" + stage + " geo=" + geo + " test=" + name +
            " elapsed_ms=" + std::to_string(static_cast<long long>(elapsed_ms())));
    };
    auto mark_done = [&](const std::string& stage, const std::string& geo, const TestResult& r) {
        progress_log(
            "DONE  [" + std::to_string(completed_tests) + "/" + std::to_string(planned_tests) + "]"
            " stage=" + stage + " geo=" + geo +
            " result=" + std::string(r.passed ? "PASS" : "FAIL") +
            " msg=\"" + r.message + "\"" +
            " case_ms=" + std::to_string(r.elapsed_ms) +
            " elapsed_ms=" + std::to_string(static_cast<long long>(elapsed_ms())));
    };
    auto effective_case_timeout_s = [&]() -> int {
        int out = configured_case_timeout_s;
        if (out <= 0) {
            return 0;
        }
        if (timeout_enabled) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return 0;
            }
            const int remain_s = static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(deadline - now).count());
            out = std::max(1, std::min(out, remain_s));
        }
        return out;
    };
    auto run_case_with_hard_timeout = [&](const std::string& stage,
                                          const std::string& geo,
                                          const std::string& case_name,
                                          auto fn) -> TestResult {
        const int hard_limit_s = effective_case_timeout_s();
        if (hard_limit_s <= 0) {
            TestResult tr;
            tr.passed = false;
            tr.name = case_name;
            tr.message = "SKIPPED: hard timeout budget exhausted";
            tr.elapsed_ms = 0.0;
            return tr;
        }
        std::promise<TestResult> promise;
        std::future<TestResult> future = promise.get_future();
        std::thread([p = std::move(promise), fn = std::move(fn), case_name]() mutable {
            try {
                TestResult r = fn();
                if (r.name.empty()) {
                    r.name = case_name;
                }
                p.set_value(std::move(r));
            } catch (const std::exception& e) {
                TestResult r;
                r.passed = false;
                r.name = case_name;
                r.message = std::string("Exception: ") + e.what();
                p.set_value(std::move(r));
            } catch (...) {
                TestResult r;
                r.passed = false;
                r.name = case_name;
                r.message = "Unknown exception";
                p.set_value(std::move(r));
            }
        }).detach();

        if (future.wait_for(std::chrono::seconds(hard_limit_s)) == std::future_status::ready) {
            return future.get();
        }

        TestResult timeout_result;
        timeout_result.passed = false;
        timeout_result.name = case_name;
        timeout_result.message = "HARD_TIMEOUT per-case (" + std::to_string(hard_limit_s) + "s)";
        timeout_result.elapsed_ms = static_cast<double>(hard_limit_s) * 1000.0;
        progress_log("CASE_TIMEOUT stage=" + stage + " geo=" + geo + " test=" + case_name +
                     " limit_s=" + std::to_string(hard_limit_s));
        return timeout_result;
    };
    bool timed_out = false;
    
    report << "ASYMMETRIC GEOMETRY TEST SUITE - LEVEL 1 STRATEGIES\n";
    report << "===================================================\n\n";
    report << "Timeout limit: " << (timeout_enabled ? (std::to_string(timeout_s) + "s") : std::string("none")) << "\n";
    report << "Per-case hard timeout: " << (configured_case_timeout_s > 0 ? (std::to_string(configured_case_timeout_s) + "s") : std::string("off")) << "\n";
    report << "Order: rosnaco po N (male -> duze)\n";
    report << "Progress file: " << progress_path << "\n\n";
    report << "Testing " << geometries.size() << " asymmetric geometries:\n";
    for (const auto& g : geometries) {
        report << "  - " << g.desc << " (seed=" << g.test_seed << ")\n";
    }
    report << "Executed geometries in this suite: " << tested_geometries.size() << "\n";
    report << "\n";

    progress_log("LEVEL1_ASYM_PROGRESS");
    progress_log("report_path=" + report_path);
    progress_log("timeout_limit_s=" + std::string(timeout_enabled ? std::to_string(timeout_s) : "none"));
    progress_log("case_hard_timeout_s=" + std::string(configured_case_timeout_s > 0 ? std::to_string(configured_case_timeout_s) : "off"));
    progress_log("planned_tests=" + std::to_string(planned_tests));

    // TEST 0: GEOMETRY CATALOG RECOGNITION
    report << "=== TEST 0: GEOMETRY CATALOG RECOGNITION ===\n\n";
    {
        if (timeout_reached()) {
            timed_out = true;
            progress_log("TIMEOUT reached before TEST 0");
        }
        if (!timed_out) {
            mark_start("GeometryCatalog", "catalog", "GeometryCatalogRecognition");
        }
        TestResult r = run_case_with_hard_timeout("GeometryCatalog", "catalog", "GeometryCatalogRecognition", [] {
            return test_geometry_recognition_catalog();
        });
        if (r.passed) {
            ++passed_tests;
        }
        ++total_tests;
        ++completed_tests;
        report << " " << r.name << "\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n\n";
        mark_done("GeometryCatalog", "catalog", r);
    }

    // TEST 1: GENERATION
    report << "=== TEST 1: ASYMMETRIC GENERATION ===\n\n";
    for (const auto* geo_ptr : tested_geometries) {
        const auto& geo = *geo_ptr;
        if (timeout_reached()) {
            timed_out = true;
            progress_log("TIMEOUT reached at TEST 1 before geo=" + geo.desc);
            break;
        }
        mark_start("Generation", geo.desc, "AsymmetricGen");
        TestResult r;
        if (geo.N == 6)      r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<6>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 8) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<8>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 10) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<10>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 12) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<12>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 15) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<15>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 16) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<16>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 18) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<18>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 20) r = run_case_with_hard_timeout("Generation", geo.desc, "AsymmetricGen", [&]() { return test_asymmetric_generation<20>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else continue;

        if (r.passed) ++passed_tests;
        ++total_tests;
        ++completed_tests;

        report << " " << r.name << " (" << geo.desc << ")\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
        mark_done("Generation", geo.desc, r);
    }

    // TEST 2: NAKED SINGLE
    report << "\n=== TEST 2: NAKED SINGLE ON ASYMMETRIC ===\n\n";
    if (!timed_out) for (const auto* geo_ptr : tested_geometries) {
        const auto& geo = *geo_ptr;
        if (timeout_reached()) {
            timed_out = true;
            progress_log("TIMEOUT reached at TEST 2 before geo=" + geo.desc);
            break;
        }
        mark_start("NakedSingle", geo.desc, "NakedSingle");
        TestResult r;
        if (geo.N == 6)      r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<6>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 8) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<8>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 10) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<10>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 12) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<12>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 15) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<15>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 16) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<16>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 18) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<18>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 20) r = run_case_with_hard_timeout("NakedSingle", geo.desc, "NakedSingle", [&]() { return test_naked_single_asymmetric<20>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else continue;

        if (r.passed) ++passed_tests;
        ++total_tests;
        ++completed_tests;

        report << " " << r.name << " (" << geo.desc << ")\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
        mark_done("NakedSingle", geo.desc, r);
    }

    // TEST 3: HIDDEN SINGLE
    report << "\n=== TEST 3: HIDDEN SINGLE ON ASYMMETRIC ===\n\n";
    if (!timed_out) for (const auto* geo_ptr : tested_geometries) {
        const auto& geo = *geo_ptr;
        if (timeout_reached()) {
            timed_out = true;
            progress_log("TIMEOUT reached at TEST 3 before geo=" + geo.desc);
            break;
        }
        mark_start("HiddenSingle", geo.desc, "HiddenSingle");
        TestResult r;
        if (geo.N == 6)      r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<6>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 8) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<8>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 10) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<10>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 12) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<12>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 15) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<15>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 16) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<16>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 18) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<18>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 20) r = run_case_with_hard_timeout("HiddenSingle", geo.desc, "HiddenSingle", [&]() { return test_hidden_single_asymmetric<20>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else continue;

        if (r.passed) ++passed_tests;
        ++total_tests;
        ++completed_tests;

        report << " " << r.name << " (" << geo.desc << ")\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
        mark_done("HiddenSingle", geo.desc, r);
    }

    // TEST 4: FULL SOLVE
    report << "\n=== TEST 4: LEVEL 1 FULL SOLVE ===\n\n";
    if (!timed_out) for (const auto* geo_ptr : tested_geometries) {
        const auto& geo = *geo_ptr;
        if (timeout_reached()) {
            timed_out = true;
            progress_log("TIMEOUT reached at TEST 4 before geo=" + geo.desc);
            break;
        }
        mark_start("Level1Full", geo.desc, "Level1Full");
        TestResult r;
        if (geo.N == 6)      r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<6>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 8) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<8>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 10) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<10>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 12) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<12>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 15) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<15>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 16) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<16>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 18) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<18>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else if (geo.N == 20) r = run_case_with_hard_timeout("Level1Full", geo.desc, "Level1Full", [&]() { return test_level1_full_solve<20>(geo.box_rows, geo.box_cols, geo.test_seed); });
        else continue;

        if (r.passed) ++passed_tests;
        ++total_tests;
        ++completed_tests;

        report << " " << r.name << " (" << geo.desc << ")\n";
        report << "  " << r.message << " (" << std::fixed << std::setprecision(2) << r.elapsed_ms << "ms)\n";
        mark_done("Level1Full", geo.desc, r);
    }

    // SUMMARY
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
    if (timed_out) {
        report << "Timeout reached: yes\n";
    } else {
        report << "Timeout reached: no\n";
    }

    const bool all_passed = (passed_tests.load() == total_tests.load());
    if (timed_out) {
        report << "\nTEST STOPPED BY TIME LIMIT\n";
    } else {
        report << "\n" << (all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    }

    report.close();
    progress_log("FINISH completed=" + std::to_string(completed_tests) + "/" + std::to_string(planned_tests) +
                 " passed=" + std::to_string(passed_tests.load()) +
                 " failed=" + std::to_string(total_tests.load() - passed_tests.load()) +
                 " timeout=" + std::string(timed_out ? "1" : "0"));
    if (progress) {
        progress.close();
    }

    std::cout << "\n=== LEVEL 1 ASYMMETRIC TEST SUMMARY ===\n";
    std::cout << "Total: " << total_tests.load()
              << ", Passed: " << passed_tests.load()
              << ", Failed: " << (total_tests.load() - passed_tests.load()) << "\n";
    std::cout << "Report: " << report_path << "\n";
    std::cout << "Progress: " << progress_path << "\n";
    std::cout << "Timeout limit: " << (timeout_enabled ? (std::to_string(timeout_s) + "s") : std::string("none")) << "\n";
    std::cout << "Per-case hard timeout: " << (configured_case_timeout_s > 0 ? (std::to_string(configured_case_timeout_s) + "s") : std::string("off")) << "\n";
}

} // namespace sudoku_testy

#endif // STRATEGIE_LEVEL1_TESTS_H

