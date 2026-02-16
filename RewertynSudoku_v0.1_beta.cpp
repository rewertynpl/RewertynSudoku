
// author copyright Marcin Matysek (rewertynPL)

//g++ RewertynSudoku_v0.1_beta.cpp -o RewertynSudoku_v0.1_beta.exe -O3 -std=c++20 -static -pthread -march=native -flto -mpopcnt -lole32 -lshell32 -luuid -lgdi32

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")

namespace fs = std::filesystem;

static constexpr int kNumStrategies = 64;

#define FOR_EACH_BIT(mask, digit) \
    for (uint64_t _tmp_m_ = (mask); _tmp_m_; _tmp_m_ &= (_tmp_m_ - 1ULL)) \
        if (const int digit = __builtin_ctzll(_tmp_m_) + 1; true)

struct Cell { int value = 0; bool revealed = false; };
struct SudokuBoard {
    long long seed = 0;
    int block_rows = 0, block_cols = 0, side_size = 0, total_cells = 0;
    std::vector<Cell> cells;
    bool valid = false;
    std::string error;
};

enum class Strategy {
    NakedSingle, HiddenSingle,
    NakedPair, HiddenPair,
    PointingPairsTriples, BoxLineReduction,
    NakedTriple, HiddenTriple,
    NakedQuad, HiddenQuad,
    XWing, YWing, XYZWing, WXYZWing, Swordfish, Jellyfish, FrankenMutantFish, KrakenFish,
    Skyscraper, TwoStringKite, SimpleColoring, ThreeDMedusa, FinnedXWingSashimi, FinnedSwordfish, FinnedJellyfish, EmptyRectangle,
    UniqueRectangle, UniqueLoop, BivalueOddagon, AvoidableRectangle, BUGPlus1,
    RemotePairs, WWing, GroupedXCycle, XChain, XYChain, GroupedAIC, AIC, ContinuousNiceLoop,
    ALSXZ, ALSXYWing, ALSChain, DeathBlossom, SueDeCoq, MSLS, Exocet, SeniorExocet, SKLoop, PatternOverlayMethod,
    ForcingChains, Backtracking
};

struct AnalysisReport {
    bool contradiction = false;
    bool solved_logically = false;
    bool requires_guessing = false;
    bool solved_with_backtracking = false;
    bool unique_solution = false;
    int solution_count = 0;
    int initial_clues = 0;
    int hardest_rank = 0;
    long long backtracking_nodes = 0;
    long long backtracking_decisions = 0;
    long long backtracking_backtracks = 0;
    int strategy_usage[kNumStrategies] = {};
    std::string hardest_strategy;
    std::vector<std::string> debug_logic_logs;
};

struct PuzzleReportEntry {
    std::string source_file;
    int line_no = 0;
    bool valid = false;
    std::string sudoku_type = "Nieznany";
    std::string board_type = "Nieznany";
    std::string parse_error;
    int initial_clues = 0;
    int difficulty_level = 0;
    bool solved_logically = false;
    bool requires_guessing = false;
    bool solved_with_backtracking = false;
    bool contradiction = false;
    int solution_count = 0;
    long long backtracking_nodes = 0;
    long long backtracking_decisions = 0;
    long long backtracking_backtracks = 0;
    int strategy_usage[kNumStrategies] = {};
    std::string hardest_strategy = "Brak";
    std::vector<std::string> debug_logic_logs;
};

struct FolderStats {
    fs::path relative_folder;
    long long non_empty_lines = 0;
    long long invalid_lines = 0;
    long long analyzed_puzzles = 0;
    long long contradictions = 0;
    long long solved_logically = 0;
    long long requires_guessing = 0;
    long long solved_with_backtracking = 0;
    long long unique_solutions = 0;
    long long multiple_solutions = 0;
    long long no_solution = 0;
    long long backtracking_nodes_sum = 0;
    long long backtracking_decisions_sum = 0;
    long long backtracking_backtracks_sum = 0;
    long long clues_sum = 0;
    long long difficulty_sum = 0;
    long long difficulty_count = 0;
    int max_difficulty = 0;
    int hardest_rank_seen = 0;
    std::string hardest_name_seen = "Brak";
    long long strategy_usage[kNumStrategies] = {};
    std::map<std::string, long long> hardest_histogram;
    std::vector<PuzzleReportEntry> puzzle_reports;
};

struct PuzzleTask {
    std::string folder_key;
    fs::path relative_folder;
    std::string source_file;
    std::string source_path;
    int line_no = 0;
    std::string clean_line;
};

struct PuzzleResult {
    bool processed = false;
    std::string folder_key;
    fs::path relative_folder;
    std::string source_file;
    std::string source_path;
    int line_no = 0;
    bool valid = false;
    std::string error;
    SudokuBoard board;
    AnalysisReport report;
};

class PuzzleResultQueue {
public:
    explicit PuzzleResultQueue(std::size_t capacity)
        : capacity_(std::max<std::size_t>(1, capacity)) {}

    bool push(PuzzleResult&& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_full_.wait(lock, [&]() { return closed_ || queue_.size() < capacity_; });
        if (closed_) return false;
        queue_.push_back(std::move(value));
        cv_not_empty_.notify_one();
        return true;
    }

    bool pop(PuzzleResult& out) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_empty_.wait(lock, [&]() { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        cv_not_full_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_not_full_.notify_all();
        cv_not_empty_.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

private:
    std::size_t capacity_ = 1;
    mutable std::mutex mtx_;
    std::condition_variable cv_not_full_;
    std::condition_variable cv_not_empty_;
    std::deque<PuzzleResult> queue_;
    bool closed_ = false;
};

class TextLineQueue {
public:
    explicit TextLineQueue(std::size_t capacity)
        : capacity_(std::max<std::size_t>(1, capacity)) {}

    bool push(std::string&& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_full_.wait(lock, [&]() { return closed_ || queue_.size() < capacity_; });
        if (closed_) return false;
        queue_.push_back(std::move(value));
        cv_not_empty_.notify_one();
        return true;
    }

    bool pop(std::string& out) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_empty_.wait(lock, [&]() { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        cv_not_full_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_not_full_.notify_all();
        cv_not_empty_.notify_all();
    }

private:
    std::size_t capacity_ = 1;
    std::mutex mtx_;
    std::condition_variable cv_not_full_;
    std::condition_variable cv_not_empty_;
    std::deque<std::string> queue_;
    bool closed_ = false;
};

struct GeneratedOutputItem {
    std::size_t index = 0; // 1-based file index: <basename>_1<ext> ... <basename>_N<ext>
    std::string line;
};

static constexpr int kDifficultyMinLevel = 1;
static constexpr int kDifficultyMaxLevel = 9;

struct ClueRange {
    int min_clues = 0;
    int max_clues = 0;
};

class GeneratedOutputQueue {
public:
    enum class PushWaitResult {
        Pushed,
        Timeout,
        Closed
    };

    explicit GeneratedOutputQueue(std::size_t capacity)
        : capacity_(std::max<std::size_t>(1, capacity)) {}

    bool push(GeneratedOutputItem&& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_full_.wait(lock, [&]() { return closed_ || queue_.size() < capacity_; });
        if (closed_) return false;
        queue_.push_back(std::move(value));
        cv_not_empty_.notify_one();
        return true;
    }

    PushWaitResult push_for(GeneratedOutputItem&& value, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        const bool ready = cv_not_full_.wait_for(lock, timeout, [&]() { return closed_ || queue_.size() < capacity_; });
        if (!ready) {
            return PushWaitResult::Timeout;
        }
        if (closed_) {
            return PushWaitResult::Closed;
        }
        queue_.push_back(std::move(value));
        cv_not_empty_.notify_one();
        return PushWaitResult::Pushed;
    }

    bool pop(GeneratedOutputItem& out) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_empty_.wait(lock, [&]() { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        cv_not_full_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_not_full_.notify_all();
        cv_not_empty_.notify_all();
    }

private:
    std::size_t capacity_ = 1;
    std::mutex mtx_;
    std::condition_variable cv_not_full_;
    std::condition_variable cv_not_empty_;
    std::deque<GeneratedOutputItem> queue_;
    bool closed_ = false;
};

struct GenerateRunConfig {
    int box_rows = 3;
    int box_cols = 3;
    long long target_puzzles = 100;
    int min_clues = 24;
    int max_clues = 40;
    int difficulty_required = kDifficultyMaxLevel;
    bool symmetry_center = false;
    bool require_unique = true;
    std::optional<Strategy> required_strategy;
    std::string required_strategy_text;
    std::size_t explicit_threads = 0;
    long long seed = 0;
    int reseed_interval_seconds = 0;
    int attempt_time_budget_s = 0;       // 0=auto
    int attempt_node_budget_s = 0;       // 0=auto (sekundy, mapowane na budget nodow)
    long long max_attempts = 0;
    fs::path output_folder = fs::path("generated_sudoku_files");
    fs::path output_file = fs::path("generated_sudoku.txt");
};

struct GenerateRunResult {
    int return_code = 0;
    std::size_t accepted = 0;
    std::size_t written = 0;
    long long attempts = 0;
    std::size_t rejected_at_verification = 0;
    double elapsed_seconds = 0.0;
};

static thread_local bool g_generationAttemptLimitsEnabled = false;
static thread_local std::chrono::steady_clock::time_point g_generationAttemptDeadline{};
static thread_local long long g_generationAttemptNodeBudget = 0LL;

static bool generationAttemptDeadlineReached() {
    return g_generationAttemptLimitsEnabled && std::chrono::steady_clock::now() >= g_generationAttemptDeadline;
}

static bool generationAttemptNodeBudgetReached(long long usedNodes) {
    return g_generationAttemptLimitsEnabled && g_generationAttemptNodeBudget > 0LL
        && usedNodes >= g_generationAttemptNodeBudget;
}

class GenerationAttemptLimitScope {
public:
    GenerationAttemptLimitScope(std::chrono::milliseconds timeBudget, long long nodeBudget)
        : prevEnabled_(g_generationAttemptLimitsEnabled),
          prevDeadline_(g_generationAttemptDeadline),
          prevNodeBudget_(g_generationAttemptNodeBudget) {
        g_generationAttemptLimitsEnabled = true;
        g_generationAttemptDeadline = std::chrono::steady_clock::now() + timeBudget;
        g_generationAttemptNodeBudget = nodeBudget;
    }

    ~GenerationAttemptLimitScope() {
        g_generationAttemptLimitsEnabled = prevEnabled_;
        g_generationAttemptDeadline = prevDeadline_;
        g_generationAttemptNodeBudget = prevNodeBudget_;
    }

private:
    bool prevEnabled_ = false;
    std::chrono::steady_clock::time_point prevDeadline_{};
    long long prevNodeBudget_ = 0LL;
};

static std::string trim(const std::string& s) {
    const std::string ws = " \t\r\n";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static bool parseIntStrict(const std::string& t, int& out) {
    try {
        std::size_t p = 0;
        const int v = std::stoi(t, &p);
        if (p != t.size()) return false;
        out = v;
        return true;
    } catch (...) { return false; }
}

static bool parseLLStrict(const std::string& t, long long& out) {
    try {
        std::size_t p = 0;
        const long long v = std::stoll(t, &p);
        if (p != t.size()) return false;
        out = v;
        return true;
    } catch (...) { return false; }
}

static int bits(uint64_t m) { return __builtin_popcountll(m); }
static int firstDigit(uint64_t m) { return m ? (__builtin_ctzll(m) + 1) : 0; }
static int maskMinDigit(uint64_t m) { return m ? (__builtin_ctzll(m) + 1) : 0; }
static int digitsToArray(uint64_t m, int* out) {
    int cnt = 0;
    while (m) { out[cnt++] = __builtin_ctzll(m) + 1; m &= m - 1ULL; }
    return cnt;
}
static std::vector<int> digitsFromMask(uint64_t m) {
    std::vector<int> out;
    while (m) { out.push_back(__builtin_ctzll(m) + 1); m &= m - 1ULL; }
    return out;
}

static std::string strategyName(Strategy s) {
    switch (s) {
        case Strategy::NakedSingle: return "Naked Single";
        case Strategy::HiddenSingle: return "Hidden Single";
        case Strategy::NakedPair: return "Naked Pair";
        case Strategy::HiddenPair: return "Hidden Pair";
        case Strategy::PointingPairsTriples: return "Intersection Removal (Pointing Pairs/Triples)";
        case Strategy::BoxLineReduction: return "Intersection Removal (Box/Line Reduction)";
        case Strategy::NakedTriple: return "Naked Triple";
        case Strategy::HiddenTriple: return "Hidden Triple";
        case Strategy::NakedQuad: return "Naked Quad";
        case Strategy::HiddenQuad: return "Hidden Quad";
        case Strategy::XWing: return "X-Wing";
        case Strategy::YWing: return "Y-Wing (XY-Wing)";
        case Strategy::XYZWing: return "XYZ-Wing";
        case Strategy::WXYZWing: return "WXYZ-Wing";
        case Strategy::Swordfish: return "Swordfish";
        case Strategy::Jellyfish: return "Jellyfish";
        case Strategy::FrankenMutantFish: return "Franken / Mutant Fish";
        case Strategy::KrakenFish: return "Kraken Fish";
        case Strategy::Skyscraper: return "Skyscraper";
        case Strategy::TwoStringKite: return "2-String Kite";
        case Strategy::SimpleColoring: return "Simple Coloring";
        case Strategy::ThreeDMedusa: return "3D Medusa";
        case Strategy::FinnedXWingSashimi: return "Finned X-Wing / Sashimi X-Wing";
        case Strategy::FinnedSwordfish: return "Finned Swordfish";
        case Strategy::FinnedJellyfish: return "Finned Jellyfish";
        case Strategy::EmptyRectangle: return "Empty Rectangle";
        case Strategy::UniqueRectangle: return "Unique Rectangle";
        case Strategy::UniqueLoop: return "Unique Loop (6+)";
        case Strategy::BivalueOddagon: return "Bivalue Oddagon";
        case Strategy::AvoidableRectangle: return "Avoidable Rectangle";
        case Strategy::BUGPlus1: return "BUG+1";
        case Strategy::RemotePairs: return "Remote Pairs";
        case Strategy::WWing: return "W-Wing";
        case Strategy::GroupedXCycle: return "Grouped X-Cycle";
        case Strategy::XChain: return "X-Chain";
        case Strategy::XYChain: return "XY-Chain";
        case Strategy::GroupedAIC: return "Grouped AIC";
        case Strategy::AIC: return "AIC (Alternating Inference Chains)";
        case Strategy::ContinuousNiceLoop: return "Continuous Nice Loop";
        case Strategy::ALSXZ: return "ALS (Rule XZ)";
        case Strategy::ALSXYWing: return "ALS-XY-Wing";
        case Strategy::ALSChain: return "ALS-Chain";
        case Strategy::DeathBlossom: return "Death Blossom";
        case Strategy::SueDeCoq: return "Sue de Coq";
        case Strategy::MSLS: return "MSLS (Multi-Sector Locked Sets)";
        case Strategy::Exocet: return "Exocet";
        case Strategy::SeniorExocet: return "Senior Exocet";
        case Strategy::SKLoop: return "SK Loop";
        case Strategy::PatternOverlayMethod: return "Pattern Overlay Method";
        case Strategy::ForcingChains: return "Forcing Chains";
        case Strategy::Backtracking: return "Backtracking";
    }
    return "Unknown";
}

static int strategyRank(Strategy s) {
    switch (s) {
        case Strategy::NakedSingle: return 1;
        case Strategy::HiddenSingle: return 1;
        case Strategy::PointingPairsTriples: return 2;
        case Strategy::BoxLineReduction: return 2;
        case Strategy::NakedPair: return 3;
        case Strategy::HiddenPair: return 3;
        case Strategy::NakedTriple: return 3;
        case Strategy::HiddenTriple: return 3;
        case Strategy::NakedQuad: return 4;
        case Strategy::HiddenQuad: return 4;
        case Strategy::XWing: return 4;
        case Strategy::YWing: return 4;
        case Strategy::Skyscraper: return 4;
        case Strategy::TwoStringKite: return 4;
        case Strategy::EmptyRectangle: return 4;
        case Strategy::RemotePairs: return 4;
        case Strategy::Swordfish: return 5;
        case Strategy::XYZWing: return 5;
        case Strategy::FinnedXWingSashimi: return 5;
        case Strategy::UniqueRectangle: return 5;
        case Strategy::BUGPlus1: return 5;
        case Strategy::WWing: return 5;
        case Strategy::SimpleColoring: return 5;
        case Strategy::Jellyfish: return 6;
        case Strategy::WXYZWing: return 6;
        case Strategy::FinnedSwordfish: return 6;
        case Strategy::FinnedJellyfish: return 6;
        case Strategy::XChain: return 6;
        case Strategy::XYChain: return 6;
        case Strategy::ALSXZ: return 6;
        case Strategy::UniqueLoop: return 6;
        case Strategy::AvoidableRectangle: return 6;
        case Strategy::BivalueOddagon: return 6;
        case Strategy::ThreeDMedusa: return 7;
        case Strategy::GroupedAIC: return 7;
        case Strategy::AIC: return 7;
        case Strategy::GroupedXCycle: return 7;
        case Strategy::ContinuousNiceLoop: return 7;
        case Strategy::ALSXYWing: return 7;
        case Strategy::ALSChain: return 7;
        case Strategy::SueDeCoq: return 7;
        case Strategy::DeathBlossom: return 7;
        case Strategy::FrankenMutantFish: return 7;
        case Strategy::KrakenFish: return 7;
        case Strategy::MSLS: return 8;
        case Strategy::Exocet: return 8;
        case Strategy::SeniorExocet: return 8;
        case Strategy::SKLoop: return 8;
        case Strategy::PatternOverlayMethod: return 8;
        case Strategy::ForcingChains: return 8;
        case Strategy::Backtracking: return 9;
    }
    return 0;
}

static std::string strategyImplementationStatus(Strategy s) {
    switch (s) {
        case Strategy::NakedSingle:
        case Strategy::HiddenSingle:
        case Strategy::NakedPair:
        case Strategy::HiddenPair:
        case Strategy::PointingPairsTriples:
        case Strategy::BoxLineReduction:
        case Strategy::NakedTriple:
        case Strategy::HiddenTriple:
        case Strategy::NakedQuad:
        case Strategy::HiddenQuad:
        case Strategy::XWing:
        case Strategy::YWing:
        case Strategy::XYZWing:
        case Strategy::WXYZWing:
        case Strategy::Swordfish:
        case Strategy::Jellyfish:
        case Strategy::FrankenMutantFish:
        case Strategy::KrakenFish:
        case Strategy::Skyscraper:
        case Strategy::TwoStringKite:
        case Strategy::SimpleColoring:
        case Strategy::ThreeDMedusa:
        case Strategy::EmptyRectangle:
        case Strategy::BUGPlus1:
        case Strategy::RemotePairs:
        case Strategy::WWing:
        case Strategy::AvoidableRectangle:
        case Strategy::GroupedXCycle:
        case Strategy::XChain:
        case Strategy::XYChain:
        case Strategy::AIC:
        case Strategy::ContinuousNiceLoop:
        case Strategy::ALSXZ:
        case Strategy::ALSXYWing:
        case Strategy::ALSChain:
        case Strategy::DeathBlossom:
        case Strategy::SueDeCoq:
        case Strategy::MSLS:
        case Strategy::SeniorExocet:
        case Strategy::SKLoop:
        case Strategy::PatternOverlayMethod:
        case Strategy::Backtracking:
            return "ZAIMPLEMENTOWANE";
        case Strategy::UniqueRectangle:
            return "ZAIMPLEMENTOWANE (Type 1/2/3/4/5/6 + Hidden UR, konserwatywne)";
        case Strategy::UniqueLoop:
        case Strategy::BivalueOddagon:
            return "ZAIMPLEMENTOWANE (konserwatywne, single-extra)";
        case Strategy::FinnedXWingSashimi:
            return "CZESCIOWO (finned core)";
        case Strategy::FinnedSwordfish:
        case Strategy::FinnedJellyfish:
            return "CZESCIOWO (finned fish, konserwatywne)";
        case Strategy::GroupedAIC:
            return "ZAIMPLEMENTOWANE (implikacyjne, bez backtrackingu)";
        case Strategy::Exocet:
            return "CZESCIOWO (target-check, bez backtrackingu)";
        case Strategy::ForcingChains:
            return "ZAIMPLEMENTOWANE (implikacyjne, bez backtrackingu)";
        default:
            return "NIEZAIMPLEMENTOWANE";
    }
}

class SudokuAnalyzer {
public:
    explicit SudokuAnalyzer(const SudokuBoard& b);
    AnalysisReport run();

private:
    const SudokuBoard& b_;
    int N_ = 0, BR_ = 0, BC_ = 0, NN_ = 0;
    uint64_t all_ = 0ULL;
    bool contradiction_ = false;
    int hardest_rank_ = 0;
    std::string hardest_name_;
    int usage_[kNumStrategies] = {};
    bool debug_logic_enabled_ = true;
    std::size_t debug_logic_limit_ = 300;
    bool debug_logic_truncated_ = false;
    std::vector<std::string> debug_logic_logs_;
    std::vector<int> grid_;
    std::vector<uint64_t> cand_;
    std::vector<std::vector<int>> houses_;
    std::vector<std::array<int, 3>> cell_houses_;
    std::vector<std::vector<int>> peers_;

    uint64_t bit(int d) const { return 1ULL << (d - 1); }
    int row(int i) const { return i / N_; }
    int col(int i) const { return i % N_; }
    int box(int r, int c) const {
        const int bpr = N_ / BC_;
        return (r / BR_) * bpr + (c / BC_);
    }
    bool solved() const;
    void buildTopology();
    bool removeCandidate(int i, int d, bool& changed);
    bool assignValue(int i, int d);
    void initCandidates();
    void use(Strategy s, int amount);
    bool applyNakedSingles(int& n);
    bool applyHiddenSingles(int& n);
    void combosRec(const std::vector<int>& src, int k, int start, std::vector<int>& cur,
                   const std::function<void(const std::vector<int>&)>& cb);
    void forEachCombo(const std::vector<int>& src, int k, const std::function<void(const std::vector<int>&)>& cb);
    bool applyNakedSubset(int k, int& n);
    bool applyHiddenSubset(int k, int& n);
    bool applyPointingPairsTriples(int& n);
    bool applyBoxLineReduction(int& n);
    bool applyFish(int size, int& n);
    bool applyKrakenFish(int& n);
    bool applyYWing(int& n);
    bool applyXYZWing(int& n);
    bool applyWXYZWing(int& n);
    bool applySkyscraper(int& n);
    bool applyTwoStringKite(int& n);
    bool applySimpleColoring(int& n);
    bool applyFinnedXWingSashimi(int& n);
    bool applyFinnedFish(int size, int& n);
    bool applyFrankenMutantFish(int size, int& n);
    bool applyEmptyRectangle(int& n);
    bool applyUniqueRectangleType1(int& n);
    bool applyUniqueRectangleType2to6(int& n);
    bool applyUniqueLoop(int& n);
    bool applyBivalueOddagon(int& n);
    bool applyAvoidableRectangle(int& n);
    bool applyBUGPlus1(int& n);
    bool applyRemotePairs(int& n);
    bool applyWWing(int& n);
    bool applyXChain(int& n);
    bool applyXYChain(int& n);
    bool applyAIC(int& n);
    bool applyContinuousNiceLoop(int& n);
    bool applyALSXZ(int& n);
    bool applyALSXYWing(int& n);
    bool applyALSChain(int& n);
    bool applyDeathBlossom(int& n);
    bool applySueDeCoq(int& n);
    bool applyMSLS(int& n);
    bool applyExocet(int& n);
    bool applySeniorExocet(int& n);
    bool applySKLoop(int& n);
    bool applyPatternOverlayMethod(int& n);
    bool applyForcingChains(int& n);
    bool applyGroupedXCycle(int& n);
    bool applyGroupedAIC(int& n);
    bool applyThreeDMedusa(int& n);
    void logicalSolve();
    bool isPeerCell(int a, int b) const;
    std::string cellName(int idx) const;
    void pushDebugLog(const std::string& line);
    bool hasLogicalSupportWithAssignments(const std::vector<std::pair<int, int>>& assignments) const;
    int cluesCount() const;
};

class BacktrackingCounter {
public:
    BacktrackingCounter(int br, int bc, int n, std::vector<int> grid);
    int countSolutions(int limit);

private:
    int BR_, BC_, N_, NN_, limit_ = 2, solutions_ = 0;
    long long nodes_ = 0;
    bool aborted_ = false;
    uint64_t all_ = 0ULL;
    std::vector<int> grid_;
    uint64_t bit(int d) const { return 1ULL << (d - 1); }
    int row(int i) const { return i / N_; }
    int col(int i) const { return i % N_; }
    int box(int r, int c) const {
        const int bpr = N_ / BC_;
        return (r / BR_) * bpr + (c / BC_);
    }
    uint64_t allowed(int idx) const;
    bool validState() const;
    void search();
};

struct BacktrackingSolveStats {
    bool solved = false;
    long long nodes = 0;
    long long decisions = 0;
    long long backtracks = 0;
};

class BacktrackingSolver {
public:
    BacktrackingSolver(int br, int bc, int n, std::vector<int> grid);
    BacktrackingSolveStats solve();

private:
    int BR_, BC_, N_, NN_;
    uint64_t all_ = 0ULL;
    std::vector<int> grid_;
    BacktrackingSolveStats stats_;
    uint64_t bit(int d) const { return 1ULL << (d - 1); }
    int row(int i) const { return i / N_; }
    int col(int i) const { return i % N_; }
    int box(int r, int c) const {
        const int bpr = N_ / BC_;
        return (r / BR_) * bpr + (c / BC_);
    }
    uint64_t allowed(int idx) const;
    bool validState() const;
    bool search();
};

static SudokuBoard parseSudokuLine(const std::string& line);
static int countSolutionsWithBacktracking(const SudokuBoard& b, int limit = 2);
static BacktrackingSolveStats solveWithBacktracking(const SudokuBoard& b, const std::vector<int>& initialGrid);
static std::string selectFolderModern();
static bool isTxtFile(const fs::path& p);
static bool isPathWithin(const fs::path& path, const fs::path& parent);
static std::vector<fs::path> collectTxtFilesRecursive(const fs::path& root, const fs::path& excludedRoot);
static long long countNonEmptyLinesInTxtFiles(const std::vector<fs::path>& files);
static std::string folderKeyFromRelativePath(const fs::path& rel);
static std::string sanitizeFileName(const std::string& name);
static std::string csvEscape(const std::string& field);
static int difficultyLevelFromReport(const AnalysisReport& report);
static std::string difficultyTypeFromReport(const AnalysisReport& report);
static ClueRange recommendedClueRangeForLevel(int sideSize, int level);
static ClueRange recommendedClueRangeForDifficultyRange(int sideSize, int difficultyMin, int difficultyMax);
static bool reportUsesStrategyAtLevel(const AnalysisReport& report, int level);
static std::string boardTypeFromBoard(const SudokuBoard& board);
static void appendInvalidPuzzleReport(FolderStats& stats, const std::string& sourceFile, int lineNo,
                                      const std::string& parseError);
static void appendValidPuzzleReport(FolderStats& stats, const std::string& sourceFile, int lineNo,
                                    const SudokuBoard& board, const AnalysisReport& report);
static void updateFolderStats(FolderStats& stats, const AnalysisReport& report);
static PuzzleResult analyzePuzzleTask(const PuzzleTask& task);
static void writeFolderReport(const fs::path& outDir, const std::string& folderKey, const FolderStats& stats);
static void writeGlobalSummary(const fs::path& outDir, const std::map<std::string, FolderStats>& allStats,
                               long long txtFilesScanned);
static void writeFolderCsv(const fs::path& outDir, const std::map<std::string, FolderStats>& allStats);
static std::size_t parseThreadOverrideFromEnv();
static std::size_t computeWorkerCount(std::size_t taskCount);
static std::size_t computeChunkSize(std::size_t workerCount);
static std::size_t computeWorkerCountWithPreferred(std::size_t taskCount, std::size_t preferred);
static std::string normalizeToken(const std::string& text);
static std::optional<Strategy> parseStrategyToken(const std::string& text);
static std::string selectOutputTxtFileDialog();
static std::string selectOutputFolderDialog();
static bool isIndexedOutputFileName(const std::string& name, const std::string& baseName, const std::string& extension);
static bool showGeneratorConfigWindow(GenerateRunConfig& cfg);
static SudokuBoard boardFromGrid(int boxRows, int boxCols, long long seed, const std::vector<int>& puzzleGrid);
static std::string puzzleLineFromPuzzleAndSolution(long long seed, int boxRows, int boxCols,
                                                   const std::vector<int>& puzzleGrid,
                                                   const std::vector<int>& solvedGrid);
static bool verifyGeneratedPuzzleLineStrict(const std::string& puzzleLine, int boxRows, int boxCols,
                                            const std::vector<int>& solvedGrid,
                                            AnalysisReport* outReport = nullptr);
static bool generateSolvedGridRandom(int boxRows, int boxCols, std::mt19937_64& rng, std::vector<int>& outGrid);
static bool isCompleteGridValid(int boxRows, int boxCols, const std::vector<int>& grid);
static bool buildPuzzleByDiggingHoles(const std::vector<int>& solvedGrid, const GenerateRunConfig& cfg,
                                      std::mt19937_64& rng, std::vector<int>& outPuzzleGrid,
                                      int minClues, int maxClues);
static bool puzzleMatchesDifficulty(const SudokuBoard& board, const GenerateRunConfig& cfg, AnalysisReport& outReport,
                                    const std::optional<Strategy>& attemptRequiredStrategy = std::nullopt,
                                    bool* outFailedRequiredStrategy = nullptr);
static GenerateRunResult runGenerateMode(const GenerateRunConfig& cfg);

SudokuAnalyzer::SudokuAnalyzer(const SudokuBoard& b) : b_(b) {
    N_ = b_.side_size;
    BR_ = b_.block_rows;
    BC_ = b_.block_cols;
    NN_ = N_ * N_;
    all_ = (N_ >= 63) ? 0ULL : ((1ULL << N_) - 1ULL);
    buildTopology();
    initCandidates();
}

int SudokuAnalyzer::cluesCount() const {
    int c = 0;
    for (const Cell& x : b_.cells) if (x.revealed) ++c;
    return c;
}

bool SudokuAnalyzer::solved() const {
    if (contradiction_) return false;
    for (int v : grid_) if (v == 0) return false;
    return true;
}

bool SudokuAnalyzer::isPeerCell(int a, int b) const {
    if (a == b) return false;
    const int ra = row(a), ca = col(a);
    const int rb = row(b), cb = col(b);
    if (ra == rb || ca == cb) return true;
    return box(ra, ca) == box(rb, cb);
}

std::string SudokuAnalyzer::cellName(int idx) const {
    std::ostringstream ss;
    ss << "r" << (row(idx) + 1) << "c" << (col(idx) + 1);
    return ss.str();
}

void SudokuAnalyzer::pushDebugLog(const std::string& line) {
    if (!debug_logic_enabled_) return;
    if (debug_logic_logs_.size() >= debug_logic_limit_) {
        if (!debug_logic_truncated_) {
            debug_logic_logs_.push_back("... log obciety (osiagnieto limit wpisow) ...");
            debug_logic_truncated_ = true;
        }
        return;
    }
    debug_logic_logs_.push_back(line);
}

bool SudokuAnalyzer::hasLogicalSupportWithAssignments(const std::vector<std::pair<int, int>>& assignments) const {
    if (generationAttemptDeadlineReached()) return false;
    std::vector<int> g = grid_;
    std::vector<uint64_t> c = cand_;
    std::vector<int> queue;
    queue.reserve(NN_);

    auto assignLocal = [&](int cell, int digit) -> bool {
        if (generationAttemptDeadlineReached()) return false;
        if (cell < 0 || cell >= NN_ || digit < 1 || digit > N_) return false;
        const uint64_t b = bit(digit);
        if (g[cell] == digit) return true;
        if (g[cell] != 0 && g[cell] != digit) return false;
        if ((c[cell] & b) == 0ULL) return false;
        g[cell] = digit;
        c[cell] = b;
        queue.push_back(cell);
        return true;
    };

    auto removeLocal = [&](int cell, int digit) -> bool {
        if (generationAttemptDeadlineReached()) return false;
        if (cell < 0 || cell >= NN_ || digit < 1 || digit > N_) return false;
        if (g[cell] != 0) return g[cell] != digit;
        const uint64_t b = bit(digit);
        if ((c[cell] & b) == 0ULL) return true;
        c[cell] &= ~b;
        if (c[cell] == 0ULL) return false;
        if (bits(c[cell]) == 1) {
            if (!assignLocal(cell, firstDigit(c[cell]))) return false;
        }
        return true;
    };

    for (const auto& asg : assignments) {
        if (!assignLocal(asg.first, asg.second)) return false;
    }

    std::size_t qHead = 0;
    while (true) {
        if (generationAttemptDeadlineReached()) return false;
        while (qHead < queue.size()) {
            if (generationAttemptDeadlineReached()) return false;
            const int cell = queue[qHead++];
            const int d = g[cell];
            if (d <= 0) return false;
            for (int p : peers_[cell]) {
                if (!removeLocal(p, d)) return false;
            }
        }

        bool pushed = false;
        for (int h = 0; h < 3 * N_; ++h) {
            if (generationAttemptDeadlineReached()) return false;
            for (int d = 1; d <= N_; ++d) {
                int solvedCnt = 0;
                int lastPos = -1;
                int places = 0;
                for (int idx : houses_[h]) {
                    if (g[idx] == d) {
                        ++solvedCnt;
                        if (solvedCnt > 1) return false;
                        continue;
                    }
                    if (g[idx] == 0 && (c[idx] & bit(d))) {
                        ++places;
                        lastPos = idx;
                    }
                }
                if (solvedCnt == 0 && places == 0) return false;
                if (solvedCnt == 0 && places == 1) {
                    const std::size_t before = queue.size();
                    if (!assignLocal(lastPos, d)) return false;
                    if (queue.size() > before) pushed = true;
                }
            }
        }

        if (qHead < queue.size()) continue;
        if (!pushed) break;
    }

    return true;
}

void SudokuAnalyzer::buildTopology() {
    // Thread-safe topology cache per (BR_, BC_)
    struct TopologyData {
        std::vector<std::vector<int>> houses;
        std::vector<std::array<int, 3>> cell_houses;
        std::vector<std::vector<int>> peers;
    };
    static std::mutex s_topoMutex;
    static std::unordered_map<int, std::shared_ptr<TopologyData>> s_topoCache;

    const int cacheKey = BR_ * 100 + BC_;
    {
        std::lock_guard<std::mutex> lock(s_topoMutex);
        auto it = s_topoCache.find(cacheKey);
        if (it != s_topoCache.end()) {
            houses_ = it->second->houses;
            cell_houses_ = it->second->cell_houses;
            peers_ = it->second->peers;
            return;
        }
    }

    houses_.assign(3 * N_, {});
    cell_houses_.assign(NN_, {0, 0, 0});
    peers_.assign(NN_, {});

    for (int r = 0; r < N_; ++r) {
        for (int c = 0; c < N_; ++c) {
            const int idx = r * N_ + c;
            houses_[r].push_back(idx);
            houses_[N_ + c].push_back(idx);
            houses_[2 * N_ + box(r, c)].push_back(idx);
        }
    }
    for (int i = 0; i < NN_; ++i) {
        const int r = row(i), c = col(i), b = box(r, c);
        cell_houses_[i] = {r, N_ + c, 2 * N_ + b};
    }
    for (int i = 0; i < NN_; ++i) {
        std::vector<char> seen(NN_, 0);
        for (int h : cell_houses_[i]) {
            for (int p : houses_[h]) {
                if (p == i || seen[p]) continue;
                seen[p] = 1;
                peers_[i].push_back(p);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_topoMutex);
        auto td = std::make_shared<TopologyData>();
        td->houses = houses_;
        td->cell_houses = cell_houses_;
        td->peers = peers_;
        s_topoCache[cacheKey] = std::move(td);
    }
}

bool SudokuAnalyzer::removeCandidate(int i, int d, bool& changed) {
    if (grid_[i] != 0) {
        if (grid_[i] == d) {
            contradiction_ = true;
            return false;
        }
        return true;
    }
    const uint64_t b = bit(d);
    if ((cand_[i] & b) == 0ULL) return true;
    const uint64_t next = cand_[i] & (~b);
    if (next == 0ULL) {
        contradiction_ = true;
        return false;
    }
    cand_[i] = next;
    changed = true;
    return true;
}

bool SudokuAnalyzer::assignValue(int i, int d) {
    if (d < 1 || d > N_) {
        contradiction_ = true;
        return false;
    }
    if (grid_[i] == d) return true;
    if (grid_[i] != 0 && grid_[i] != d) {
        contradiction_ = true;
        return false;
    }
    if ((cand_[i] & bit(d)) == 0ULL) {
        contradiction_ = true;
        return false;
    }

    grid_[i] = d;
    cand_[i] = bit(d);
    for (int p : peers_[i]) {
        if (grid_[p] == d) {
            contradiction_ = true;
            return false;
        }
        bool changed = false;
        if (!removeCandidate(p, d, changed)) return false;
    }
    return true;
}

void SudokuAnalyzer::initCandidates() {
    grid_.assign(NN_, 0);
    cand_.assign(NN_, all_);
    for (int i = 0; i < NN_; ++i) {
        if (!b_.cells[i].revealed) continue;
        if (!assignValue(i, b_.cells[i].value)) {
            contradiction_ = true;
            return;
        }
    }
}

void SudokuAnalyzer::use(Strategy s, int amount) {
    if (amount <= 0) return;
    usage_[static_cast<int>(s)] += amount;
    const int r = strategyRank(s);
    if (r > hardest_rank_) {
        hardest_rank_ = r;
        hardest_name_ = strategyName(s);
    }
}

bool SudokuAnalyzer::applyNakedSingles(int& n) {
    n = 0;
    bool changed = false;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] != 0) continue;
        if (bits(cand_[i]) != 1) continue;
        if (!assignValue(i, firstDigit(cand_[i]))) return changed;
        changed = true;
        ++n;
    }
    return changed;
}

bool SudokuAnalyzer::applyHiddenSingles(int& n) {
    n = 0;
    for (const auto& h : houses_) {
        for (int d = 1; d <= N_; ++d) {
            bool already = false;
            int count = 0, lastCell = -1;
            for (int i : h) {
                if (grid_[i] == d) { already = true; break; }
                if (grid_[i] == 0 && (cand_[i] & bit(d))) { ++count; lastCell = i; }
            }
            if (already) continue;
            if (count == 0) {
                contradiction_ = true;
                return false;
            }
            if (count == 1) {
                if (!assignValue(lastCell, d)) return false;
                n = 1;
                return true;
            }
        }
    }
    return false;
}

void SudokuAnalyzer::combosRec(const std::vector<int>& src, int k, int start, std::vector<int>& cur,
                               const std::function<void(const std::vector<int>&)>& cb) {
    if (static_cast<int>(cur.size()) == k) {
        cb(cur);
        return;
    }
    const int need = k - static_cast<int>(cur.size());
    const int lim = static_cast<int>(src.size()) - need;
    for (int i = start; i <= lim; ++i) {
        cur.push_back(src[i]);
        combosRec(src, k, i + 1, cur, cb);
        cur.pop_back();
    }
}

void SudokuAnalyzer::forEachCombo(const std::vector<int>& src, int k,
                                  const std::function<void(const std::vector<int>&)>& cb) {
    std::vector<int> cur;
    combosRec(src, k, 0, cur, cb);
}

bool SudokuAnalyzer::applyNakedSubset(int k, int& n) {
    n = 0;
    for (const auto& h : houses_) {
        std::vector<int> pool;
        for (int i : h) {
            if (grid_[i] != 0) continue;
            const int bc = bits(cand_[i]);
            if (bc >= 2 && bc <= k) pool.push_back(i);
        }
        if (static_cast<int>(pool.size()) < k) continue;

        bool found = false;
        // Naked subset: k komorek tworzy lacznie dokladnie k cyfr,
        // wiec te cyfry mozna usunac z pozostalych komorek domu.
        forEachCombo(pool, k, [&](const std::vector<int>& cset) {
            if (found || contradiction_) return;
            uint64_t uni = 0ULL;
            for (int i : cset) uni |= cand_[i];
            if (bits(uni) != k) return;

            std::vector<int> subset;
            for (int i : h) if (grid_[i] == 0 && (cand_[i] & ~uni) == 0ULL) subset.push_back(i);
            if (static_cast<int>(subset.size()) != k) return;

            std::set<int> subset_set(subset.begin(), subset.end());
            int local = 0;
            for (int i : h) {
                if (grid_[i] != 0 || subset_set.count(i)) continue;
                uint64_t m = uni;
                while (m) {
                    const uint64_t one = m & (~m + 1ULL);
                    bool changed = false;
                    if (!removeCandidate(i, firstDigit(one), changed)) return;
                    if (changed) ++local;
                    m &= (m - 1ULL);
                }
            }
            if (local > 0) {
                n = local;
                found = true;
            }
        });
        if (found) return true;
    }
    return false;
}

bool SudokuAnalyzer::applyHiddenSubset(int k, int& n) {
    n = 0;
    std::vector<int> digits;
    for (int d = 1; d <= N_; ++d) digits.push_back(d);

    for (const auto& h : houses_) {
        bool found = false;
        // Hidden subset: k cyfr wystepuje tylko w k komorkach,
        // wiec w tych komorkach usuwamy wszystkich innych kandydatow.
        forEachCombo(digits, k, [&](const std::vector<int>& dset) {
            if (found || contradiction_) return;
            uint64_t dm = 0ULL;
            for (int d : dset) dm |= bit(d);

            std::vector<int> union_cells;
            for (int i : h) if (grid_[i] == 0 && (cand_[i] & dm)) union_cells.push_back(i);
            if (static_cast<int>(union_cells.size()) != k) return;

            for (int d : dset) {
                bool present = false;
                for (int i : union_cells) if (cand_[i] & bit(d)) { present = true; break; }
                if (!present) return;
            }

            int local = 0;
            for (int i : union_cells) {
                uint64_t extra = cand_[i] & (~dm);
                while (extra) {
                    const uint64_t one = extra & (~extra + 1ULL);
                    bool changed = false;
                    if (!removeCandidate(i, firstDigit(one), changed)) return;
                    if (changed) ++local;
                    extra &= (extra - 1ULL);
                }
            }
            if (local > 0) {
                n = local;
                found = true;
            }
        });
        if (found) return true;
    }
    return false;
}

bool SudokuAnalyzer::applyPointingPairsTriples(int& n) {
    n = 0;
    for (int bh = 2 * N_; bh < 3 * N_; ++bh) {
        const auto& box_cells = houses_[bh];
        for (int d = 1; d <= N_; ++d) {
            int wCount = 0, wFirst = -1;
            int rCommon = -1, cCommon = -1;
            bool allSameRow = true, allSameCol = true;
            for (int i : box_cells) {
                if (grid_[i] == 0 && (cand_[i] & bit(d))) {
                    if (wCount == 0) {
                        wFirst = i;
                        rCommon = row(i);
                        cCommon = col(i);
                    } else {
                        if (row(i) != rCommon) allSameRow = false;
                        if (col(i) != cCommon) allSameCol = false;
                    }
                    ++wCount;
                }
            }
            if (wCount < 2) continue;

            if (allSameRow) {
                int local = 0;
                for (int i : houses_[rCommon]) {
                    if (grid_[i] != 0 || box(row(i), col(i)) == (bh - 2 * N_)) continue;
                    bool changed = false;
                    if (!removeCandidate(i, d, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }

            if (allSameCol) {
                int local = 0;
                for (int i : houses_[N_ + cCommon]) {
                    if (grid_[i] != 0 || box(row(i), col(i)) == (bh - 2 * N_)) continue;
                    bool changed = false;
                    if (!removeCandidate(i, d, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyBoxLineReduction(int& n) {
    n = 0;
    for (int r = 0; r < N_; ++r) {
        for (int d = 1; d <= N_; ++d) {
            int wCount = 0, b0 = -1;
            bool sameBox = true;
            for (int i : houses_[r]) {
                if (grid_[i] == 0 && (cand_[i] & bit(d))) {
                    const int bi = box(row(i), col(i));
                    if (wCount == 0) b0 = bi;
                    else if (bi != b0) sameBox = false;
                    ++wCount;
                }
            }
            if (wCount < 2 || !sameBox) continue;

            int local = 0;
            for (int i : houses_[2 * N_ + b0]) {
                if (grid_[i] != 0 || row(i) == r) continue;
                bool changed = false;
                if (!removeCandidate(i, d, changed)) return false;
                if (changed) ++local;
            }
            if (local > 0) { n = local; return true; }
        }
    }

    for (int c = 0; c < N_; ++c) {
        for (int d = 1; d <= N_; ++d) {
            int wCount = 0, b0 = -1;
            bool sameBox = true;
            for (int i : houses_[N_ + c]) {
                if (grid_[i] == 0 && (cand_[i] & bit(d))) {
                    const int bi = box(row(i), col(i));
                    if (wCount == 0) b0 = bi;
                    else if (bi != b0) sameBox = false;
                    ++wCount;
                }
            }
            if (wCount < 2 || !sameBox) continue;

            int local = 0;
            for (int i : houses_[2 * N_ + b0]) {
                if (grid_[i] != 0 || col(i) == c) continue;
                bool changed = false;
                if (!removeCandidate(i, d, changed)) return false;
                if (changed) ++local;
            }
            if (local > 0) { n = local; return true; }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyFish(int size, int& n) {
    n = 0;
    if (size < 2 || size > N_) return false;

    for (int d = 1; d <= N_; ++d) {
        for (int mode = 0; mode < 2; ++mode) {
            uint64_t posMask[36] = {};
            std::vector<int> eligible;
            for (int line = 0; line < N_; ++line) {
                uint64_t pm = 0ULL;
                for (int p = 0; p < N_; ++p) {
                    const int idx = (mode == 0) ? (line * N_ + p) : (p * N_ + line);
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) {
                        pm |= 1ULL << p;
                    }
                }
                posMask[line] = pm;
                const int cnt = bits(pm);
                if (cnt >= 2 && cnt <= size) {
                    eligible.push_back(line);
                }
            }
            if (static_cast<int>(eligible.size()) < size) continue;

            bool found = false;
            forEachCombo(eligible, size, [&](const std::vector<int>& lines) {
                if (found || contradiction_) return;

                uint64_t lineMask = 0ULL, unionPos = 0ULL;
                for (int line : lines) {
                    lineMask |= 1ULL << line;
                    unionPos |= posMask[line];
                }
                if (bits(unionPos) != size) return;

                int local = 0;
                FOR_EACH_BIT(unionPos, pShifted) {
                    const int p = pShifted - 1;
                    for (int line = 0; line < N_; ++line) {
                        if (lineMask & (1ULL << line)) continue;
                        const int idx = (mode == 0) ? (line * N_ + p) : (p * N_ + line);
                        bool changed = false;
                        if (!removeCandidate(idx, d, changed)) return;
                        if (changed) ++local;
                    }
                }
                if (local > 0) {
                    n = local;
                    found = true;
                }
            });
            if (found) return true;
        }
    }
    return false;
}

bool SudokuAnalyzer::applyKrakenFish(int& n) {
    n = 0;
    if (N_ != 9) return false;

    auto buildStrongAdj = [&](int d) {
        std::vector<std::vector<int>> adj(NN_);
        for (int h = 0; h < 3 * N_; ++h) {
            std::vector<int> where;
            for (int idx : houses_[h]) {
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) where.push_back(idx);
            }
            if (where.size() == 2U) {
                adj[where[0]].push_back(where[1]);
                adj[where[1]].push_back(where[0]);
            }
        }
        return adj;
    };

    for (int d = 1; d <= N_; ++d) {
        const std::vector<std::vector<int>> strongAdj = buildStrongAdj(d);
        auto victimSeesFinByChain = [&](int victim, int fin) {
            if (isPeerCell(victim, fin)) return true;
            for (int s1 : strongAdj[fin]) {
                if (isPeerCell(victim, s1)) return true;
                for (int s2 : strongAdj[s1]) {
                    if (s2 == fin) continue;
                    if (isPeerCell(victim, s2)) return true;
                }
            }
            return false;
        };

        auto process = [&](bool rowBased, int fishSize) -> bool {
            std::vector<std::vector<int>> positions(N_);
            std::vector<int> eligible;
            for (int line = 0; line < N_; ++line) {
                std::vector<int> pos;
                for (int p = 0; p < N_; ++p) {
                    const int idx = rowBased ? (line * N_ + p) : (p * N_ + line);
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) pos.push_back(p);
                }
                positions[line] = std::move(pos);
                const int cnt = static_cast<int>(positions[line].size());
                if (cnt >= 2 && cnt <= fishSize + 2) eligible.push_back(line);
            }
            if (static_cast<int>(eligible.size()) < fishSize) return false;

            bool found = false;
            forEachCombo(eligible, fishSize, [&](const std::vector<int>& lines) {
                if (found || contradiction_) return;

                std::vector<char> inLine(N_, 0), inUnionPos(N_, 0);
                std::vector<int> unionPos;
                for (int line : lines) {
                    inLine[line] = 1;
                    for (int p : positions[line]) {
                        if (!inUnionPos[p]) {
                            inUnionPos[p] = 1;
                            unionPos.push_back(p);
                        }
                    }
                }
                const int uc = static_cast<int>(unionPos.size());
                if (uc <= fishSize || uc > fishSize + 2) return;

                forEachCombo(unionPos, fishSize, [&](const std::vector<int>& coverPos) {
                    if (found || contradiction_) return;
                    std::vector<char> inCover(N_, 0);
                    for (int p : coverPos) inCover[p] = 1;

                    std::vector<int> finCells;
                    for (int line : lines) {
                        for (int p : positions[line]) {
                            if (inCover[p]) continue;
                            finCells.push_back(rowBased ? (line * N_ + p) : (p * N_ + line));
                        }
                    }
                    if (finCells.empty()) return;

                    int finBox = -1;
                    bool sameBox = true;
                    for (int f : finCells) {
                        const int b = box(row(f), col(f));
                        if (finBox < 0) finBox = b;
                        else if (finBox != b) { sameBox = false; break; }
                    }
                    if (!sameBox || finBox < 0) return;

                    int local = 0;
                    for (int p : coverPos) {
                        const std::vector<int>& coverHouse = rowBased ? houses_[N_ + p] : houses_[p];
                        for (int idx : coverHouse) {
                            const int line = rowBased ? row(idx) : col(idx);
                            if (inLine[line]) continue;
                            if (grid_[idx] != 0 || (cand_[idx] & bit(d)) == 0ULL) continue;
                            if (box(row(idx), col(idx)) != finBox) continue;

                            bool seesAll = true;
                            for (int f : finCells) {
                                if (!victimSeesFinByChain(idx, f)) { seesAll = false; break; }
                            }
                            if (!seesAll) continue;

                            bool changed = false;
                            if (!removeCandidate(idx, d, changed)) return;
                            if (changed) ++local;
                        }
                    }
                    if (local > 0) {
                        std::ostringstream ss;
                        ss << "KrakenFish(" << fishSize << "): remove " << d << " from " << local
                           << " cell(s) via fin-chain support";
                        pushDebugLog(ss.str());
                        n = local;
                        found = true;
                    }
                });
            });
            return found;
        };

        if (process(true, 2) || process(false, 2) || process(true, 3) || process(false, 3)) return true;
    }
    return false;
}

bool SudokuAnalyzer::applyFrankenMutantFish(int size, int& n) {
    n = 0;
    if (N_ != 9 || size < 2 || size > 3) return false;

    auto tryMode = [&](bool rowBase) -> bool {
        std::vector<int> basePool;
        std::vector<int> coverPool;
        for (int r = 0; r < N_; ++r) basePool.push_back(r);
        for (int c = 0; c < N_; ++c) coverPool.push_back(N_ + c);
        for (int b = 0; b < N_; ++b) {
            const int boxHouse = 2 * N_ + b;
            basePool.push_back(boxHouse);
            coverPool.push_back(boxHouse);
        }
        if (!rowBase) std::swap(basePool, coverPool);

        for (int d = 1; d <= N_; ++d) {
            std::vector<int> baseEligible;
            std::vector<int> coverEligible;
            for (int h : basePool) {
                int cnt = 0;
                for (int idx : houses_[h]) if (grid_[idx] == 0 && (cand_[idx] & bit(d))) ++cnt;
                if (cnt >= 2 && cnt <= 6) baseEligible.push_back(h);
            }
            for (int h : coverPool) {
                int cnt = 0;
                for (int idx : houses_[h]) if (grid_[idx] == 0 && (cand_[idx] & bit(d))) ++cnt;
                if (cnt >= 2 && cnt <= 6) coverEligible.push_back(h);
            }
            if (static_cast<int>(baseEligible.size()) < size || static_cast<int>(coverEligible.size()) < size) continue;

            bool found = false;
            forEachCombo(baseEligible, size, [&](const std::vector<int>& baseSets) {
                if (found || contradiction_) return;
                std::vector<char> inBase(NN_, 0);
                std::vector<int> baseCandCells;
                for (int h : baseSets) {
                    for (int idx : houses_[h]) {
                        if (grid_[idx] != 0 || (cand_[idx] & bit(d)) == 0ULL) continue;
                        if (!inBase[idx]) {
                            inBase[idx] = 1;
                            baseCandCells.push_back(idx);
                        }
                    }
                }
                if (baseCandCells.size() < static_cast<std::size_t>(size)) return;

                forEachCombo(coverEligible, size, [&](const std::vector<int>& coverSets) {
                    if (found || contradiction_) return;
                    std::vector<char> inCover(NN_, 0);
                    for (int h : coverSets) {
                        for (int idx : houses_[h]) {
                            if (grid_[idx] == 0 && (cand_[idx] & bit(d))) inCover[idx] = 1;
                        }
                    }

                    for (int idx : baseCandCells) {
                        if (!inCover[idx]) return;
                    }

                    int local = 0;
                    for (int idx = 0; idx < NN_; ++idx) {
                        if (!inCover[idx] || inBase[idx]) continue;
                        if (grid_[idx] != 0 || (cand_[idx] & bit(d)) == 0ULL) continue;
                        bool changed = false;
                        if (!removeCandidate(idx, d, changed)) return;
                        if (changed) ++local;
                    }
                    if (local > 0) {
                        std::ostringstream ss;
                        ss << "FrankenMutantFish(" << size << "): remove " << d
                           << " from " << local << " cell(s)";
                        pushDebugLog(ss.str());
                        n = local;
                        found = true;
                    }
                });
            });
            if (found) return true;
        }
        return false;
    };

    if (tryMode(true)) return true;
    return tryMode(false);
}

bool SudokuAnalyzer::applyYWing(int& n) {
    n = 0;
    std::vector<int> bivalueCells;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] == 0 && bits(cand_[i]) == 2) bivalueCells.push_back(i);
    }

    for (int pivot : bivalueCells) {
        const uint64_t pivotMask = cand_[pivot];
        const std::vector<int> pd = digitsFromMask(pivotMask);
        if (pd.size() != 2U) continue;
        const int a = pd[0], b = pd[1];

        for (int wing1 : peers_[pivot]) {
            if (grid_[wing1] != 0 || bits(cand_[wing1]) != 2) continue;
            const uint64_t m1 = cand_[wing1];
            const uint64_t common1 = m1 & pivotMask;
            if (bits(common1) != 1) continue;
            const int shared1 = firstDigit(common1);
            const int z1 = firstDigit(m1 & ~common1);
            if (z1 == 0) continue;

            const int neededShared = (shared1 == a) ? b : ((shared1 == b) ? a : 0);
            if (neededShared == 0) continue;

            for (int wing2 : peers_[pivot]) {
                if (wing2 == wing1) continue;
                if (grid_[wing2] != 0 || bits(cand_[wing2]) != 2) continue;
                const uint64_t m2 = cand_[wing2];
                const uint64_t common2 = m2 & pivotMask;
                if (bits(common2) != 1) continue;
                const int shared2 = firstDigit(common2);
                const int z2 = firstDigit(m2 & ~common2);
                if (shared2 != neededShared || z2 != z1) continue;

                int local = 0;
                for (int i = 0; i < NN_; ++i) {
                    if (i == pivot || i == wing1 || i == wing2) continue;
                    if (grid_[i] != 0 || (cand_[i] & bit(z1)) == 0ULL) continue;
                    if (!isPeerCell(i, wing1) || !isPeerCell(i, wing2)) continue;
                    bool changed = false;
                    if (!removeCandidate(i, z1, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyXYZWing(int& n) {
    n = 0;
    for (int pivot = 0; pivot < NN_; ++pivot) {
        if (grid_[pivot] != 0 || bits(cand_[pivot]) != 3) continue;
        const uint64_t pm = cand_[pivot];

        std::vector<int> wingCells;
        for (int p : peers_[pivot]) {
            if (grid_[p] != 0 || bits(cand_[p]) != 2) continue;
            if ((cand_[p] & ~pm) == 0ULL) wingCells.push_back(p);
        }
        if (wingCells.size() < 2U) continue;

        for (std::size_t i = 0; i < wingCells.size(); ++i) {
            const int w1 = wingCells[i];
            const uint64_t m1 = cand_[w1];
            for (std::size_t j = i + 1; j < wingCells.size(); ++j) {
                const int w2 = wingCells[j];
                const uint64_t m2 = cand_[w2];
                if ((m1 | m2) != pm) continue;
                const uint64_t zMask = m1 & m2;
                if (bits(zMask) != 1) continue;
                const int z = firstDigit(zMask);

                int local = 0;
                for (int c = 0; c < NN_; ++c) {
                    if (c == pivot || c == w1 || c == w2) continue;
                    if (grid_[c] != 0 || (cand_[c] & bit(z)) == 0ULL) continue;
                    if (!isPeerCell(c, pivot) || !isPeerCell(c, w1) || !isPeerCell(c, w2)) continue;
                    bool changed = false;
                    if (!removeCandidate(c, z, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyWXYZWing(int& n) {
    n = 0;

    for (int pivot = 0; pivot < NN_; ++pivot) {
        if (grid_[pivot] != 0 || bits(cand_[pivot]) != 4) continue;
        const uint64_t pm = cand_[pivot];

        std::vector<int> wings;
        for (int p : peers_[pivot]) {
            if (grid_[p] != 0) continue;
            const int bc = bits(cand_[p]);
            if (bc < 2 || bc > 3) continue;
            if ((cand_[p] & ~pm) == 0ULL) wings.push_back(p);
        }
        if (wings.size() < 3U) continue;

        for (std::size_t i = 0; i < wings.size(); ++i) {
            const int w1 = wings[i];
            for (std::size_t j = i + 1; j < wings.size(); ++j) {
                const int w2 = wings[j];
                for (std::size_t k = j + 1; k < wings.size(); ++k) {
                    const int w3 = wings[k];
                    const uint64_t um = pm | cand_[w1] | cand_[w2] | cand_[w3];
                    if (um != pm) continue;

                    uint64_t zMask = pm & cand_[w1] & cand_[w2] & cand_[w3];
                    while (zMask) {
                        const uint64_t one = zMask & (~zMask + 1ULL);
                        const int z = firstDigit(one);
                        zMask &= (zMask - 1ULL);

                        std::vector<int> zCells;
                        if (pm & bit(z)) zCells.push_back(pivot);
                        if (cand_[w1] & bit(z)) zCells.push_back(w1);
                        if (cand_[w2] & bit(z)) zCells.push_back(w2);
                        if (cand_[w3] & bit(z)) zCells.push_back(w3);
                        if (zCells.size() < 3U) continue;

                        bool covered = true;
                        uint64_t rest = pm & ~bit(z);
                        while (rest) {
                            const uint64_t od = rest & (~rest + 1ULL);
                            const int d = firstDigit(od);
                            rest &= (rest - 1ULL);

                            int cnt = 0;
                            if (pm & bit(d)) ++cnt;
                            if (cand_[w1] & bit(d)) ++cnt;
                            if (cand_[w2] & bit(d)) ++cnt;
                            if (cand_[w3] & bit(d)) ++cnt;
                            if (cnt < 2) {
                                covered = false;
                                break;
                            }
                        }
                        if (!covered) continue;

                        int local = 0;
                        for (int c = 0; c < NN_; ++c) {
                            if (c == pivot || c == w1 || c == w2 || c == w3) continue;
                            if (grid_[c] != 0 || (cand_[c] & bit(z)) == 0ULL) continue;
                            bool seesAll = true;
                            for (int s : zCells) {
                                if (!isPeerCell(c, s)) { seesAll = false; break; }
                            }
                            if (!seesAll) continue;
                            bool changed = false;
                            if (!removeCandidate(c, z, changed)) return false;
                            if (changed) ++local;
                        }
                        if (local > 0) {
                            std::ostringstream ss;
                            ss << "WXYZWing: pivot " << cellName(pivot) << " remove " << z
                               << " from " << local << " cell(s)";
                            pushDebugLog(ss.str());
                            n = local;
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applySkyscraper(int& n) {
    n = 0;

    for (int d = 1; d <= N_; ++d) {
        std::vector<std::pair<int, std::array<int, 2>>> rowPairs;
        for (int r = 0; r < N_; ++r) {
            std::vector<int> cols;
            for (int c = 0; c < N_; ++c) {
                const int idx = r * N_ + c;
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) cols.push_back(c);
            }
            if (cols.size() == 2U) rowPairs.push_back({r, {cols[0], cols[1]}});
        }

        for (std::size_t i = 0; i < rowPairs.size(); ++i) {
            for (std::size_t j = i + 1; j < rowPairs.size(); ++j) {
                const int r1 = rowPairs[i].first, r2 = rowPairs[j].first;
                const std::array<int, 2>& a = rowPairs[i].second;
                const std::array<int, 2>& b = rowPairs[j].second;
                int shared = -1, oa = -1, ob = -1;
                for (int x : a) {
                    for (int y : b) {
                        if (x == y) shared = x;
                    }
                }
                if (shared < 0) continue;
                oa = (a[0] == shared) ? a[1] : a[0];
                ob = (b[0] == shared) ? b[1] : b[0];
                const int roof1 = r1 * N_ + oa;
                const int roof2 = r2 * N_ + ob;

                int local = 0;
                for (int c = 0; c < NN_; ++c) {
                    if (c == roof1 || c == roof2) continue;
                    if (grid_[c] != 0 || (cand_[c] & bit(d)) == 0ULL) continue;
                    if (!isPeerCell(c, roof1) || !isPeerCell(c, roof2)) continue;
                    bool changed = false;
                    if (!removeCandidate(c, d, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }
        }

        std::vector<std::pair<int, std::array<int, 2>>> colPairs;
        for (int c = 0; c < N_; ++c) {
            std::vector<int> rows;
            for (int r = 0; r < N_; ++r) {
                const int idx = r * N_ + c;
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) rows.push_back(r);
            }
            if (rows.size() == 2U) colPairs.push_back({c, {rows[0], rows[1]}});
        }
        for (std::size_t i = 0; i < colPairs.size(); ++i) {
            for (std::size_t j = i + 1; j < colPairs.size(); ++j) {
                const int c1 = colPairs[i].first, c2 = colPairs[j].first;
                const std::array<int, 2>& a = colPairs[i].second;
                const std::array<int, 2>& b = colPairs[j].second;
                int shared = -1, oa = -1, ob = -1;
                for (int x : a) {
                    for (int y : b) {
                        if (x == y) shared = x;
                    }
                }
                if (shared < 0) continue;
                oa = (a[0] == shared) ? a[1] : a[0];
                ob = (b[0] == shared) ? b[1] : b[0];
                const int roof1 = oa * N_ + c1;
                const int roof2 = ob * N_ + c2;

                int local = 0;
                for (int c = 0; c < NN_; ++c) {
                    if (c == roof1 || c == roof2) continue;
                    if (grid_[c] != 0 || (cand_[c] & bit(d)) == 0ULL) continue;
                    if (!isPeerCell(c, roof1) || !isPeerCell(c, roof2)) continue;
                    bool changed = false;
                    if (!removeCandidate(c, d, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyTwoStringKite(int& n) {
    n = 0;
    for (int d = 1; d <= N_; ++d) {
        std::vector<std::tuple<int, int, int>> rowPairs;
        std::vector<std::tuple<int, int, int>> colPairs;
        for (int r = 0; r < N_; ++r) {
            std::vector<int> cols;
            for (int c = 0; c < N_; ++c) {
                const int idx = r * N_ + c;
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) cols.push_back(c);
            }
            if (cols.size() == 2U) rowPairs.push_back({r, cols[0], cols[1]});
        }
        for (int c = 0; c < N_; ++c) {
            std::vector<int> rows;
            for (int r = 0; r < N_; ++r) {
                const int idx = r * N_ + c;
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) rows.push_back(r);
            }
            if (rows.size() == 2U) colPairs.push_back({c, rows[0], rows[1]});
        }

        for (const auto& rp : rowPairs) {
            const int r = std::get<0>(rp);
            const int c1 = std::get<1>(rp), c2 = std::get<2>(rp);
            for (const auto& cp : colPairs) {
                const int c = std::get<0>(cp);
                const int r1 = std::get<1>(cp), r2 = std::get<2>(cp);
                if (c != c1 && c != c2) continue;
                if (r != r1 && r != r2) continue;

                const int otherCol = (c == c1) ? c2 : c1;
                const int otherRow = (r == r1) ? r2 : r1;
                const int a = r * N_ + otherCol;
                const int b = otherRow * N_ + c;

                int local = 0;
                for (int i = 0; i < NN_; ++i) {
                    if (i == a || i == b) continue;
                    if (grid_[i] != 0 || (cand_[i] & bit(d)) == 0ULL) continue;
                    if (!isPeerCell(i, a) || !isPeerCell(i, b)) continue;
                    bool changed = false;
                    if (!removeCandidate(i, d, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) { n = local; return true; }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applySimpleColoring(int& n) {
    n = 0;
    for (int d = 1; d <= N_; ++d) {
        std::vector<std::vector<int>> g(NN_);
        std::vector<char> hasNode(NN_, 0);
        for (const auto& h : houses_) {
            std::vector<int> w;
            for (int i : h) if (grid_[i] == 0 && (cand_[i] & bit(d))) w.push_back(i);
            if (w.size() == 2U) {
                const int a = w[0], b = w[1];
                g[a].push_back(b);
                g[b].push_back(a);
                hasNode[a] = hasNode[b] = 1;
            }
        }

        std::vector<int> color(NN_, -1);
        std::vector<int> comp(NN_, -1);
        std::vector<std::array<std::vector<int>, 2>> compNodes;

        int compId = 0;
        for (int s = 0; s < NN_; ++s) {
            if (!hasNode[s] || color[s] != -1) continue;
            compNodes.push_back({});
            std::vector<int> q = {s};
            color[s] = 0;
            comp[s] = compId;
            for (std::size_t qi = 0; qi < q.size(); ++qi) {
                const int u = q[qi];
                compNodes[compId][color[u]].push_back(u);
                for (int v : g[u]) {
                    if (color[v] == -1) {
                        color[v] = 1 - color[u];
                        comp[v] = compId;
                        q.push_back(v);
                    }
                }
            }
            ++compId;
        }

        int local = 0;
        for (int i = 0; i < NN_; ++i) {
            if (grid_[i] != 0 || (cand_[i] & bit(d)) == 0U || color[i] != -1) continue;
            std::map<int, int> seen;
            for (int p : peers_[i]) {
                if (color[p] == -1 || comp[p] < 0) continue;
                seen[comp[p]] |= (1 << color[p]);
            }
            bool removed = false;
            for (const auto& it : seen) {
                if (it.second == 3) {
                    bool changed = false;
                    if (!removeCandidate(i, d, changed)) return false;
                    if (changed) { ++local; removed = true; }
                    break;
                }
            }
            if (removed) continue;
        }
        if (local > 0) { n = local; return true; }

        for (int cid = 0; cid < compId; ++cid) {
            for (int clr = 0; clr < 2; ++clr) {
                bool badColor = false;
                for (const auto& h : houses_) {
                    int cnt = 0;
                    for (int i : h) {
                        if (comp[i] == cid && color[i] == clr && grid_[i] == 0 && (cand_[i] & bit(d))) {
                            ++cnt;
                            if (cnt >= 2) { badColor = true; break; }
                        }
                    }
                    if (badColor) break;
                }
                if (!badColor) continue;

                int removed = 0;
                for (int i : compNodes[cid][clr]) {
                    bool changed = false;
                    if (!removeCandidate(i, d, changed)) return false;
                    if (changed) ++removed;
                }
                if (removed > 0) { n = removed; return true; }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyUniqueRectangleType1(int& n) {
    n = 0;
    for (int r1 = 0; r1 < N_; ++r1) {
        for (int r2 = r1 + 1; r2 < N_; ++r2) {
            for (int c1 = 0; c1 < N_; ++c1) {
                for (int c2 = c1 + 1; c2 < N_; ++c2) {
                    const std::array<int, 4> cells = {
                        r1 * N_ + c1, r1 * N_ + c2, r2 * N_ + c1, r2 * N_ + c2
                    };
                    bool ok = true;
                    for (int idx : cells) {
                        if (grid_[idx] != 0 || bits(cand_[idx]) < 2) { ok = false; break; }
                    }
                    if (!ok) continue;

                    std::set<uint64_t> pairMasks;
                    for (int idx : cells) if (bits(cand_[idx]) == 2) pairMasks.insert(cand_[idx]);
                    for (uint64_t pairMask : pairMasks) {
                        int exact = 0;
                        int extraCell = -1;
                        bool valid = true;
                        for (int idx : cells) {
                            const uint64_t m = cand_[idx];
                            if ((m & pairMask) != pairMask) { valid = false; break; }
                            if (m == pairMask) {
                                ++exact;
                            } else if ((m & ~pairMask) != 0U && extraCell == -1) {
                                extraCell = idx;
                            } else {
                                valid = false;
                                break;
                            }
                        }
                        if (!valid || exact != 3 || extraCell < 0) continue;

                        int local = 0;
                        uint64_t rm = pairMask;
                        while (rm) {
                            const uint64_t one = rm & (~rm + 1ULL);
                            bool changed = false;
                            if (!removeCandidate(extraCell, firstDigit(one), changed)) return false;
                            if (changed) ++local;
                            rm &= (rm - 1ULL);
                        }
                        if (local > 0) { n = local; return true; }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyUniqueRectangleType2to6(int& n) {
    n = 0;

    auto digitsInRow = [&](int r, int d) {
        std::vector<int> out;
        for (int c = 0; c < N_; ++c) {
            const int idx = r * N_ + c;
            if (grid_[idx] == 0 && (cand_[idx] & bit(d))) out.push_back(idx);
        }
        return out;
    };
    auto digitsInCol = [&](int c, int d) {
        std::vector<int> out;
        for (int r = 0; r < N_; ++r) {
            const int idx = r * N_ + c;
            if (grid_[idx] == 0 && (cand_[idx] & bit(d))) out.push_back(idx);
        }
        return out;
    };
    auto removeFromSharedLine = [&](int roofA, int roofB, int z, int& local) -> bool {
        if (row(roofA) == row(roofB)) {
            const int r = row(roofA);
            for (int idx : houses_[r]) {
                if (idx == roofA || idx == roofB || grid_[idx] != 0) continue;
                bool changed = false;
                if (!removeCandidate(idx, z, changed)) return false;
                if (changed) ++local;
            }
            return true;
        }
        if (col(roofA) == col(roofB)) {
            const int c = col(roofA);
            for (int idx : houses_[N_ + c]) {
                if (idx == roofA || idx == roofB || grid_[idx] != 0) continue;
                bool changed = false;
                if (!removeCandidate(idx, z, changed)) return false;
                if (changed) ++local;
            }
            return true;
        }
        return true;
    };

    for (int r1 = 0; r1 < N_; ++r1) {
        for (int r2 = r1 + 1; r2 < N_; ++r2) {
            for (int c1 = 0; c1 < N_; ++c1) {
                for (int c2 = c1 + 1; c2 < N_; ++c2) {
                    const int a = r1 * N_ + c1;
                    const int b = r1 * N_ + c2;
                    const int c = r2 * N_ + c1;
                    const int d = r2 * N_ + c2;
                    const std::array<int, 4> cells = {a, b, c, d};

                    bool allUnsolved = true;
                    for (int idx : cells) {
                        if (grid_[idx] != 0 || bits(cand_[idx]) < 2) {
                            allUnsolved = false;
                            break;
                        }
                    }
                    if (!allUnsolved) continue;

                    const uint64_t common = cand_[a] & cand_[b] & cand_[c] & cand_[d];
                    const std::vector<int> commonDigits = digitsFromMask(common);
                    if (commonDigits.size() < 2U) continue;

                    for (std::size_t i = 0; i < commonDigits.size(); ++i) {
                        for (std::size_t j = i + 1; j < commonDigits.size(); ++j) {
                            const int p1 = commonDigits[i];
                            const int p2 = commonDigits[j];
                            const uint64_t pairMask = bit(p1) | bit(p2);

                            bool pairInAll = true;
                            for (int idx : cells) {
                                if ((cand_[idx] & pairMask) != pairMask) {
                                    pairInAll = false;
                                    break;
                                }
                            }
                            if (!pairInAll) continue;

                            std::vector<int> floors;
                            std::vector<int> roofs;
                            for (int idx : cells) {
                                if (cand_[idx] == pairMask) floors.push_back(idx);
                                else roofs.push_back(idx);
                            }
                            if (floors.size() != 2U || roofs.size() != 2U) continue;

                            const int roofA = roofs[0], roofB = roofs[1];
                            const uint64_t extraA = cand_[roofA] & ~pairMask;
                            const uint64_t extraB = cand_[roofB] & ~pairMask;
                            if (extraA == 0U || extraB == 0ULL) continue;

                            // Hidden UR (konserwatywnie): para (p1,p2) jest ukryta w obu wierszach i kolumnach
                            // prostokata, wiec dodatki poza para mozna usunac z tych 4 komorek.
                            auto inSet2 = [&](const std::vector<int>& where, int u, int v) {
                                return where.size() == 2U &&
                                       ((where[0] == u && where[1] == v) || (where[0] == v && where[1] == u));
                            };
                            auto whereInRow = [&](int rr, int dig) {
                                std::vector<int> where;
                                for (int idx : houses_[rr]) {
                                    if (grid_[idx] == 0 && (cand_[idx] & bit(dig))) where.push_back(idx);
                                }
                                return where;
                            };
                            auto whereInCol = [&](int cc, int dig) {
                                std::vector<int> where;
                                for (int idx : houses_[N_ + cc]) {
                                    if (grid_[idx] == 0 && (cand_[idx] & bit(dig))) where.push_back(idx);
                                }
                                return where;
                            };
                            bool hiddenUr = true;
                            for (int dig : {p1, p2}) {
                                if (!inSet2(whereInRow(r1, dig), a, b)) hiddenUr = false;
                                if (!inSet2(whereInRow(r2, dig), c, d)) hiddenUr = false;
                                if (!inSet2(whereInCol(c1, dig), a, c)) hiddenUr = false;
                                if (!inSet2(whereInCol(c2, dig), b, d)) hiddenUr = false;
                            }
                            if (hiddenUr) {
                                int local = 0;
                                for (int idx : cells) {
                                    uint64_t extras = cand_[idx] & ~pairMask;
                                    while (extras) {
                                        const uint64_t one = extras & (~extras + 1ULL);
                                        bool changed = false;
                                        if (!removeCandidate(idx, firstDigit(one), changed)) return false;
                                        if (changed) ++local;
                                        extras &= (extras - 1ULL);
                                    }
                                }
                                if (local > 0) {
                                    pushDebugLog("HiddenUR: remove extras outside pair in rectangle");
                                    n = local;
                                    return true;
                                }
                            }

                            // Type 2: dwa roofy w jednej linii, ten sam dodatkowy kandydat.
                            if ((row(roofA) == row(roofB) || col(roofA) == col(roofB)) &&
                                bits(extraA) == 1 && extraA == extraB) {
                                const int z = firstDigit(extraA);
                                int local = 0;
                                if (!removeFromSharedLine(roofA, roofB, z, local)) return false;
                                if (local > 0) { n = local; return true; }
                            }

                            // Type 4: silne lacze na jednej cyfrze pary w domu wspolnym dla roofow.
                            const std::vector<int> pairDigits = {p1, p2};
                            for (int p : pairDigits) {
                                const int other = (p == p1) ? p2 : p1;
                                std::vector<int> housesToCheck;
                                if (row(roofA) == row(roofB)) housesToCheck.push_back(row(roofA));
                                if (col(roofA) == col(roofB)) housesToCheck.push_back(N_ + col(roofA));
                                if (box(row(roofA), col(roofA)) == box(row(roofB), col(roofB))) {
                                    housesToCheck.push_back(2 * N_ + box(row(roofA), col(roofA)));
                                }
                                for (int h : housesToCheck) {
                                    std::vector<int> where;
                                    for (int idx : houses_[h]) {
                                        if (grid_[idx] == 0 && (cand_[idx] & bit(p))) where.push_back(idx);
                                    }
                                    if (where.size() != 2U) continue;
                                    const bool sameTwo =
                                        ((where[0] == roofA && where[1] == roofB) ||
                                         (where[0] == roofB && where[1] == roofA));
                                    if (!sameTwo) continue;

                                    int local = 0;
                                    for (int r : roofs) {
                                        bool changed = false;
                                        if (!removeCandidate(r, other, changed)) return false;
                                        if (changed) ++local;
                                    }
                                    if (local > 0) { n = local; return true; }
                                }
                            }

                            // Type 3 (konserwatywny): eliminacje dodatkow z roofow tylko jesli zalozenie
                            // roof=dodatkowa_cyfra prowadzi do sprzecznosci na poziomie propagacji logicznej.
                            if (row(roofA) == row(roofB) || col(roofA) == col(roofB)) {
                                const uint64_t extrasUnion = extraA | extraB;
                                if (bits(extrasUnion) >= 2) {
                                    int local = 0;
                                    for (int r : roofs) {
                                        uint64_t em = cand_[r] & ~pairMask;
                                        while (em) {
                                            const uint64_t one = em & (~em + 1ULL);
                                            const int z = firstDigit(one);
                                            if (!hasLogicalSupportWithAssignments({{r, z}})) {
                                                bool changed = false;
                                                if (!removeCandidate(r, z, changed)) return false;
                                                if (changed) ++local;
                                            }
                                            em &= (em - 1ULL);
                                        }
                                    }
                                    if (local > 0) {
                                        std::ostringstream ss;
                                        ss << "UR Type3: remove roof extras in "
                                           << cellName(roofA) << "/" << cellName(roofB);
                                        pushDebugLog(ss.str());
                                        n = local;
                                        return true;
                                    }
                                }
                            }

                            // Type 5 (konserwatywny): jesli cyfra pary jest silnie zwiazana z oboma floorami
                            // (koniugacje w obu osiach dla kazdego floora), usun z peerow obu floorow.
                            auto strongAroundFloor = [&](int floorCell, int digit) -> bool {
                                int rowCnt = 0, colCnt = 0;
                                const int rr = row(floorCell), cc = col(floorCell);
                                for (int idx : houses_[rr]) {
                                    if (grid_[idx] == 0 && (cand_[idx] & bit(digit))) ++rowCnt;
                                }
                                for (int idx : houses_[N_ + cc]) {
                                    if (grid_[idx] == 0 && (cand_[idx] & bit(digit))) ++colCnt;
                                }
                                return (rowCnt == 2 && colCnt == 2);
                            };
                            for (int p : pairDigits) {
                                if (!strongAroundFloor(floors[0], p) || !strongAroundFloor(floors[1], p)) continue;
                                int local = 0;
                                for (int idx = 0; idx < NN_; ++idx) {
                                    if (idx == floors[0] || idx == floors[1]) continue;
                                    if (grid_[idx] != 0 || (cand_[idx] & bit(p)) == 0ULL) continue;
                                    if (!isPeerCell(idx, floors[0]) || !isPeerCell(idx, floors[1])) continue;
                                    bool changed = false;
                                    if (!removeCandidate(idx, p, changed)) return false;
                                    if (changed) ++local;
                                }
                                if (local > 0) {
                                    std::ostringstream ss;
                                    ss << "UR Type5: remove " << p << " from peers of floors "
                                       << cellName(floors[0]) << "/" << cellName(floors[1]);
                                    pushDebugLog(ss.str());
                                    n = local;
                                    return true;
                                }
                            }

                            // Type 6 (wariant X-link): diagonalne roofy + silne lacza tej samej cyfry pary na obu wierszach i kolumnach.
                            const bool diagonalRoofs =
                                ((roofA == a && roofB == d) || (roofA == d && roofB == a) ||
                                 (roofA == b && roofB == c) || (roofA == c && roofB == b));
                            if (diagonalRoofs) {
                                for (int p : pairDigits) {
                                    const std::vector<int> r1pos = digitsInRow(r1, p);
                                    const std::vector<int> r2pos = digitsInRow(r2, p);
                                    const std::vector<int> c1pos = digitsInCol(c1, p);
                                    const std::vector<int> c2pos = digitsInCol(c2, p);
                                    if (r1pos.size() != 2U || r2pos.size() != 2U || c1pos.size() != 2U || c2pos.size() != 2U) continue;
                                    bool rowOk = true, colOk = true;
                                    for (int idx : r1pos) if (!(idx == a || idx == b)) rowOk = false;
                                    for (int idx : r2pos) if (!(idx == c || idx == d)) rowOk = false;
                                    for (int idx : c1pos) if (!(idx == a || idx == c)) colOk = false;
                                    for (int idx : c2pos) if (!(idx == b || idx == d)) colOk = false;
                                    if (!rowOk || !colOk) continue;

                                    const int other = (p == p1) ? p2 : p1;
                                    int local = 0;
                                    for (int r : roofs) {
                                        bool changed = false;
                                        if (!removeCandidate(r, other, changed)) return false;
                                        if (changed) ++local;
                                    }
                                    if (local > 0) { n = local; return true; }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyUniqueLoop(int& n) {
    n = 0;

    for (int p1 = 1; p1 <= N_; ++p1) {
        for (int p2 = p1 + 1; p2 <= N_; ++p2) {
            const uint64_t pairMask = bit(p1) | bit(p2);
            std::vector<int> nodes;
            std::vector<int> idByCell(NN_, -1);
            for (int idx = 0; idx < NN_; ++idx) {
                if (grid_[idx] != 0) continue;
                if ((cand_[idx] & pairMask) != pairMask) continue;
                const int bc = bits(cand_[idx]);
                if (bc < 2 || bc > 3) continue;
                idByCell[idx] = static_cast<int>(nodes.size());
                nodes.push_back(idx);
            }
            if (nodes.size() < 6U) continue;

            const int M = static_cast<int>(nodes.size());
            std::vector<std::vector<int>> adj(M);
            auto addEdge = [&](int a, int b) {
                if (a < 0 || b < 0 || a == b) return;
                if (std::find(adj[a].begin(), adj[a].end(), b) == adj[a].end()) adj[a].push_back(b);
                if (std::find(adj[b].begin(), adj[b].end(), a) == adj[b].end()) adj[b].push_back(a);
            };
            for (int h = 0; h < 3 * N_; ++h) {
                std::vector<int> where;
                for (int idx : houses_[h]) {
                    if (idByCell[idx] >= 0 && (cand_[idx] & pairMask) == pairMask) where.push_back(idByCell[idx]);
                }
                if (where.size() == 2U) addEdge(where[0], where[1]);
            }

            std::vector<char> used(M, 0);
            std::vector<int> path;
            std::function<bool(int, int)> dfs = [&](int start, int u) -> bool {
                if (path.size() > 14U) return false;
                for (int v : adj[u]) {
                    if (v == start) {
                        const std::size_t L = path.size();
                        if (L < 6U || (L % 2U) != 0ULL) continue;

                        int extraNode = -1;
                        bool ok = true;
                        for (int nid : path) {
                            const uint64_t extras = cand_[nodes[nid]] & ~pairMask;
                            if (extras == 0ULL) continue;
                            if (bits(extras) > 1 || extraNode != -1) { ok = false; break; }
                            extraNode = nid;
                        }
                        if (!ok || extraNode < 0) continue;

                        int local = 0;
                        bool ch1 = false, ch2 = false;
                        if (!removeCandidate(nodes[extraNode], p1, ch1)) return false;
                        if (ch1) ++local;
                        if (!removeCandidate(nodes[extraNode], p2, ch2)) return false;
                        if (ch2) ++local;
                        if (local > 0) {
                            std::ostringstream ss;
                            ss << "UniqueLoop: cycle length " << L
                               << " remove {" << p1 << "," << p2 << "} from " << cellName(nodes[extraNode]);
                            pushDebugLog(ss.str());
                            n = local;
                            return true;
                        }
                        continue;
                    }
                    if (used[v]) continue;
                    used[v] = 1;
                    path.push_back(v);
                    if (dfs(start, v)) return true;
                    path.pop_back();
                    used[v] = 0;
                }
                return false;
            };

            for (int s = 0; s < M; ++s) {
                std::fill(used.begin(), used.end(), 0);
                path.clear();
                used[s] = 1;
                path.push_back(s);
                if (dfs(s, s)) return true;
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyBivalueOddagon(int& n) {
    n = 0;

    for (int p1 = 1; p1 <= N_; ++p1) {
        for (int p2 = p1 + 1; p2 <= N_; ++p2) {
            const uint64_t pairMask = bit(p1) | bit(p2);
            std::vector<int> nodes;
            std::vector<int> idByCell(NN_, -1);
            for (int idx = 0; idx < NN_; ++idx) {
                if (grid_[idx] != 0) continue;
                if ((cand_[idx] & pairMask) != pairMask) continue;
                const int bc = bits(cand_[idx]);
                if (bc < 2 || bc > 3) continue;
                idByCell[idx] = static_cast<int>(nodes.size());
                nodes.push_back(idx);
            }
            if (nodes.size() < 5U) continue;

            const int M = static_cast<int>(nodes.size());
            std::vector<std::vector<int>> adj(M);
            auto addEdge = [&](int a, int b) {
                if (a < 0 || b < 0 || a == b) return;
                if (std::find(adj[a].begin(), adj[a].end(), b) == adj[a].end()) adj[a].push_back(b);
                if (std::find(adj[b].begin(), adj[b].end(), a) == adj[b].end()) adj[b].push_back(a);
            };
            for (int h = 0; h < 3 * N_; ++h) {
                std::vector<int> where;
                for (int idx : houses_[h]) {
                    if (idByCell[idx] >= 0 && (cand_[idx] & pairMask) == pairMask) where.push_back(idByCell[idx]);
                }
                if (where.size() == 2U) addEdge(where[0], where[1]);
            }

            std::vector<char> used(M, 0);
            std::vector<int> path;
            std::function<bool(int, int)> dfs = [&](int start, int u) -> bool {
                if (path.size() > 13U) return false;
                for (int v : adj[u]) {
                    if (v == start) {
                        const std::size_t L = path.size();
                        if (L < 5U || (L % 2U) == 0ULL) continue;

                        int extraNode = -1;
                        bool ok = true;
                        for (int nid : path) {
                            const uint64_t extras = cand_[nodes[nid]] & ~pairMask;
                            if (extras == 0ULL) continue;
                            if (bits(extras) > 1 || extraNode != -1) { ok = false; break; }
                            extraNode = nid;
                        }
                        if (!ok || extraNode < 0) continue;

                        int local = 0;
                        bool ch1 = false, ch2 = false;
                        if (!removeCandidate(nodes[extraNode], p1, ch1)) return false;
                        if (ch1) ++local;
                        if (!removeCandidate(nodes[extraNode], p2, ch2)) return false;
                        if (ch2) ++local;
                        if (local > 0) {
                            std::ostringstream ss;
                            ss << "BivalueOddagon: cycle length " << L
                               << " remove {" << p1 << "," << p2 << "} from " << cellName(nodes[extraNode]);
                            pushDebugLog(ss.str());
                            n = local;
                            return true;
                        }
                        continue;
                    }
                    if (used[v]) continue;
                    used[v] = 1;
                    path.push_back(v);
                    if (dfs(start, v)) return true;
                    path.pop_back();
                    used[v] = 0;
                }
                return false;
            };

            for (int s = 0; s < M; ++s) {
                std::fill(used.begin(), used.end(), 0);
                path.clear();
                used[s] = 1;
                path.push_back(s);
                if (dfs(s, s)) return true;
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyBUGPlus1(int& n) {
    n = 0;
    int bugCell = -1;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] != 0) continue;
        const int bc = bits(cand_[i]);
        if (bc == 2) continue;
        if (bc == 3 && bugCell == -1) {
            bugCell = i;
            continue;
        }
        return false;
    }
    if (bugCell < 0) return false;

    const int r = row(bugCell), c = col(bugCell), b = box(r, c);
    FOR_EACH_BIT(cand_[bugCell], d) {
        int rowCnt = 0, colCnt = 0, boxCnt = 0;
        for (int i : houses_[r]) if (grid_[i] == 0 && (cand_[i] & bit(d))) ++rowCnt;
        for (int i : houses_[N_ + c]) if (grid_[i] == 0 && (cand_[i] & bit(d))) ++colCnt;
        for (int i : houses_[2 * N_ + b]) if (grid_[i] == 0 && (cand_[i] & bit(d))) ++boxCnt;
        if ((rowCnt % 2) == 1 && (colCnt % 2) == 1 && (boxCnt % 2) == 1) {
            if (!assignValue(bugCell, d)) return false;
            n = 1;
            return true;
        }
    }
    return false;
}

bool SudokuAnalyzer::applyRemotePairs(int& n) {
    n = 0;
    std::map<uint64_t, std::vector<int>> byPair;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] == 0 && bits(cand_[i]) == 2) {
            byPair[cand_[i]].push_back(i);
        }
    }

    for (const auto& entry : byPair) {
        const uint64_t pairMask = entry.first;
        const std::vector<int>& nodes = entry.second;
        if (nodes.size() < 4U) continue;
        std::map<int, int> nodePos;
        for (std::size_t i = 0; i < nodes.size(); ++i) nodePos[nodes[i]] = static_cast<int>(i);

        std::vector<std::vector<int>> g(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            for (std::size_t j = i + 1; j < nodes.size(); ++j) {
                if (!isPeerCell(nodes[i], nodes[j])) continue;
                g[i].push_back(static_cast<int>(j));
                g[j].push_back(static_cast<int>(i));
            }
        }

        std::vector<int> color(nodes.size(), -1), comp(nodes.size(), -1);
        int compId = 0;
        std::vector<std::array<std::vector<int>, 2>> compNodes;
        for (std::size_t s = 0; s < nodes.size(); ++s) {
            if (color[s] != -1) continue;
            compNodes.push_back({});
            std::vector<int> q = {static_cast<int>(s)};
            color[s] = 0;
            comp[s] = compId;
            for (std::size_t qi = 0; qi < q.size(); ++qi) {
                const int u = q[qi];
                compNodes[compId][color[u]].push_back(nodes[u]);
                for (int v : g[u]) {
                    if (color[v] == -1) {
                        color[v] = 1 - color[u];
                        comp[v] = compId;
                        q.push_back(v);
                    }
                }
            }
            ++compId;
        }

        int local = 0;
        for (int i = 0; i < NN_; ++i) {
            if (grid_[i] != 0) continue;
            if ((cand_[i] & pairMask) == 0ULL) continue;
            std::map<int, int> seen;
            for (std::size_t p = 0; p < nodes.size(); ++p) {
                if (!isPeerCell(i, nodes[p])) continue;
                seen[comp[p]] |= (1 << color[p]);
            }
            bool validComp = false;
            for (const auto& it : seen) {
                if (it.second == 3) { validComp = true; break; }
            }
            if (!validComp) continue;

            uint64_t rm = pairMask & cand_[i];
            while (rm) {
                const uint64_t one = rm & (~rm + 1ULL);
                bool changed = false;
                if (!removeCandidate(i, firstDigit(one), changed)) return false;
                if (changed) ++local;
                rm &= (rm - 1ULL);
            }
        }
        if (local > 0) { n = local; return true; }
    }
    return false;
}

bool SudokuAnalyzer::applyWWing(int& n) {
    n = 0;
    std::vector<std::vector<std::pair<int, int>>> conjugates(N_ + 1);
    for (int d = 1; d <= N_; ++d) {
        for (const auto& h : houses_) {
            std::vector<int> w;
            for (int i : h) if (grid_[i] == 0 && (cand_[i] & bit(d))) w.push_back(i);
            if (w.size() == 2U) conjugates[d].push_back({w[0], w[1]});
        }
    }

    std::vector<int> bivalueCells;
    for (int i = 0; i < NN_; ++i) if (grid_[i] == 0 && bits(cand_[i]) == 2) bivalueCells.push_back(i);

    for (std::size_t i = 0; i < bivalueCells.size(); ++i) {
        const int p = bivalueCells[i];
        for (std::size_t j = i + 1; j < bivalueCells.size(); ++j) {
            const int q = bivalueCells[j];
            if (cand_[p] != cand_[q] || isPeerCell(p, q)) continue;
            const std::vector<int> ds = digitsFromMask(cand_[p]);
            if (ds.size() != 2U) continue;

            for (int linkDigit : ds) {
                const int elimDigit = (ds[0] == linkDigit) ? ds[1] : ds[0];
                for (const auto& lk : conjugates[linkDigit]) {
                    const int a = lk.first, b = lk.second;
                    const bool ok1 = isPeerCell(p, a) && isPeerCell(q, b);
                    const bool ok2 = isPeerCell(p, b) && isPeerCell(q, a);
                    if (!ok1 && !ok2) continue;

                    int local = 0;
                    for (int c = 0; c < NN_; ++c) {
                        if (c == p || c == q) continue;
                        if (grid_[c] != 0 || (cand_[c] & bit(elimDigit)) == 0ULL) continue;
                        if (!isPeerCell(c, p) || !isPeerCell(c, q)) continue;
                        bool changed = false;
                        if (!removeCandidate(c, elimDigit, changed)) return false;
                        if (changed) ++local;
                    }
                    if (local > 0) { n = local; return true; }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyFinnedXWingSashimi(int& n) {
    n = 0;

    auto process = [&](bool rowBased) -> bool {
        for (int d = 1; d <= N_; ++d) {
            for (int l1 = 0; l1 < N_; ++l1) {
                std::vector<int> p1;
                for (int p = 0; p < N_; ++p) {
                    const int idx = rowBased ? (l1 * N_ + p) : (p * N_ + l1);
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) p1.push_back(p);
                }
                if (p1.size() != 2U) continue;

                for (int l2 = 0; l2 < N_; ++l2) {
                    if (l2 == l1) continue;
                    std::vector<int> p2;
                    for (int p = 0; p < N_; ++p) {
                        const int idx = rowBased ? (l2 * N_ + p) : (p * N_ + l2);
                        if (grid_[idx] == 0 && (cand_[idx] & bit(d))) p2.push_back(p);
                    }
                    if (p2.size() < 3U || p2.size() > 4U) continue;

                    std::set<int> baseSet(p1.begin(), p1.end());
                    std::vector<int> extras;
                    for (int p : p2) {
                        if (!baseSet.count(p)) extras.push_back(p);
                    }
                    if (extras.empty()) continue;

                    int finBox = -1;
                    bool sameBox = true;
                    for (int ep : extras) {
                        const int b = rowBased ? box(l2, ep) : box(ep, l2);
                        if (finBox < 0) finBox = b;
                        else if (finBox != b) { sameBox = false; break; }
                    }
                    if (!sameBox || finBox < 0) continue;

                    for (int basePos : p1) {
                        const int baseInL2Box = rowBased ? box(l2, basePos) : box(basePos, l2);
                        if (baseInL2Box != finBox) continue;

                        int local = 0;
                        const std::vector<int>& targetHouse = rowBased ? houses_[N_ + basePos] : houses_[basePos];
                        for (int idx : targetHouse) {
                            const int line = rowBased ? row(idx) : col(idx);
                            if (line == l1 || line == l2) continue;
                            if ((cand_[idx] & bit(d)) == 0U || grid_[idx] != 0) continue;
                            if (box(row(idx), col(idx)) != finBox) continue;
                            bool changed = false;
                            if (!removeCandidate(idx, d, changed)) return false;
                            if (changed) ++local;
                        }
                        if (local > 0) { n = local; return true; }
                    }
                }
            }
        }
        return false;
    };

    if (process(true)) return true;
    return process(false);
}

bool SudokuAnalyzer::applyFinnedFish(int size, int& n) {
    n = 0;
    if (size < 2 || size > N_) return false;

    auto process = [&](bool rowBased) -> bool {
        for (int d = 1; d <= N_; ++d) {
            std::vector<std::vector<int>> positions(N_);
            std::vector<int> eligible;
            for (int line = 0; line < N_; ++line) {
                std::vector<int> pos;
                for (int p = 0; p < N_; ++p) {
                    const int idx = rowBased ? (line * N_ + p) : (p * N_ + line);
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) pos.push_back(p);
                }
                positions[line] = std::move(pos);
                const int cnt = static_cast<int>(positions[line].size());
                if (cnt >= 2 && cnt <= size + 2) eligible.push_back(line);
            }
            if (static_cast<int>(eligible.size()) < size) continue;

            bool found = false;
            forEachCombo(eligible, size, [&](const std::vector<int>& lines) {
                if (found || contradiction_) return;

                std::vector<char> inLine(N_, 0), inUnionPos(N_, 0);
                std::vector<int> unionPos;
                for (int line : lines) {
                    inLine[line] = 1;
                    for (int p : positions[line]) {
                        if (!inUnionPos[p]) {
                            inUnionPos[p] = 1;
                            unionPos.push_back(p);
                        }
                    }
                }
                const int uc = static_cast<int>(unionPos.size());
                if (uc <= size || uc > size + 2) return;
                if (uc < size) return;

                forEachCombo(unionPos, size, [&](const std::vector<int>& coverPos) {
                    if (found || contradiction_) return;
                    std::vector<char> inCover(N_, 0);
                    for (int p : coverPos) inCover[p] = 1;

                    std::vector<int> finIndices;
                    bool valid = true;
                    for (int line : lines) {
                        int baseCnt = 0;
                        int finCnt = 0;
                        for (int p : positions[line]) {
                            if (inCover[p]) {
                                ++baseCnt;
                            } else {
                                ++finCnt;
                                const int finIdx = rowBased ? (line * N_ + p) : (p * N_ + line);
                                finIndices.push_back(finIdx);
                            }
                        }
                        if (baseCnt < 2 || finCnt > 2) {
                            valid = false;
                            break;
                        }
                    }
                    if (!valid || finIndices.empty()) return;

                    int finBox = -1;
                    for (int idx : finIndices) {
                        const int b = box(row(idx), col(idx));
                        if (finBox < 0) finBox = b;
                        else if (finBox != b) { valid = false; break; }
                    }
                    if (!valid || finBox < 0) return;

                    int local = 0;
                    for (int p : coverPos) {
                        const std::vector<int>& coverHouse = rowBased ? houses_[N_ + p] : houses_[p];
                        for (int idx : coverHouse) {
                            const int line = rowBased ? row(idx) : col(idx);
                            if (inLine[line]) continue;
                            if (grid_[idx] != 0 || (cand_[idx] & bit(d)) == 0ULL) continue;
                            if (box(row(idx), col(idx)) != finBox) continue;
                            bool seesAllFins = true;
                            for (int f : finIndices) {
                                if (!isPeerCell(idx, f)) { seesAllFins = false; break; }
                            }
                            if (!seesAllFins) continue;
                            bool changed = false;
                            if (!removeCandidate(idx, d, changed)) return;
                            if (changed) ++local;
                        }
                    }
                    if (local > 0) {
                        std::ostringstream ss;
                        ss << "FinnedFish(" << size << "): remove " << d
                           << " in fin box " << (finBox + 1)
                           << " from " << local << " cell(s)";
                        pushDebugLog(ss.str());
                        n = local;
                        found = true;
                    }
                });
            });
            if (found) return true;
        }
        return false;
    };

    if (process(true)) return true;
    return process(false);
}

bool SudokuAnalyzer::applyEmptyRectangle(int& n) {
    n = 0;
    for (int d = 1; d <= N_; ++d) {
        for (int b = 0; b < N_; ++b) {
            const auto& boxCells = houses_[2 * N_ + b];
            std::vector<int> candInBox;
            std::set<int> rowsInBox, colsInBox;
            for (int idx : boxCells) {
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) {
                    candInBox.push_back(idx);
                    rowsInBox.insert(row(idx));
                    colsInBox.insert(col(idx));
                }
            }
            if (candInBox.size() < 3U) continue;

            for (int r : rowsInBox) {
                for (int c : colsInBox) {
                    const int crossIdx = r * N_ + c;
                    if (box(r, c) != b) continue;
                    if (grid_[crossIdx] == 0 && (cand_[crossIdx] & bit(d))) continue;

                    bool crossShape = true;
                    std::vector<int> rowArm, colArm;
                    for (int idx : candInBox) {
                        if (row(idx) != r && col(idx) != c) { crossShape = false; break; }
                        if (row(idx) == r && col(idx) != c) rowArm.push_back(idx);
                        if (col(idx) == c && row(idx) != r) colArm.push_back(idx);
                    }
                    if (!crossShape || rowArm.empty() || colArm.empty()) continue;

                    std::vector<int> rowCandidates;
                    for (int idx : houses_[r]) {
                        if (grid_[idx] == 0 && (cand_[idx] & bit(d))) rowCandidates.push_back(idx);
                    }
                    std::vector<int> colCandidates;
                    for (int idx : houses_[N_ + c]) {
                        if (grid_[idx] == 0 && (cand_[idx] & bit(d))) colCandidates.push_back(idx);
                    }
                    if (rowCandidates.size() != 2U || colCandidates.size() != 2U) continue;

                    int rowInside = -1, rowOutside = -1;
                    for (int idx : rowCandidates) {
                        if (box(row(idx), col(idx)) == b) rowInside = idx;
                        else rowOutside = idx;
                    }
                    int colInside = -1, colOutside = -1;
                    for (int idx : colCandidates) {
                        if (box(row(idx), col(idx)) == b) colInside = idx;
                        else colOutside = idx;
                    }
                    if (rowInside < 0 || rowOutside < 0 || colInside < 0 || colOutside < 0) continue;

                    const int elimIdx = row(colOutside) * N_ + col(rowOutside);
                    if (row(elimIdx) != row(colOutside) || col(elimIdx) != col(rowOutside)) continue;
                    if (elimIdx == rowOutside || elimIdx == colOutside) continue;
                    if (grid_[elimIdx] != 0 || (cand_[elimIdx] & bit(d)) == 0ULL) continue;

                    bool changed = false;
                    if (!removeCandidate(elimIdx, d, changed)) return false;
                    if (changed) { n = 1; return true; }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyAvoidableRectangle(int& n) {
    n = 0;
    for (int r1 = 0; r1 < N_; ++r1) {
        for (int r2 = r1 + 1; r2 < N_; ++r2) {
            for (int c1 = 0; c1 < N_; ++c1) {
                for (int c2 = c1 + 1; c2 < N_; ++c2) {
                    const std::array<int, 4> cells = {
                        r1 * N_ + c1, r1 * N_ + c2, r2 * N_ + c1, r2 * N_ + c2
                    };

                    bool hasGiven = false;
                    for (int idx : cells) {
                        if (b_.cells[idx].revealed) { hasGiven = true; break; }
                    }
                    if (!hasGiven) continue;

                    std::set<uint64_t> pairMasks;
                    for (int idx : cells) {
                        if (grid_[idx] == 0 && bits(cand_[idx]) == 2) {
                            pairMasks.insert(cand_[idx]);
                        }
                    }

                    for (uint64_t pairMask : pairMasks) {
                        int exact = 0;
                        int extraCell = -1;
                        bool valid = true;
                        for (int idx : cells) {
                            if (grid_[idx] != 0) {
                                const int v = grid_[idx];
                                if ((pairMask & bit(v)) == 0ULL) { valid = false; break; }
                                ++exact;
                                continue;
                            }
                            const uint64_t m = cand_[idx];
                            if ((m & pairMask) != pairMask) { valid = false; break; }
                            if (m == pairMask) {
                                ++exact;
                            } else if ((m & ~pairMask) != 0U && extraCell == -1) {
                                extraCell = idx;
                            } else {
                                valid = false;
                                break;
                            }
                        }
                        if (!valid || exact != 3 || extraCell < 0) continue;

                        int local = 0;
                        uint64_t rm = pairMask;
                        while (rm) {
                            const uint64_t one = rm & (~rm + 1ULL);
                            bool changed = false;
                            if (!removeCandidate(extraCell, firstDigit(one), changed)) return false;
                            if (changed) ++local;
                            rm &= (rm - 1ULL);
                        }
                        if (local > 0) { n = local; return true; }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyXChain(int& n) {
    n = 0;
    for (int d = 1; d <= N_; ++d) {
        std::vector<int> nodes;
        for (int i = 0; i < NN_; ++i) {
            if (grid_[i] == 0 && (cand_[i] & bit(d))) nodes.push_back(i);
        }
        const int M = static_cast<int>(nodes.size());
        if (M < 4) continue;

        std::map<int, int> pos;
        for (int i = 0; i < M; ++i) pos[nodes[i]] = i;

        std::vector<std::vector<unsigned char>> edge(M, std::vector<unsigned char>(M, 0));
        for (const auto& h : houses_) {
            std::vector<int> hnodes;
            for (int idx : h) {
                const auto it = pos.find(idx);
                if (it != pos.end()) hnodes.push_back(it->second);
            }
            if (hnodes.size() < 2U) continue;
            const bool strongHouse = (hnodes.size() == 2U);
            for (std::size_t i = 0; i < hnodes.size(); ++i) {
                for (std::size_t j = i + 1; j < hnodes.size(); ++j) {
                    const int a = hnodes[i], b = hnodes[j];
                    if (strongHouse) {
                        edge[a][b] = edge[b][a] = 2U;
                    } else if ((edge[a][b] & 2U) == 0ULL) {
                        edge[a][b] = edge[b][a] = static_cast<unsigned char>(edge[a][b] | 1ULL);
                    }
                }
            }
        }

        const int maxDepth = 9;
        for (int s = 0; s < M; ++s) {
            std::vector<char> used(M, 0);
            used[s] = 1;
            std::function<bool(int, bool, int)> dfs = [&](int u, bool expectStrong, int depth) -> bool {
                if (depth >= maxDepth) return false;
                for (int v = 0; v < M; ++v) {
                    if (used[v] || v == u) continue;
                    const unsigned char t = edge[u][v];
                    const bool ok = expectStrong ? ((t & 2U) != 0ULL) : ((t & 1ULL) != 0ULL);
                    if (!ok) continue;

                    const int newDepth = depth + 1;
                    const bool edgeWasStrong = expectStrong;
                    if (edgeWasStrong && newDepth >= 3) {
                        int local = 0;
                        for (int i = 0; i < NN_; ++i) {
                            if (i == nodes[s] || i == nodes[v]) continue;
                            if (grid_[i] != 0 || (cand_[i] & bit(d)) == 0ULL) continue;
                            if (!isPeerCell(i, nodes[s]) || !isPeerCell(i, nodes[v])) continue;
                            bool changed = false;
                            if (!removeCandidate(i, d, changed)) return true;
                            if (changed) ++local;
                        }
                        if (local > 0) { n = local; return true; }
                    }

                    used[v] = 1;
                    if (dfs(v, !expectStrong, newDepth)) return true;
                    used[v] = 0;
                }
                return false;
            };

            if (dfs(s, true, 0)) return true;
        }
    }
    return false;
}

bool SudokuAnalyzer::applyXYChain(int& n) {
    n = 0;
    std::vector<int> cells;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] == 0 && bits(cand_[i]) == 2) cells.push_back(i);
    }
    if (cells.size() < 3U) return false;

    auto otherDigit = [&](uint64_t m, int d) -> int {
        const uint64_t rest = m & ~bit(d);
        return firstDigit(rest);
    };

    const int maxLen = 10;
    for (std::size_t si = 0; si < cells.size(); ++si) {
        const int sCell = cells[si];
        const std::vector<int> sd = digitsFromMask(cand_[sCell]);
        if (sd.size() != 2U) continue;

        for (int z : sd) {
            const int firstShared = (sd[0] == z) ? sd[1] : sd[0];
            std::vector<char> used(cells.size(), 0);
            used[si] = 1;

            std::function<bool(int, int, int)> dfs = [&](int curIdx, int sharedDigit, int len) -> bool {
                if (len >= maxLen) return false;
                const int curCell = cells[curIdx];
                for (std::size_t ni = 0; ni < cells.size(); ++ni) {
                    if (used[ni] || ni == static_cast<std::size_t>(curIdx)) continue;
                    const int nxtCell = cells[ni];
                    if (!isPeerCell(curCell, nxtCell)) continue;
                    if ((cand_[nxtCell] & bit(sharedDigit)) == 0ULL) continue;

                    const int nextShared = otherDigit(cand_[nxtCell], sharedDigit);
                    if (nextShared == 0) continue;
                    const int newLen = len + 1;

                    if (newLen >= 3 && (cand_[nxtCell] & bit(z))) {
                        int local = 0;
                        for (int i = 0; i < NN_; ++i) {
                            if (i == sCell || i == nxtCell) continue;
                            if (grid_[i] != 0 || (cand_[i] & bit(z)) == 0ULL) continue;
                            if (!isPeerCell(i, sCell) || !isPeerCell(i, nxtCell)) continue;
                            bool changed = false;
                            if (!removeCandidate(i, z, changed)) return true;
                            if (changed) ++local;
                        }
                        if (local > 0) { n = local; return true; }
                    }

                    used[ni] = 1;
                    if (dfs(static_cast<int>(ni), nextShared, newLen)) return true;
                    used[ni] = 0;
                }
                return false;
            };

            if (dfs(static_cast<int>(si), firstShared, 1)) return true;
        }
    }
    return false;
}

bool SudokuAnalyzer::applyAIC(int& n) {
    n = 0;

    struct Node {
        int cell = -1;
        int digit = 0;
    };

    std::vector<Node> nodes;
    std::vector<int> nodeByCellDigit(NN_ * (N_ + 1), -1);
    auto nodeKey = [&](int cell, int digit) { return cell * (N_ + 1) + digit; };

    for (int cell = 0; cell < NN_; ++cell) {
        if (grid_[cell] != 0) continue;
        uint64_t m = cand_[cell];
        while (m) {
            const uint64_t one = m & (~m + 1ULL);
            const int d = firstDigit(one);
            const int id = static_cast<int>(nodes.size());
            nodes.push_back({cell, d});
            nodeByCellDigit[nodeKey(cell, d)] = id;
            m &= (m - 1ULL);
        }
    }
    if (nodes.size() < 4U) return false;

    std::vector<std::vector<std::pair<int, unsigned char>>> adj(nodes.size());
    auto addEdge = [&](int a, int b, unsigned char edgeType) {
        if (a < 0 || b < 0 || a == b) return;
        bool found = false;
        for (auto& pr : adj[a]) {
            if (pr.first == b) {
                pr.second = static_cast<unsigned char>(pr.second | edgeType);
                found = true;
                break;
            }
        }
        if (!found) adj[a].push_back({b, edgeType});
        found = false;
        for (auto& pr : adj[b]) {
            if (pr.first == a) {
                pr.second = static_cast<unsigned char>(pr.second | edgeType);
                found = true;
                break;
            }
        }
        if (!found) adj[b].push_back({a, edgeType});
    };

    for (int cell = 0; cell < NN_; ++cell) {
        if (grid_[cell] != 0) continue;
        std::vector<int> ds = digitsFromMask(cand_[cell]);
        if (ds.size() < 2U) continue;
        const unsigned char t = (ds.size() == 2U) ? static_cast<unsigned char>(3U) : static_cast<unsigned char>(1ULL);
        for (std::size_t i = 0; i < ds.size(); ++i) {
            for (std::size_t j = i + 1; j < ds.size(); ++j) {
                const int a = nodeByCellDigit[nodeKey(cell, ds[i])];
                const int b = nodeByCellDigit[nodeKey(cell, ds[j])];
                addEdge(a, b, t);
            }
        }
    }

    for (const auto& h : houses_) {
        for (int d = 1; d <= N_; ++d) {
            std::vector<int> houseNodes;
            for (int idx : h) {
                const int nid = nodeByCellDigit[nodeKey(idx, d)];
                if (nid >= 0) houseNodes.push_back(nid);
            }
            if (houseNodes.size() < 2U) continue;
            const unsigned char t = (houseNodes.size() == 2U) ? static_cast<unsigned char>(3U) : static_cast<unsigned char>(1ULL);
            for (std::size_t i = 0; i < houseNodes.size(); ++i) {
                for (std::size_t j = i + 1; j < houseNodes.size(); ++j) {
                    addEdge(houseNodes[i], houseNodes[j], t);
                }
            }
        }
    }

    const int maxDepth = 12;
    for (int s = 0; s < static_cast<int>(nodes.size()); ++s) {
        std::vector<char> used(nodes.size(), 0);
        used[s] = 1;

        std::function<bool(int, bool, int)> dfs = [&](int u, bool expectStrong, int depth) -> bool {
            if (depth >= maxDepth) return false;
            for (const auto& e : adj[u]) {
                const int v = e.first;
                const unsigned char et = e.second;
                if (used[v]) continue;
                if (expectStrong && (et & 2U) == 0ULL) continue;
                if (!expectStrong && (et & 1ULL) == 0ULL) continue;

                const int newDepth = depth + 1;
                const bool edgeWasStrong = expectStrong;
                if (edgeWasStrong && newDepth >= 3 &&
                    nodes[v].digit == nodes[s].digit &&
                    nodes[v].cell != nodes[s].cell) {
                    const int d = nodes[s].digit;
                    int local = 0;
                    for (int i = 0; i < NN_; ++i) {
                        if (i == nodes[s].cell || i == nodes[v].cell) continue;
                        if (grid_[i] != 0 || (cand_[i] & bit(d)) == 0ULL) continue;
                        if (!isPeerCell(i, nodes[s].cell) || !isPeerCell(i, nodes[v].cell)) continue;
                        bool changed = false;
                        if (!removeCandidate(i, d, changed)) return false;
                        if (changed) ++local;
                    }
                    if (local > 0) {
                        n = local;
                        return true;
                    }
                }

                used[v] = 1;
                if (dfs(v, !expectStrong, newDepth)) return true;
                used[v] = 0;
            }
            return false;
        };

        if (dfs(s, true, 0)) return true;
    }

    return false;
}

bool SudokuAnalyzer::applyContinuousNiceLoop(int& n) {
    n = 0;

    struct Node {
        int cell = -1;
        int digit = 0;
    };

    std::vector<Node> nodes;
    std::vector<int> nodeByCellDigit(NN_ * (N_ + 1), -1);
    auto nodeKey = [&](int cell, int digit) { return cell * (N_ + 1) + digit; };

    for (int cell = 0; cell < NN_; ++cell) {
        if (grid_[cell] != 0) continue;
        uint64_t m = cand_[cell];
        while (m) {
            const uint64_t one = m & (~m + 1ULL);
            const int d = firstDigit(one);
            const int id = static_cast<int>(nodes.size());
            nodes.push_back({cell, d});
            nodeByCellDigit[nodeKey(cell, d)] = id;
            m &= (m - 1ULL);
        }
    }
    if (nodes.size() < 4U) return false;

    const int M = static_cast<int>(nodes.size());
    std::vector<std::vector<unsigned char>> edge(M, std::vector<unsigned char>(M, 0U));
    auto addEdge = [&](int a, int b, unsigned char edgeType) {
        if (a < 0 || b < 0 || a == b) return;
        edge[a][b] = static_cast<unsigned char>(edge[a][b] | edgeType);
        edge[b][a] = static_cast<unsigned char>(edge[b][a] | edgeType);
    };

    for (int cell = 0; cell < NN_; ++cell) {
        if (grid_[cell] != 0) continue;
        const std::vector<int> ds = digitsFromMask(cand_[cell]);
        if (ds.size() < 2U) continue;
        const unsigned char t = (ds.size() == 2U) ? static_cast<unsigned char>(3U) : static_cast<unsigned char>(1ULL);
        for (std::size_t i = 0; i < ds.size(); ++i) {
            for (std::size_t j = i + 1; j < ds.size(); ++j) {
                addEdge(nodeByCellDigit[nodeKey(cell, ds[i])], nodeByCellDigit[nodeKey(cell, ds[j])], t);
            }
        }
    }
    for (const auto& h : houses_) {
        for (int d = 1; d <= N_; ++d) {
            std::vector<int> houseNodes;
            for (int idx : h) {
                const int nid = nodeByCellDigit[nodeKey(idx, d)];
                if (nid >= 0) houseNodes.push_back(nid);
            }
            if (houseNodes.size() < 2U) continue;
            const unsigned char t = (houseNodes.size() == 2U) ? static_cast<unsigned char>(3U) : static_cast<unsigned char>(1ULL);
            for (std::size_t i = 0; i < houseNodes.size(); ++i) {
                for (std::size_t j = i + 1; j < houseNodes.size(); ++j) {
                    addEdge(houseNodes[i], houseNodes[j], t);
                }
            }
        }
    }

    auto tryWeakEdgeElim = [&](int a, int b) -> bool {
        if (nodes[a].digit != nodes[b].digit) return false;
        if (nodes[a].cell == nodes[b].cell) return false;
        const int d = nodes[a].digit;
        int local = 0;
        for (int i = 0; i < NN_; ++i) {
            if (i == nodes[a].cell || i == nodes[b].cell) continue;
            if (grid_[i] != 0 || (cand_[i] & bit(d)) == 0ULL) continue;
            if (!isPeerCell(i, nodes[a].cell) || !isPeerCell(i, nodes[b].cell)) continue;
            bool changed = false;
            if (!removeCandidate(i, d, changed)) return false;
            if (changed) ++local;
        }
        if (local > 0) {
            n = local;
            std::ostringstream ss;
            ss << "ContinuousNiceLoop: weak-link closure removes " << d
               << " from peers of " << cellName(nodes[a].cell)
               << " and " << cellName(nodes[b].cell);
            pushDebugLog(ss.str());
            return true;
        }
        return false;
    };

    for (int u = 0; u < M; ++u) {
        for (int v = 0; v < M; ++v) {
            if (v == u || (edge[u][v] & 2U) == 0ULL) continue; // strong
            for (int w = 0; w < M; ++w) {
                if (w == u || w == v || (edge[v][w] & 1ULL) == 0ULL) continue; // weak
                for (int x = 0; x < M; ++x) {
                    if (x == u || x == v || x == w) continue;
                    if ((edge[w][x] & 2U) == 0ULL) continue; // strong
                    if ((edge[x][u] & 1ULL) == 0ULL) continue; // weak closes loop

                    if (tryWeakEdgeElim(v, w)) return true;
                    if (tryWeakEdgeElim(x, u)) return true;
                }
            }
        }
    }

    return false;
}

bool SudokuAnalyzer::applySKLoop(int& n) {
    n = 0;
    if (N_ != 9) return false;

    auto strongInHouse = [&](const std::vector<int>& house, int d, int a, int b) {
        std::vector<int> where;
        for (int idx : house) {
            if (grid_[idx] == 0 && (cand_[idx] & bit(d))) where.push_back(idx);
        }
        return where.size() == 2U &&
               ((where[0] == a && where[1] == b) || (where[0] == b && where[1] == a));
    };

    for (int r1 = 0; r1 < N_; ++r1) {
        for (int r2 = r1 + 1; r2 < N_; ++r2) {
            for (int c1 = 0; c1 < N_; ++c1) {
                for (int c2 = c1 + 1; c2 < N_; ++c2) {
                    const int a = r1 * N_ + c1;
                    const int b = r1 * N_ + c2;
                    const int c = r2 * N_ + c1;
                    const int d = r2 * N_ + c2;
                    if (grid_[a] != 0 || grid_[b] != 0 || grid_[c] != 0 || grid_[d] != 0) continue;
                    if (bits(cand_[a]) != 2 || bits(cand_[b]) != 2 || bits(cand_[c]) != 2 || bits(cand_[d]) != 2) continue;

                    const uint64_t ab = cand_[a] & cand_[b];
                    const uint64_t bd = cand_[b] & cand_[d];
                    const uint64_t dc = cand_[d] & cand_[c];
                    const uint64_t ca = cand_[c] & cand_[a];
                    if (bits(ab) != 1 || bits(bd) != 1 || bits(dc) != 1 || bits(ca) != 1) continue;

                    const int dab = firstDigit(ab);
                    const int dbd = firstDigit(bd);
                    const int ddc = firstDigit(dc);
                    const int dca = firstDigit(ca);

                    if ((cand_[a] & ~(bit(dab) | bit(dca))) != 0ULL) continue;
                    if ((cand_[b] & ~(bit(dab) | bit(dbd))) != 0ULL) continue;
                    if ((cand_[d] & ~(bit(dbd) | bit(ddc))) != 0ULL) continue;
                    if ((cand_[c] & ~(bit(ddc) | bit(dca))) != 0ULL) continue;

                    if (!strongInHouse(houses_[r1], dab, a, b)) continue;
                    if (!strongInHouse(houses_[N_ + c2], dbd, b, d)) continue;
                    if (!strongInHouse(houses_[r2], ddc, d, c)) continue;
                    if (!strongInHouse(houses_[N_ + c1], dca, c, a)) continue;

                    int local = 0;
                    auto elimFromPeers = [&](int u, int v, int dig) -> bool {
                        for (int idx = 0; idx < NN_; ++idx) {
                            if (idx == u || idx == v || grid_[idx] != 0) continue;
                            if ((cand_[idx] & bit(dig)) == 0ULL) continue;
                            if (!isPeerCell(idx, u) || !isPeerCell(idx, v)) continue;
                            bool changed = false;
                            if (!removeCandidate(idx, dig, changed)) return false;
                            if (changed) ++local;
                        }
                        return true;
                    };
                    if (!elimFromPeers(a, b, dab)) return false;
                    if (!elimFromPeers(b, d, dbd)) return false;
                    if (!elimFromPeers(d, c, ddc)) return false;
                    if (!elimFromPeers(c, a, dca)) return false;

                    if (local > 0) {
                        std::ostringstream ss;
                        ss << "SKLoop: rectangle r" << (r1 + 1) << "/r" << (r2 + 1)
                           << " c" << (c1 + 1) << "/c" << (c2 + 1)
                           << " removed " << local << " candidate(s)";
                        pushDebugLog(ss.str());
                        n = local;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyALSXZ(int& n) {
    n = 0;

    struct ALS {
        std::vector<int> cells;
        uint64_t mask = 0ULL;
    };

    std::vector<ALS> alss;
    std::unordered_set<uint64_t> seen;

    auto addAls = [&](std::vector<int> cells) {
        if (cells.empty() || cells.size() > 3U) return;
        std::sort(cells.begin(), cells.end());
        uint64_t key = static_cast<uint64_t>(cells.size());
        for (int c : cells) key = key * 1297ULL + static_cast<uint64_t>(c);
        if (!seen.insert(key).second) return;

        uint64_t m = 0ULL;
        for (int c : cells) {
            if (grid_[c] != 0) return;
            m |= cand_[c];
        }
        if (bits(m) != static_cast<int>(cells.size()) + 1) return;
        alss.push_back({std::move(cells), m});
    };

    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] == 0 && bits(cand_[i]) == 2) {
            addAls({i});
        }
    }

    for (const auto& h : houses_) {
        std::vector<int> unsolved;
        for (int i : h) if (grid_[i] == 0) unsolved.push_back(i);
        if (unsolved.size() >= 2U) {
            forEachCombo(unsolved, 2, [&](const std::vector<int>& cs) { addAls(cs); });
        }
        if (unsolved.size() >= 3U) {
            forEachCombo(unsolved, 3, [&](const std::vector<int>& cs) { addAls(cs); });
        }
    }

    auto hasCell = [&](const ALS& a, int c) {
        return std::find(a.cells.begin(), a.cells.end(), c) != a.cells.end();
    };
    auto seesAll = [&](int c, const std::vector<int>& xs) {
        for (int x : xs) if (!isPeerCell(c, x)) return false;
        return true;
    };

    for (std::size_t i = 0; i < alss.size(); ++i) {
        for (std::size_t j = i + 1; j < alss.size(); ++j) {
            const ALS& A = alss[i];
            const ALS& B = alss[j];

            bool disjoint = true;
            for (int c : A.cells) {
                if (hasCell(B, c)) { disjoint = false; break; }
            }
            if (!disjoint) continue;

            const uint64_t common = A.mask & B.mask;
            if (bits(common) < 2) continue;

            FOR_EACH_BIT(common, x) {
                std::vector<int> ax, bx;
                for (int c : A.cells) if (cand_[c] & bit(x)) ax.push_back(c);
                for (int c : B.cells) if (cand_[c] & bit(x)) bx.push_back(c);
                if (ax.empty() || bx.empty()) continue;

                bool restricted = true;
                for (int ca : ax) {
                    for (int cb : bx) {
                        if (!isPeerCell(ca, cb)) {
                            restricted = false;
                            break;
                        }
                    }
                    if (!restricted) break;
                }
                if (!restricted) continue;

                uint64_t zMask = common & ~bit(x);
                while (zMask) {
                    const uint64_t one = zMask & (~zMask + 1ULL);
                    const int z = firstDigit(one);
                    zMask &= (zMask - 1ULL);

                    std::vector<int> az, bz;
                    for (int c : A.cells) if (cand_[c] & bit(z)) az.push_back(c);
                    for (int c : B.cells) if (cand_[c] & bit(z)) bz.push_back(c);
                    if (az.empty() || bz.empty()) continue;

                    int local = 0;
                    for (int c = 0; c < NN_; ++c) {
                        if (grid_[c] != 0 || (cand_[c] & bit(z)) == 0ULL) continue;
                        if (hasCell(A, c) || hasCell(B, c)) continue;
                        if (!seesAll(c, az) || !seesAll(c, bz)) continue;
                        bool changed = false;
                        if (!removeCandidate(c, z, changed)) return false;
                        if (changed) ++local;
                    }
                    if (local > 0) {
                        n = local;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool SudokuAnalyzer::applyALSXYWing(int& n) {
    n = 0;

    struct ALS {
        std::vector<int> cells;
        uint64_t mask = 0ULL;
    };
    std::vector<ALS> alss;
    std::set<std::string> seen;
    auto addAls = [&](std::vector<int> cells) {
        if (cells.empty() || cells.size() > 3U) return;
        std::sort(cells.begin(), cells.end());
        std::ostringstream key;
        for (int c : cells) key << c << ",";
        if (!seen.insert(key.str()).second) return;
        uint64_t m = 0ULL;
        for (int c : cells) {
            if (grid_[c] != 0) return;
            m |= cand_[c];
        }
        if (bits(m) != static_cast<int>(cells.size()) + 1) return;
        alss.push_back({std::move(cells), m});
    };
    for (int i = 0; i < NN_; ++i) if (grid_[i] == 0 && bits(cand_[i]) == 2) addAls({i});
    for (const auto& h : houses_) {
        std::vector<int> uns;
        for (int i : h) if (grid_[i] == 0) uns.push_back(i);
        if (uns.size() >= 2U) forEachCombo(uns, 2, [&](const std::vector<int>& cs) { addAls(cs); });
        if (uns.size() >= 3U) forEachCombo(uns, 3, [&](const std::vector<int>& cs) { addAls(cs); });
    }
    if (alss.size() > 260U) alss.resize(260U);

    auto hasCell = [&](const ALS& a, int c) {
        return std::find(a.cells.begin(), a.cells.end(), c) != a.cells.end();
    };
    auto disjoint3 = [&](const ALS& A, const ALS& B, const ALS& C) {
        for (int c : A.cells) if (hasCell(B, c) || hasCell(C, c)) return false;
        for (int c : B.cells) if (hasCell(C, c)) return false;
        return true;
    };
    auto digitCells = [&](const ALS& a, int d) {
        std::vector<int> out;
        for (int c : a.cells) if (cand_[c] & bit(d)) out.push_back(c);
        return out;
    };
    auto isRCC = [&](const ALS& A, const ALS& B, int d) {
        const std::vector<int> da = digitCells(A, d);
        const std::vector<int> db = digitCells(B, d);
        if (da.empty() || db.empty()) return false;
        for (int ca : da) for (int cb : db) if (!isPeerCell(ca, cb)) return false;
        return true;
    };
    auto seesAll = [&](int cell, const std::vector<int>& xs) {
        for (int x : xs) if (!isPeerCell(cell, x)) return false;
        return true;
    };

    for (std::size_t i = 0; i < alss.size(); ++i) {
        for (std::size_t j = 0; j < alss.size(); ++j) {
            if (j == i) continue;
            for (std::size_t k = 0; k < alss.size(); ++k) {
                if (k == i || k == j) continue;
                const ALS& A = alss[i];
                const ALS& B = alss[j];
                const ALS& C = alss[k];
                if (!disjoint3(A, B, C)) continue;

                const uint64_t ab = A.mask & B.mask;
                const uint64_t ac = A.mask & C.mask;
                const uint64_t bc = B.mask & C.mask;
                if (ab == 0U || ac == 0U || bc == 0ULL) continue;

                FOR_EACH_BIT(ab, x) {
                    if (!isRCC(A, B, x)) continue;
                    FOR_EACH_BIT(ac, y) {
                        if (y == x || !isRCC(A, C, y)) continue;
                        uint64_t zMask = bc & ~bit(x) & ~bit(y);
                        while (zMask) {
                            const uint64_t one = zMask & (~zMask + 1ULL);
                            const int z = firstDigit(one);
                            zMask &= (zMask - 1ULL);
                            const std::vector<int> bz = digitCells(B, z);
                            const std::vector<int> cz = digitCells(C, z);
                            if (bz.empty() || cz.empty()) continue;

                            int local = 0;
                            for (int cell = 0; cell < NN_; ++cell) {
                                if (grid_[cell] != 0 || (cand_[cell] & bit(z)) == 0ULL) continue;
                                if (hasCell(A, cell) || hasCell(B, cell) || hasCell(C, cell)) continue;
                                if (!seesAll(cell, bz) || !seesAll(cell, cz)) continue;
                                bool changed = false;
                                if (!removeCandidate(cell, z, changed)) return false;
                                if (changed) ++local;
                            }
                            if (local > 0) {
                                std::ostringstream ss;
                                ss << "ALS-XY-Wing: remove " << z << " from " << local << " cell(s)";
                                pushDebugLog(ss.str());
                                n = local;
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyALSChain(int& n) {
    n = 0;

    struct ALS {
        std::vector<int> cells;
        uint64_t mask = 0ULL;
    };
    std::vector<ALS> alss;
    std::set<std::string> seen;
    auto addAls = [&](std::vector<int> cells) {
        if (cells.empty() || cells.size() > 3U) return;
        std::sort(cells.begin(), cells.end());
        std::ostringstream key;
        for (int c : cells) key << c << ",";
        if (!seen.insert(key.str()).second) return;
        uint64_t m = 0ULL;
        for (int c : cells) {
            if (grid_[c] != 0) return;
            m |= cand_[c];
        }
        if (bits(m) != static_cast<int>(cells.size()) + 1) return;
        alss.push_back({std::move(cells), m});
    };
    for (int i = 0; i < NN_; ++i) if (grid_[i] == 0 && bits(cand_[i]) == 2) addAls({i});
    for (const auto& h : houses_) {
        std::vector<int> uns;
        for (int i : h) if (grid_[i] == 0) uns.push_back(i);
        if (uns.size() >= 2U) forEachCombo(uns, 2, [&](const std::vector<int>& cs) { addAls(cs); });
        if (uns.size() >= 3U) forEachCombo(uns, 3, [&](const std::vector<int>& cs) { addAls(cs); });
    }
    if (alss.size() > 260U) alss.resize(260U);

    auto hasCell = [&](const ALS& a, int c) {
        return std::find(a.cells.begin(), a.cells.end(), c) != a.cells.end();
    };
    auto disjoint3 = [&](const ALS& A, const ALS& B, const ALS& C) {
        for (int c : A.cells) if (hasCell(B, c) || hasCell(C, c)) return false;
        for (int c : B.cells) if (hasCell(C, c)) return false;
        return true;
    };
    auto digitCells = [&](const ALS& a, int d) {
        std::vector<int> out;
        for (int c : a.cells) if (cand_[c] & bit(d)) out.push_back(c);
        return out;
    };
    auto isRCC = [&](const ALS& A, const ALS& B, int d) {
        const std::vector<int> da = digitCells(A, d);
        const std::vector<int> db = digitCells(B, d);
        if (da.empty() || db.empty()) return false;
        for (int ca : da) for (int cb : db) if (!isPeerCell(ca, cb)) return false;
        return true;
    };
    auto seesAll = [&](int cell, const std::vector<int>& xs) {
        for (int x : xs) if (!isPeerCell(cell, x)) return false;
        return true;
    };

    for (std::size_t i = 0; i < alss.size(); ++i) {
        for (std::size_t j = 0; j < alss.size(); ++j) {
            if (j == i) continue;
            for (std::size_t k = 0; k < alss.size(); ++k) {
                if (k == i || k == j) continue;
                const ALS& A = alss[i];
                const ALS& B = alss[j];
                const ALS& C = alss[k];
                if (!disjoint3(A, B, C)) continue;

                const uint64_t ab = A.mask & B.mask;
                const uint64_t bc = B.mask & C.mask;
                const uint64_t ac = A.mask & C.mask;
                if (ab == 0U || bc == 0U || ac == 0ULL) continue;

                FOR_EACH_BIT(ab, x) {
                    if (!isRCC(A, B, x)) continue;
                    FOR_EACH_BIT(bc, y) {
                        if (y == x || !isRCC(B, C, y)) continue;
                        uint64_t zMask = ac & ~bit(x) & ~bit(y);
                        while (zMask) {
                            const uint64_t one = zMask & (~zMask + 1ULL);
                            const int z = firstDigit(one);
                            zMask &= (zMask - 1ULL);
                            const std::vector<int> az = digitCells(A, z);
                            const std::vector<int> cz = digitCells(C, z);
                            if (az.empty() || cz.empty()) continue;

                            int local = 0;
                            for (int cell = 0; cell < NN_; ++cell) {
                                if (grid_[cell] != 0 || (cand_[cell] & bit(z)) == 0ULL) continue;
                                if (hasCell(A, cell) || hasCell(B, cell) || hasCell(C, cell)) continue;
                                if (!seesAll(cell, az) || !seesAll(cell, cz)) continue;
                                bool changed = false;
                                if (!removeCandidate(cell, z, changed)) return false;
                                if (changed) ++local;
                            }
                            if (local > 0) {
                                std::ostringstream ss;
                                ss << "ALS-Chain: remove " << z << " from " << local << " cell(s)";
                                pushDebugLog(ss.str());
                                n = local;
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyDeathBlossom(int& n) {
    n = 0;
    if (N_ != 9) return false;

    struct ALS {
        std::vector<int> cells;
        uint64_t mask = 0ULL;
    };
    std::vector<ALS> alss;
    std::set<std::string> seen;
    auto addAls = [&](std::vector<int> cells) {
        if (cells.empty() || cells.size() > 3U) return;
        std::sort(cells.begin(), cells.end());
        std::ostringstream key;
        for (int c : cells) key << c << ",";
        if (!seen.insert(key.str()).second) return;
        uint64_t m = 0ULL;
        for (int c : cells) {
            if (grid_[c] != 0) return;
            m |= cand_[c];
        }
        if (bits(m) != static_cast<int>(cells.size()) + 1) return;
        alss.push_back({std::move(cells), m});
    };
    for (int i = 0; i < NN_; ++i) if (grid_[i] == 0 && bits(cand_[i]) == 2) addAls({i});
    for (const auto& h : houses_) {
        std::vector<int> uns;
        for (int i : h) if (grid_[i] == 0) uns.push_back(i);
        if (uns.size() >= 2U) forEachCombo(uns, 2, [&](const std::vector<int>& cs) { addAls(cs); });
        if (uns.size() >= 3U) forEachCombo(uns, 3, [&](const std::vector<int>& cs) { addAls(cs); });
    }
    if (alss.size() > 220U) alss.resize(220U);

    auto hasCell = [&](const ALS& a, int c) {
        return std::find(a.cells.begin(), a.cells.end(), c) != a.cells.end();
    };
    auto digitCells = [&](const ALS& a, int d) {
        std::vector<int> out;
        for (int c : a.cells) if (cand_[c] & bit(d)) out.push_back(c);
        return out;
    };
    auto rccPivot = [&](int pivot, const ALS& a, int d) {
        const std::vector<int> ds = digitCells(a, d);
        if (ds.empty()) return false;
        for (int c : ds) if (!isPeerCell(pivot, c)) return false;
        return true;
    };
    auto seesAll = [&](int cell, const std::vector<int>& xs) {
        for (int x : xs) if (!isPeerCell(cell, x)) return false;
        return true;
    };

    for (int pivot = 0; pivot < NN_; ++pivot) {
        if (grid_[pivot] != 0 || bits(cand_[pivot]) != 3) continue;
        const std::vector<int> pd = digitsFromMask(cand_[pivot]);
        if (pd.size() != 3U) continue;

        for (int z = 1; z <= N_; ++z) {
            if ((cand_[pivot] & bit(z)) != 0ULL) continue;

            std::vector<int> p0, p1, p2;
            for (int ai = 0; ai < static_cast<int>(alss.size()); ++ai) {
                const ALS& A = alss[ai];
                if (hasCell(A, pivot)) continue;
                if ((A.mask & bit(z)) == 0ULL) continue;
                if ((A.mask & bit(pd[0])) && rccPivot(pivot, A, pd[0])) p0.push_back(ai);
                if ((A.mask & bit(pd[1])) && rccPivot(pivot, A, pd[1])) p1.push_back(ai);
                if ((A.mask & bit(pd[2])) && rccPivot(pivot, A, pd[2])) p2.push_back(ai);
            }
            if (p0.empty() || p1.empty() || p2.empty()) continue;

            for (int a : p0) for (int b : p1) for (int c : p2) {
                if (a == b || b == c || a == c) continue;
                const ALS& A0 = alss[a];
                const ALS& A1 = alss[b];
                const ALS& A2 = alss[c];

                bool disjoint = true;
                for (int v : A0.cells) if (hasCell(A1, v) || hasCell(A2, v)) { disjoint = false; break; }
                if (!disjoint) continue;
                for (int v : A1.cells) if (hasCell(A2, v)) { disjoint = false; break; }
                if (!disjoint) continue;

                const std::vector<int> z0 = digitCells(A0, z);
                const std::vector<int> z1 = digitCells(A1, z);
                const std::vector<int> z2 = digitCells(A2, z);
                if (z0.empty() || z1.empty() || z2.empty()) continue;

                int local = 0;
                for (int cell = 0; cell < NN_; ++cell) {
                    if (grid_[cell] != 0 || (cand_[cell] & bit(z)) == 0ULL) continue;
                    if (cell == pivot || hasCell(A0, cell) || hasCell(A1, cell) || hasCell(A2, cell)) continue;
                    if (!seesAll(cell, z0) || !seesAll(cell, z1) || !seesAll(cell, z2)) continue;
                    bool changed = false;
                    if (!removeCandidate(cell, z, changed)) return false;
                    if (changed) ++local;
                }
                if (local > 0) {
                    std::ostringstream ss;
                    ss << "DeathBlossom: pivot " << cellName(pivot) << " remove " << z
                       << " from " << local << " cell(s)";
                    pushDebugLog(ss.str());
                    n = local;
                    return true;
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applySueDeCoq(int& n) {
    n = 0;

    auto tryMode = [&](bool rowMode) -> bool {
        for (int b = 0; b < N_; ++b) {
            const int bpr = N_ / BC_;
            const int sr = (b / bpr) * BR_;
            const int sc = (b % bpr) * BC_;

            for (int off = 0; off < (rowMode ? BR_ : BC_); ++off) {
                const int line = (rowMode ? (sr + off) : (sc + off));
                std::vector<int> I;
                for (int d1 = 0; d1 < (rowMode ? BC_ : BR_); ++d1) {
                    const int r = rowMode ? line : (sr + d1);
                    const int c = rowMode ? (sc + d1) : line;
                    const int idx = r * N_ + c;
                    if (grid_[idx] == 0) I.push_back(idx);
                }
                if (I.size() != 2U) continue;

                uint64_t mI = 0ULL;
                for (int c : I) mI |= cand_[c];

                std::vector<int> linePool;
                const std::vector<int>& lineHouse = rowMode ? houses_[line] : houses_[N_ + line];
                for (int idx : lineHouse) {
                    if (grid_[idx] != 0) continue;
                    if (box(row(idx), col(idx)) == b) continue;
                    linePool.push_back(idx);
                }
                std::vector<int> boxPool;
                const std::vector<int>& boxHouse = houses_[2 * N_ + b];
                for (int idx : boxHouse) {
                    if (grid_[idx] != 0) continue;
                    if ((rowMode && row(idx) == line) || (!rowMode && col(idx) == line)) continue;
                    boxPool.push_back(idx);
                }

                if (linePool.empty() || boxPool.empty()) continue;

                for (int kL = 1; kL <= std::min(2, static_cast<int>(linePool.size())); ++kL) {
                    for (int kB = 1; kB <= std::min(2, static_cast<int>(boxPool.size())); ++kB) {
                        bool found = false;
                        forEachCombo(linePool, kL, [&](const std::vector<int>& Lset) {
                            if (found || contradiction_) return;
                            uint64_t mL = 0ULL;
                            for (int c : Lset) mL |= cand_[c];

                            forEachCombo(boxPool, kB, [&](const std::vector<int>& Bset) {
                                if (found || contradiction_) return;
                                uint64_t mB = 0ULL;
                                for (int c : Bset) mB |= cand_[c];

                                if ((mL & ~mI) == 0U || (mB & ~mI) == 0ULL) return;
                                if (((mL & mB) & ~mI) != 0ULL) return;

                                const uint64_t mIL = mI | mL;
                                const uint64_t mIB = mI | mB;
                                const uint64_t mAll = mI | mL | mB;
                                if (bits(mIL) != static_cast<int>(I.size()) + static_cast<int>(Lset.size())) return;
                                if (bits(mIB) != static_cast<int>(I.size()) + static_cast<int>(Bset.size())) return;
                                if (bits(mAll) != static_cast<int>(I.size()) + static_cast<int>(Lset.size()) + static_cast<int>(Bset.size())) return;

                                std::set<int> Iset(I.begin(), I.end());
                                std::set<int> Lmark(Lset.begin(), Lset.end());
                                std::set<int> Bmark(Bset.begin(), Bset.end());

                                int local = 0;
                                for (int idx : lineHouse) {
                                    if (grid_[idx] != 0) continue;
                                    if (Iset.count(idx) || Lmark.count(idx)) continue;
                                    uint64_t rm = cand_[idx] & mIL;
                                    while (rm) {
                                        const uint64_t one = rm & (~rm + 1ULL);
                                        bool changed = false;
                                        if (!removeCandidate(idx, firstDigit(one), changed)) return;
                                        if (changed) ++local;
                                        rm &= (rm - 1ULL);
                                    }
                                }
                                for (int idx : boxHouse) {
                                    if (grid_[idx] != 0) continue;
                                    if (Iset.count(idx) || Bmark.count(idx)) continue;
                                    uint64_t rm = cand_[idx] & mIB;
                                    while (rm) {
                                        const uint64_t one = rm & (~rm + 1ULL);
                                        bool changed = false;
                                        if (!removeCandidate(idx, firstDigit(one), changed)) return;
                                        if (changed) ++local;
                                        rm &= (rm - 1ULL);
                                    }
                                }
                                if (local > 0) {
                                    n = local;
                                    found = true;
                                }
                            });
                        });
                        if (found) return true;
                    }
                }
            }
        }
        return false;
    };

    if (tryMode(true)) return true;
    return tryMode(false);
}

bool SudokuAnalyzer::applyMSLS(int& n) {
    n = 0;
    if (N_ != 9) return false;

    auto tryMode = [&](bool rowMode) -> bool {
        for (int b = 0; b < N_; ++b) {
            const int bpr = N_ / BC_;
            const int sr = (b / bpr) * BR_;
            const int sc = (b % bpr) * BC_;

            for (int off = 0; off < (rowMode ? BR_ : BC_); ++off) {
                const int line = rowMode ? (sr + off) : (sc + off);

                std::vector<int> inter;
                for (int k = 0; k < (rowMode ? BC_ : BR_); ++k) {
                    const int r = rowMode ? line : (sr + k);
                    const int c = rowMode ? (sc + k) : line;
                    const int idx = r * N_ + c;
                    if (grid_[idx] == 0) inter.push_back(idx);
                }
                if (inter.size() < 2U) continue;

                const std::vector<int>& lineHouse = rowMode ? houses_[line] : houses_[N_ + line];
                const std::vector<int>& boxHouse = houses_[2 * N_ + b];

                std::vector<int> linePool;
                for (int idx : lineHouse) {
                    if (grid_[idx] != 0) continue;
                    if (box(row(idx), col(idx)) == b) continue;
                    linePool.push_back(idx);
                }
                std::vector<int> boxPool;
                for (int idx : boxHouse) {
                    if (grid_[idx] != 0) continue;
                    if ((rowMode && row(idx) == line) || (!rowMode && col(idx) == line)) continue;
                    boxPool.push_back(idx);
                }
                if (linePool.empty() || boxPool.empty()) continue;

                const int iMax = std::min(3, static_cast<int>(inter.size()));
                for (int isz = 2; isz <= iMax; ++isz) {
                    bool found = false;
                    forEachCombo(inter, isz, [&](const std::vector<int>& Iset) {
                        if (found || contradiction_) return;
                        uint64_t mI = 0ULL;
                        for (int c : Iset) mI |= cand_[c];

                        for (int kL = 1; kL <= std::min(3, static_cast<int>(linePool.size())); ++kL) {
                            forEachCombo(linePool, kL, [&](const std::vector<int>& Lset) {
                                if (found || contradiction_) return;
                                uint64_t mL = 0ULL;
                                for (int c : Lset) mL |= cand_[c];

                                for (int kB = 1; kB <= std::min(3, static_cast<int>(boxPool.size())); ++kB) {
                                    forEachCombo(boxPool, kB, [&](const std::vector<int>& Bset) {
                                        if (found || contradiction_) return;
                                        uint64_t mB = 0ULL;
                                        for (int c : Bset) mB |= cand_[c];

                                        if ((mL & ~mI) == 0U || (mB & ~mI) == 0ULL) return;
                                        if (((mL & mB) & ~mI) != 0ULL) return;

                                        const uint64_t mIL = mI | mL;
                                        const uint64_t mIB = mI | mB;
                                        const uint64_t mAll = mI | mL | mB;
                                        if (bits(mIL) != static_cast<int>(Iset.size()) + static_cast<int>(Lset.size())) return;
                                        if (bits(mIB) != static_cast<int>(Iset.size()) + static_cast<int>(Bset.size())) return;
                                        if (bits(mAll) != static_cast<int>(Iset.size()) + static_cast<int>(Lset.size()) + static_cast<int>(Bset.size())) return;

                                        std::set<int> Imark(Iset.begin(), Iset.end());
                                        std::set<int> Lmark(Lset.begin(), Lset.end());
                                        std::set<int> Bmark(Bset.begin(), Bset.end());

                                        int local = 0;
                                        for (int idx : lineHouse) {
                                            if (grid_[idx] != 0) continue;
                                            if (Imark.count(idx) || Lmark.count(idx)) continue;
                                            uint64_t rm = cand_[idx] & mIL;
                                            while (rm) {
                                                const uint64_t one = rm & (~rm + 1ULL);
                                                bool changed = false;
                                                if (!removeCandidate(idx, firstDigit(one), changed)) return;
                                                if (changed) ++local;
                                                rm &= (rm - 1ULL);
                                            }
                                        }
                                        for (int idx : boxHouse) {
                                            if (grid_[idx] != 0) continue;
                                            if (Imark.count(idx) || Bmark.count(idx)) continue;
                                            uint64_t rm = cand_[idx] & mIB;
                                            while (rm) {
                                                const uint64_t one = rm & (~rm + 1ULL);
                                                bool changed = false;
                                                if (!removeCandidate(idx, firstDigit(one), changed)) return;
                                                if (changed) ++local;
                                                rm &= (rm - 1ULL);
                                            }
                                        }
                                        if (local > 0) {
                                            std::ostringstream ss;
                                            ss << "MSLS: line/box sector elimination removed " << local << " candidate(s)";
                                            pushDebugLog(ss.str());
                                            n = local;
                                            found = true;
                                        }
                                    });
                                }
                            });
                        }
                    });
                    if (found) return true;
                }
            }
        }
        return false;
    };

    if (tryMode(true)) return true;
    return tryMode(false);
}

bool SudokuAnalyzer::applyExocet(int& n) {
    n = 0;
    if (N_ < 6) return false;

    std::vector<int> unsolved;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] == 0 && bits(cand_[i]) >= 2) unsolved.push_back(i);
    }
    if (unsolved.size() < 6U) return false;

    struct Perm {
        int b1d = 0;
        int b2d = 0;
        int expect1 = 0;
        int expect2 = 0;
    };

    for (std::size_t i = 0; i < unsolved.size(); ++i) {
        for (std::size_t j = i + 1; j < unsolved.size(); ++j) {
            const int b1 = unsolved[i];
            const int b2 = unsolved[j];
            if (box(row(b1), col(b1)) != box(row(b2), col(b2))) continue;
            const int baseBox = box(row(b1), col(b1));

            const uint64_t common = cand_[b1] & cand_[b2];
            const std::vector<int> cd = digitsFromMask(common);
            if (cd.size() < 2U) continue;

            for (std::size_t d1 = 0; d1 < cd.size(); ++d1) {
                for (std::size_t d2 = d1 + 1; d2 < cd.size(); ++d2) {
                    const int x = cd[d1];
                    const int y = cd[d2];
                    const uint64_t pairMask = bit(x) | bit(y);
                    auto buildBranch = [&](int baseCell, bool useRow) -> std::vector<int> {
                        std::vector<int> out;
                        if (useRow) {
                            const int r = row(baseCell);
                            for (int idx : houses_[r]) {
                                if (grid_[idx] != 0) continue;
                                if (box(row(idx), col(idx)) == baseBox) continue;
                                if ((cand_[idx] & pairMask) == 0ULL) continue;
                                out.push_back(idx);
                            }
                        } else {
                            const int c = col(baseCell);
                            for (int idx : houses_[N_ + c]) {
                                if (grid_[idx] != 0) continue;
                                if (box(row(idx), col(idx)) == baseBox) continue;
                                if ((cand_[idx] & pairMask) == 0ULL) continue;
                                out.push_back(idx);
                            }
                        }
                        std::sort(out.begin(), out.end());
                        out.erase(std::unique(out.begin(), out.end()), out.end());
                        return out;
                    };

                    const std::vector<std::pair<bool, bool>> mapModes = {
                        {false, false}, // col-col
                        {true, true},   // row-row
                        {true, false},  // row-col
                        {false, true}   // col-row
                    };

                    for (const auto& mode : mapModes) {
                        const std::vector<int> branch1 = buildBranch(b1, mode.first);
                        const std::vector<int> branch2 = buildBranch(b2, mode.second);
                        if (branch1.size() < 2U || branch2.size() < 2U) continue;

                        bool disjoint = true;
                        for (int t : branch1) {
                            if (std::find(branch2.begin(), branch2.end(), t) != branch2.end()) {
                                disjoint = false;
                                break;
                            }
                        }
                        if (!disjoint) continue;

                        std::set<int> targetSet(branch1.begin(), branch1.end());
                        targetSet.insert(branch2.begin(), branch2.end());
                        if (targetSet.size() > 14U) continue;

                        const std::string modeName =
                            std::string(mode.first ? "row" : "col") + "-" + (mode.second ? "row" : "col");
                        auto cellListText = [&](const std::vector<int>& cells) -> std::string {
                            std::ostringstream ss;
                            for (std::size_t k = 0; k < cells.size(); ++k) {
                                if (k > 0) ss << ",";
                                ss << cellName(cells[k]);
                            }
                            return ss.str();
                        };

                        std::map<std::string, bool> supportCache;
                        auto supported = [&](std::vector<std::pair<int, int>> asg) -> bool {
                            std::sort(asg.begin(), asg.end(), [](const auto& A, const auto& B) {
                                if (A.first != B.first) return A.first < B.first;
                                return A.second < B.second;
                            });
                            std::ostringstream key;
                            for (const auto& p : asg) key << p.first << "=" << p.second << ";";
                            const std::string k = key.str();
                            const auto it = supportCache.find(k);
                            if (it != supportCache.end()) return it->second;
                            const bool ok = hasLogicalSupportWithAssignments(asg);
                            supportCache[k] = ok;
                            return ok;
                        };

                        const std::vector<Perm> perms = {
                            {x, y, y, x},
                            {y, x, x, y}
                        };
                        std::vector<Perm> feasiblePerms;
                        for (const Perm& p : perms) {
                            const std::vector<std::pair<int, int>> bases = {
                                {b1, p.b1d}, {b2, p.b2d}
                            };
                            if (!supported(bases)) continue;

                            bool support1 = false;
                            for (int t : branch1) {
                                if ((cand_[t] & bit(p.expect1)) == 0ULL) continue;
                                std::vector<std::pair<int, int>> asg = bases;
                                asg.push_back({t, p.expect1});
                                if (supported(asg)) { support1 = true; break; }
                            }
                            if (!support1) continue;

                            bool support2 = false;
                            for (int t : branch2) {
                                if ((cand_[t] & bit(p.expect2)) == 0ULL) continue;
                                std::vector<std::pair<int, int>> asg = bases;
                                asg.push_back({t, p.expect2});
                                if (supported(asg)) { support2 = true; break; }
                            }
                            if (!support2) continue;

                            feasiblePerms.push_back(p);
                        }
                        if (feasiblePerms.empty()) continue;

                        bool patternLogged = false;
                        auto ensurePatternLogged = [&]() {
                            if (patternLogged) return;
                            std::ostringstream permSs;
                            for (std::size_t pi = 0; pi < feasiblePerms.size(); ++pi) {
                                if (pi > 0) permSs << " | ";
                                permSs << "("
                                       << cellName(b1) << "=" << feasiblePerms[pi].b1d << ", "
                                       << cellName(b2) << "=" << feasiblePerms[pi].b2d << ")";
                            }
                            std::ostringstream head;
                            head << "Exocet pattern: base{" << cellName(b1) << "," << cellName(b2)
                                 << "}, pair={" << x << "," << y << "}, mode=" << modeName
                                 << ", branch1={" << cellListText(branch1) << "}, branch2={"
                                 << cellListText(branch2) << "}, feasible=" << permSs.str();
                            pushDebugLog(head.str());
                            patternLogged = true;
                        };

                        int local = 0;
                        auto eliminateInBranch = [&](const std::vector<int>& branch, int which) -> bool {
                            for (int t : branch) {
                                if (grid_[t] != 0) continue;
                                const std::vector<int> dlist = digitsFromMask(cand_[t]);
                                for (int d : dlist) {
                                    bool supp = false;
                                    for (const Perm& p : feasiblePerms) {
                                        const int expected = (which == 1) ? p.expect1 : p.expect2;
                                        if ((d == x || d == y) && d != expected) continue;
                                        std::vector<std::pair<int, int>> asg = {
                                            {b1, p.b1d}, {b2, p.b2d}, {t, d}
                                        };
                                        if (supported(asg)) { supp = true; break; }
                                    }
                                    if (!supp) {
                                        // Konserwatywnie: usuwaj tylko kandydaty bez wsparcia implikacyjnego.
                                        if (supported({{t, d}})) continue;
                                        bool changed = false;
                                        if (!removeCandidate(t, d, changed)) return false;
                                        if (changed) {
                                            ensurePatternLogged();
                                            std::ostringstream ss;
                                            ss << "Exocet: remove " << d << " from " << cellName(t)
                                               << " (branch " << which << ", mode=" << modeName << ")";
                                            pushDebugLog(ss.str());
                                            ++local;
                                        }
                                    }
                                }
                            }
                            return true;
                        };

                        if (!eliminateInBranch(branch1, 1)) return false;
                        if (!eliminateInBranch(branch2, 2)) return false;

                        for (int t : targetSet) {
                            if (grid_[t] != 0) continue;
                            uint64_t extras = cand_[t] & ~pairMask;
                            while (extras) {
                                const uint64_t one = extras & (~extras + 1ULL);
                                const int z = firstDigit(one);
                                bool supp = false;
                                for (const Perm& p : feasiblePerms) {
                                    std::vector<std::pair<int, int>> asg = {
                                        {b1, p.b1d}, {b2, p.b2d}, {t, z}
                                    };
                                    if (supported(asg)) { supp = true; break; }
                                }
                                if (!supp) {
                                    if (supported({{t, z}})) {
                                        extras &= (extras - 1ULL);
                                        continue;
                                    }
                                    bool changed = false;
                                    if (!removeCandidate(t, z, changed)) return false;
                                    if (changed) {
                                        ensurePatternLogged();
                                        std::ostringstream ss;
                                        ss << "Exocet: remove " << z << " from " << cellName(t)
                                           << " (target extra, mode=" << modeName << ")";
                                        pushDebugLog(ss.str());
                                        ++local;
                                    }
                                }
                                extras &= (extras - 1ULL);
                            }
                        }

                        if (local > 0) {
                            n = local;
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applySeniorExocet(int& n) {
    n = 0;
    if (N_ < 6) return false;

    std::vector<int> unsolved;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] == 0 && bits(cand_[i]) >= 2) unsolved.push_back(i);
    }
    if (unsolved.size() < 6U) return false;

    struct Perm {
        int b1d = 0;
        int b2d = 0;
        int expect1 = 0;
        int expect2 = 0;
    };

    for (std::size_t i = 0; i < unsolved.size(); ++i) {
        for (std::size_t j = i + 1; j < unsolved.size(); ++j) {
            const int b1 = unsolved[i];
            const int b2 = unsolved[j];
            if (box(row(b1), col(b1)) != box(row(b2), col(b2))) continue;
            const int baseBox = box(row(b1), col(b1));

            const uint64_t common = cand_[b1] & cand_[b2];
            const std::vector<int> cd = digitsFromMask(common);
            if (cd.size() < 2U) continue;

            for (std::size_t d1 = 0; d1 < cd.size(); ++d1) {
                for (std::size_t d2 = d1 + 1; d2 < cd.size(); ++d2) {
                    const int x = cd[d1];
                    const int y = cd[d2];
                    const uint64_t pairMask = bit(x) | bit(y);

                    auto buildBranch = [&](int baseCell, bool useRow) -> std::vector<int> {
                        std::vector<int> out;
                        if (useRow) {
                            const int r = row(baseCell);
                            for (int idx : houses_[r]) {
                                if (grid_[idx] != 0) continue;
                                if (box(row(idx), col(idx)) == baseBox) continue;
                                if ((cand_[idx] & pairMask) == 0ULL) continue;
                                out.push_back(idx);
                            }
                        } else {
                            const int c = col(baseCell);
                            for (int idx : houses_[N_ + c]) {
                                if (grid_[idx] != 0) continue;
                                if (box(row(idx), col(idx)) == baseBox) continue;
                                if ((cand_[idx] & pairMask) == 0ULL) continue;
                                out.push_back(idx);
                            }
                        }
                        std::sort(out.begin(), out.end());
                        out.erase(std::unique(out.begin(), out.end()), out.end());
                        return out;
                    };

                    const std::vector<std::pair<bool, bool>> mapModes = {
                        {false, false}, {true, true}, {true, false}, {false, true}
                    };
                    for (const auto& mode : mapModes) {
                        const std::vector<int> branch1 = buildBranch(b1, mode.first);
                        const std::vector<int> branch2 = buildBranch(b2, mode.second);
                        if (branch1.size() < 2U || branch2.size() < 2U) continue;

                        bool disjoint = true;
                        for (int t : branch1) {
                            if (std::find(branch2.begin(), branch2.end(), t) != branch2.end()) {
                                disjoint = false;
                                break;
                            }
                        }
                        if (!disjoint) continue;

                        std::map<std::string, bool> supportCache;
                        auto supported = [&](std::vector<std::pair<int, int>> asg) -> bool {
                            std::sort(asg.begin(), asg.end(), [](const auto& A, const auto& B) {
                                if (A.first != B.first) return A.first < B.first;
                                return A.second < B.second;
                            });
                            std::ostringstream key;
                            for (const auto& p : asg) key << p.first << "=" << p.second << ";";
                            const std::string k = key.str();
                            const auto it = supportCache.find(k);
                            if (it != supportCache.end()) return it->second;
                            const bool ok = hasLogicalSupportWithAssignments(asg);
                            supportCache[k] = ok;
                            return ok;
                        };

                        const std::vector<Perm> perms = {
                            {x, y, y, x},
                            {y, x, x, y}
                        };
                        std::vector<Perm> feasiblePerms;
                        for (const Perm& p : perms) {
                            const std::vector<std::pair<int, int>> bases = {
                                {b1, p.b1d}, {b2, p.b2d}
                            };
                            if (!supported(bases)) continue;

                            bool support1 = false;
                            for (int t : branch1) {
                                if ((cand_[t] & bit(p.expect1)) == 0ULL) continue;
                                std::vector<std::pair<int, int>> asg = bases;
                                asg.push_back({t, p.expect1});
                                if (supported(asg)) { support1 = true; break; }
                            }
                            if (!support1) continue;

                            bool support2 = false;
                            for (int t : branch2) {
                                if ((cand_[t] & bit(p.expect2)) == 0ULL) continue;
                                std::vector<std::pair<int, int>> asg = bases;
                                asg.push_back({t, p.expect2});
                                if (supported(asg)) { support2 = true; break; }
                            }
                            if (!support2) continue;

                            feasiblePerms.push_back(p);
                        }
                        if (feasiblePerms.size() != 1ULL) continue;
                        const Perm p = feasiblePerms[0];

                        int local = 0;
                        const int bad1 = (p.b1d == x) ? y : x;
                        const int bad2 = (p.b2d == x) ? y : x;
                        bool ch1 = false, ch2 = false;
                        if (!removeCandidate(b1, bad1, ch1)) return false;
                        if (ch1) ++local;
                        if (!removeCandidate(b2, bad2, ch2)) return false;
                        if (ch2) ++local;

                        if (local > 0) {
                            std::ostringstream ss;
                            ss << "SeniorExocet: fixed base pair "
                               << cellName(b1) << "=" << p.b1d << ", "
                               << cellName(b2) << "=" << p.b2d;
                            pushDebugLog(ss.str());
                            n = local;
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool SudokuAnalyzer::applyPatternOverlayMethod(int& n) {
    n = 0;
    if (N_ != 9) return false;

    for (int d = 1; d <= N_; ++d) {
        std::vector<std::vector<int>> rowOpts(N_);
        bool impossible = false;
        for (int r = 0; r < N_; ++r) {
            int fixedCol = -1;
            for (int c = 0; c < N_; ++c) {
                if (grid_[r * N_ + c] == d) {
                    fixedCol = c;
                    break;
                }
            }
            if (fixedCol >= 0) {
                rowOpts[r].push_back(fixedCol);
            } else {
                for (int c = 0; c < N_; ++c) {
                    const int idx = r * N_ + c;
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) rowOpts[r].push_back(c);
                }
            }
            if (rowOpts[r].empty()) {
                impossible = true;
                break;
            }
        }
        if (impossible) continue;

        std::vector<int> rowOrder(N_);
        for (int r = 0; r < N_; ++r) rowOrder[r] = r;
        std::sort(rowOrder.begin(), rowOrder.end(), [&](int a, int b) {
            return rowOpts[a].size() < rowOpts[b].size();
        });

        std::vector<int> chosenCol(N_, -1);
        std::vector<char> canBeTrue(NN_, 0);
        int patterns = 0;
        const int maxPatterns = 20000;
        bool overflow = false;

        std::function<void(int, int, int)> dfs = [&](int depth, int usedCols, int usedBoxes) {
            if (overflow) return;
            if (depth == N_) {
                ++patterns;
                for (int r = 0; r < N_; ++r) {
                    if (chosenCol[r] >= 0) canBeTrue[r * N_ + chosenCol[r]] = 1;
                }
                if (patterns >= maxPatterns) overflow = true;
                return;
            }

            const int r = rowOrder[depth];
            for (int c : rowOpts[r]) {
                const int b = box(r, c);
                if (usedCols & (1 << c)) continue;
                if (usedBoxes & (1 << b)) continue;
                chosenCol[r] = c;

                bool futureOk = true;
                for (int nd = depth + 1; nd < N_; ++nd) {
                    const int rr = rowOrder[nd];
                    bool any = false;
                    for (int cc : rowOpts[rr]) {
                        const int bb = box(rr, cc);
                        if ((usedCols & (1 << cc)) != 0) continue;
                        if ((usedBoxes & (1 << bb)) != 0) continue;
                        if (cc == c || bb == b) continue;
                        any = true;
                        break;
                    }
                    if (!any) {
                        futureOk = false;
                        break;
                    }
                }
                if (futureOk) dfs(depth + 1, usedCols | (1 << c), usedBoxes | (1 << b));
                chosenCol[r] = -1;
                if (overflow) return;
            }
        };
        dfs(0, 0, 0);
        if (overflow || patterns <= 0) continue;

        int local = 0;
        for (int idx = 0; idx < NN_; ++idx) {
            if (grid_[idx] != 0 || (cand_[idx] & bit(d)) == 0ULL) continue;
            if (canBeTrue[idx]) continue;
            bool changed = false;
            if (!removeCandidate(idx, d, changed)) return false;
            if (changed) ++local;
        }
        if (local > 0) {
            std::ostringstream ss;
            ss << "POM: digit " << d << " removed from " << local << " cell(s), patterns=" << patterns;
            pushDebugLog(ss.str());
            n = local;
            return true;
        }
    }
    return false;
}

bool SudokuAnalyzer::applyForcingChains(int& n) {
    n = 0;
    if (N_ != 9) return false;

    std::map<std::string, bool> supportCache;
    auto supported = [&](std::vector<std::pair<int, int>> asg) -> bool {
        std::sort(asg.begin(), asg.end(), [](const auto& A, const auto& B) {
            if (A.first != B.first) return A.first < B.first;
            return A.second < B.second;
        });
        std::ostringstream key;
        for (const auto& p : asg) key << p.first << "=" << p.second << ";";
        const std::string k = key.str();
        const auto it = supportCache.find(k);
        if (it != supportCache.end()) return it->second;
        const bool ok = hasLogicalSupportWithAssignments(asg);
        supportCache[k] = ok;
        return ok;
    };

    auto evalBranches = [&](const std::vector<std::pair<int, int>>& branches) -> bool {
        std::vector<std::pair<int, int>> feasible;
        feasible.reserve(branches.size());
        for (const auto& b : branches) {
            if (supported({b})) feasible.push_back(b);
        }
        if (feasible.size() < 2U) return false;

        std::vector<int> probeCells;
        probeCells.reserve(NN_);
        for (int i = 0; i < NN_; ++i) {
            if (grid_[i] == 0 && cand_[i] != 0ULL) {
                probeCells.push_back(i);
            }
        }

        for (int c : probeCells) {
            const std::vector<int> dlist = digitsFromMask(cand_[c]);
            for (int d : dlist) {
                bool supportedAnywhere = false;
                for (const auto& br : feasible) {
                    bool supp = false;
                    if (br.first == c) {
                        supp = (br.second == d);
                    } else {
                        supp = supported({br, {c, d}});
                    }
                    if (supp) {
                        supportedAnywhere = true;
                        break;
                    }
                }
                if (!supportedAnywhere) {
                    bool changed = false;
                    if (!removeCandidate(c, d, changed)) return false;
                    if (changed) {
                        std::ostringstream ss;
                        ss << "ForcingChains: remove " << d << " from " << cellName(c)
                           << " (all branches contradicted)";
                        pushDebugLog(ss.str());
                        n = 1;
                        return true;
                    }
                }
            }
        }
        return false;
    };

    for (int cell = 0; cell < NN_; ++cell) {
        if (grid_[cell] != 0) continue;
        const int bc = bits(cand_[cell]);
        if (bc < 2 || bc > 3) continue;
        std::vector<std::pair<int, int>> branches;
        FOR_EACH_BIT(cand_[cell], d) {
            branches.push_back({cell, d});
        }
        if (evalBranches(branches)) return true;
    }

    for (int h = 0; h < 3 * N_; ++h) {
        for (int d = 1; d <= N_; ++d) {
            std::vector<int> where;
            for (int idx : houses_[h]) {
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) where.push_back(idx);
            }
            if (where.size() < 2U || where.size() > 3U) continue;
            std::vector<std::pair<int, int>> branches;
            for (int idx : where) {
                branches.push_back({idx, d});
            }
            if (evalBranches(branches)) return true;
        }
    }

    return false;
}

bool SudokuAnalyzer::applyGroupedXCycle(int& n) {
    n = 0;

    auto eliminateSeeingBoth = [&](int a, int b, int d) -> bool {
        int local = 0;
        for (int i = 0; i < NN_; ++i) {
            if (i == a || i == b || grid_[i] != 0) continue;
            if ((cand_[i] & bit(d)) == 0ULL) continue;
            if (!isPeerCell(i, a) || !isPeerCell(i, b)) continue;
            bool changed = false;
            if (!removeCandidate(i, d, changed)) return false;
            if (changed) ++local;
        }
        if (local > 0) {
            n = local;
            return true;
        }
        return false;
    };

    for (int d = 1; d <= N_; ++d) {
        const int bpr = N_ / BC_;
        for (int b = 0; b < N_; ++b) {
            const int sr = (b / bpr) * BR_;
            const int sc = (b % bpr) * BC_;

            for (int rr = 0; rr < BR_; ++rr) {
                const int r = sr + rr;
                std::vector<int> group;
                for (int cc = 0; cc < BC_; ++cc) {
                    const int c = sc + cc;
                    const int idx = r * N_ + c;
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) group.push_back(idx);
                }
                if (group.size() < 2U) continue;

                std::vector<int> rowAll;
                for (int idx : houses_[r]) if (grid_[idx] == 0 && (cand_[idx] & bit(d))) rowAll.push_back(idx);
                if (rowAll.size() != group.size() + 1ULL) continue;
                int outsideRow = -1;
                for (int idx : rowAll) {
                    if (std::find(group.begin(), group.end(), idx) == group.end()) {
                        outsideRow = idx;
                        break;
                    }
                }
                if (outsideRow < 0) continue;

                std::vector<int> boxAll;
                for (int idx : houses_[2 * N_ + b]) if (grid_[idx] == 0 && (cand_[idx] & bit(d))) boxAll.push_back(idx);
                if (boxAll.size() != group.size() + 1ULL) continue;
                int outsideBox = -1;
                for (int idx : boxAll) {
                    if (std::find(group.begin(), group.end(), idx) == group.end()) {
                        outsideBox = idx;
                        break;
                    }
                }
                if (outsideBox < 0 || outsideBox == outsideRow) continue;

                if (eliminateSeeingBoth(outsideRow, outsideBox, d)) return true;
            }

            for (int cc = 0; cc < BC_; ++cc) {
                const int c = sc + cc;
                std::vector<int> group;
                for (int rr = 0; rr < BR_; ++rr) {
                    const int r = sr + rr;
                    const int idx = r * N_ + c;
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) group.push_back(idx);
                }
                if (group.size() < 2U) continue;

                std::vector<int> colAll;
                for (int idx : houses_[N_ + c]) if (grid_[idx] == 0 && (cand_[idx] & bit(d))) colAll.push_back(idx);
                if (colAll.size() != group.size() + 1ULL) continue;
                int outsideCol = -1;
                for (int idx : colAll) {
                    if (std::find(group.begin(), group.end(), idx) == group.end()) {
                        outsideCol = idx;
                        break;
                    }
                }
                if (outsideCol < 0) continue;

                std::vector<int> boxAll;
                for (int idx : houses_[2 * N_ + b]) if (grid_[idx] == 0 && (cand_[idx] & bit(d))) boxAll.push_back(idx);
                if (boxAll.size() != group.size() + 1ULL) continue;
                int outsideBox = -1;
                for (int idx : boxAll) {
                    if (std::find(group.begin(), group.end(), idx) == group.end()) {
                        outsideBox = idx;
                        break;
                    }
                }
                if (outsideBox < 0 || outsideBox == outsideCol) continue;

                if (eliminateSeeingBoth(outsideCol, outsideBox, d)) return true;
            }
        }
    }

    return false;
}

bool SudokuAnalyzer::applyGroupedAIC(int& n) {
    n = 0;
    if (N_ != 9) return false;

    auto branchText = [&](const std::vector<std::pair<int, int>>& branches) -> std::string {
        std::ostringstream ss;
        for (std::size_t i = 0; i < branches.size(); ++i) {
            if (i > 0) ss << " | ";
            ss << cellName(branches[i].first) << "=" << branches[i].second;
        }
        return ss.str();
    };

    std::map<std::string, bool> supportCache;
    auto supported = [&](std::vector<std::pair<int, int>> asg) -> bool {
        std::sort(asg.begin(), asg.end(), [](const auto& A, const auto& B) {
            if (A.first != B.first) return A.first < B.first;
            return A.second < B.second;
        });
        std::ostringstream key;
        for (const auto& p : asg) key << p.first << "=" << p.second << ";";
        const std::string k = key.str();
        const auto it = supportCache.find(k);
        if (it != supportCache.end()) return it->second;
        const bool ok = hasLogicalSupportWithAssignments(asg);
        supportCache[k] = ok;
        return ok;
    };

    auto evalBranches = [&](const std::vector<std::pair<int, int>>& branches) -> bool {
        std::vector<std::pair<int, int>> feasible;
        for (const auto& b : branches) {
            if (supported({b})) feasible.push_back(b);
        }
        if (feasible.size() < 2U) return false;

        for (int c = 0; c < NN_; ++c) {
            if (grid_[c] != 0) continue;
            FOR_EACH_BIT(cand_[c], d) {
                bool supp = false;
                for (const auto& br : feasible) {
                    if (br.first == c) {
                        if (br.second == d) { supp = true; break; }
                    } else if (supported({br, {c, d}})) {
                        supp = true;
                        break;
                    }
                }
                if (!supp) {
                    bool changed = false;
                    if (!removeCandidate(c, d, changed)) return false;
                    if (changed) {
                        std::ostringstream ss;
                        ss << "GroupedAIC: branches={" << branchText(feasible)
                           << "} -> remove " << d << " from " << cellName(c);
                        pushDebugLog(ss.str());
                        n = 1;
                        return true;
                    }
                }
            }
        }
        return false;
    };

    const int bpr = N_ / BC_;
    for (int d = 1; d <= N_; ++d) {
        for (int b = 0; b < N_; ++b) {
            const int sr = (b / bpr) * BR_;
            const int sc = (b % bpr) * BC_;

            for (int rr = 0; rr < BR_; ++rr) {
                const int r = sr + rr;
                std::vector<int> group;
                for (int cc = 0; cc < BC_; ++cc) {
                    const int idx = r * N_ + (sc + cc);
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) group.push_back(idx);
                }
                if (group.size() >= 2U && group.size() <= 4U) {
                    std::vector<std::pair<int, int>> branches;
                    for (int idx : group) branches.push_back({idx, d});
                    if (evalBranches(branches)) return true;
                }
            }

            for (int cc = 0; cc < BC_; ++cc) {
                const int c = sc + cc;
                std::vector<int> group;
                for (int rr = 0; rr < BR_; ++rr) {
                    const int idx = (sr + rr) * N_ + c;
                    if (grid_[idx] == 0 && (cand_[idx] & bit(d))) group.push_back(idx);
                }
                if (group.size() >= 2U && group.size() <= 4U) {
                    std::vector<std::pair<int, int>> branches;
                    for (int idx : group) branches.push_back({idx, d});
                    if (evalBranches(branches)) return true;
                }
            }
        }
    }

    return false;
}

bool SudokuAnalyzer::applyThreeDMedusa(int& n) {
    n = 0;

    struct Node { int cell = -1; int digit = 0; };
    std::vector<Node> nodes;
    std::vector<int> nodeId(NN_ * (N_ + 1), -1);
    auto key = [&](int cell, int digit) { return cell * (N_ + 1) + digit; };

    for (int c = 0; c < NN_; ++c) {
        if (grid_[c] != 0) continue;
        FOR_EACH_BIT(cand_[c], d) {
            const int id = static_cast<int>(nodes.size());
            nodes.push_back({c, d});
            nodeId[key(c, d)] = id;
        }
    }
    if (nodes.empty()) return false;

    std::vector<std::vector<int>> g(nodes.size());
    auto addEdge = [&](int a, int b) {
        if (a < 0 || b < 0 || a == b) return;
        g[a].push_back(b);
        g[b].push_back(a);
    };

    for (int c = 0; c < NN_; ++c) {
        if (grid_[c] != 0 || bits(cand_[c]) != 2) continue;
        const std::vector<int> ds = digitsFromMask(cand_[c]);
        addEdge(nodeId[key(c, ds[0])], nodeId[key(c, ds[1])]);
    }
    for (int h = 0; h < 3 * N_; ++h) {
        for (int d = 1; d <= N_; ++d) {
            std::vector<int> where;
            for (int idx : houses_[h]) {
                if (grid_[idx] == 0 && (cand_[idx] & bit(d))) where.push_back(idx);
            }
            if (where.size() == 2U) {
                addEdge(nodeId[key(where[0], d)], nodeId[key(where[1], d)]);
            }
        }
    }

    std::vector<int> comp(nodes.size(), -1), color(nodes.size(), -1);
    int compCnt = 0;
    for (int s = 0; s < static_cast<int>(nodes.size()); ++s) {
        if (comp[s] != -1) continue;
        std::vector<int> q = {s};
        comp[s] = compCnt;
        color[s] = 0;
        for (std::size_t qi = 0; qi < q.size(); ++qi) {
            const int u = q[qi];
            for (int v : g[u]) {
                if (comp[v] == -1) {
                    comp[v] = compCnt;
                    color[v] = 1 - color[u];
                    q.push_back(v);
                }
            }
        }
        ++compCnt;
    }

    std::vector<std::array<bool, 2>> bad(compCnt, {false, false});

    for (int c = 0; c < NN_; ++c) {
        std::map<int, std::array<int, 2>> cnt;
        FOR_EACH_BIT(cand_[c], d) {
            const int id = nodeId[key(c, d)];
            if (id < 0) continue;
            cnt[comp[id]][color[id]]++;
        }
        for (const auto& kv : cnt) {
            for (int col = 0; col < 2; ++col) {
                if (kv.second[col] >= 2) bad[kv.first][col] = true;
            }
        }
    }

    for (int h = 0; h < 3 * N_; ++h) {
        for (int d = 1; d <= N_; ++d) {
            std::map<int, std::array<int, 2>> cnt;
            for (int c : houses_[h]) {
                const int id = nodeId[key(c, d)];
                if (id < 0) continue;
                cnt[comp[id]][color[id]]++;
            }
            for (const auto& kv : cnt) {
                for (int col = 0; col < 2; ++col) {
                    if (kv.second[col] >= 2) bad[kv.first][col] = true;
                }
            }
        }
    }

    int local = 0;
    for (int id = 0; id < static_cast<int>(nodes.size()); ++id) {
        if (!bad[comp[id]][color[id]]) continue;
        bool changed = false;
        if (!removeCandidate(nodes[id].cell, nodes[id].digit, changed)) return false;
        if (changed) ++local;
    }
    if (local > 0) { n = local; return true; }

    for (int c = 0; c < NN_; ++c) {
        if (grid_[c] != 0) continue;
        FOR_EACH_BIT(cand_[c], d) {
            std::map<int, std::array<bool, 2>> seen;
            for (int id = 0; id < static_cast<int>(nodes.size()); ++id) {
                if (nodes[id].digit != d) continue;
                if (!isPeerCell(c, nodes[id].cell)) continue;
                seen[comp[id]][color[id]] = true;
            }
            bool remove = false;
            for (const auto& kv : seen) {
                if (kv.second[0] && kv.second[1]) { remove = true; break; }
            }
            if (remove) {
                bool changed = false;
                if (!removeCandidate(c, d, changed)) return false;
                if (changed) { n = 1; return true; }
            }
        }
    }

    return false;
}

void SudokuAnalyzer::logicalSolve() {
    bool progress = true;
    while (!contradiction_ && !solved() && progress) {
        if (generationAttemptDeadlineReached()) {
            return;
        }
        progress = false;
        int n = 0;
        if (applyNakedSingles(n))      { use(Strategy::NakedSingle, n); progress = true; continue; }
        if (applyHiddenSingles(n))     { use(Strategy::HiddenSingle, n); progress = true; continue; }
        if (applyNakedSubset(2, n))    { use(Strategy::NakedPair, n); progress = true; continue; }
        if (applyHiddenSubset(2, n))   { use(Strategy::HiddenPair, n); progress = true; continue; }
        if (applyPointingPairsTriples(n)) { use(Strategy::PointingPairsTriples, n); progress = true; continue; }
        if (applyBoxLineReduction(n))  { use(Strategy::BoxLineReduction, n); progress = true; continue; }
        if (applyNakedSubset(3, n))    { use(Strategy::NakedTriple, n); progress = true; continue; }
        if (applyHiddenSubset(3, n))   { use(Strategy::HiddenTriple, n); progress = true; continue; }
        if (applyNakedSubset(4, n))    { use(Strategy::NakedQuad, n); progress = true; continue; }
        if (applyHiddenSubset(4, n))   { use(Strategy::HiddenQuad, n); progress = true; continue; }
        if (applyFish(2, n))           { use(Strategy::XWing, n); progress = true; continue; }
        if (applyYWing(n))             { use(Strategy::YWing, n); progress = true; continue; }
        if (applyXYZWing(n))           { use(Strategy::XYZWing, n); progress = true; continue; }
        if (applyWXYZWing(n))          { use(Strategy::WXYZWing, n); progress = true; continue; }
        if (applyFish(3, n))           { use(Strategy::Swordfish, n); progress = true; continue; }
        if (applyFinnedFish(3, n))     { use(Strategy::FinnedSwordfish, n); progress = true; continue; }
        if (applyFrankenMutantFish(2, n)) { use(Strategy::FrankenMutantFish, n); progress = true; continue; }
        if (applyKrakenFish(n))        { use(Strategy::KrakenFish, n); progress = true; continue; }
        if (applySkyscraper(n))        { use(Strategy::Skyscraper, n); progress = true; continue; }
        if (applyTwoStringKite(n))     { use(Strategy::TwoStringKite, n); progress = true; continue; }
        if (applySimpleColoring(n))    { use(Strategy::SimpleColoring, n); progress = true; continue; }
        if (applyThreeDMedusa(n))      { use(Strategy::ThreeDMedusa, n); progress = true; continue; }
        if (applyFish(4, n))           { use(Strategy::Jellyfish, n); progress = true; continue; }
        if (applyFinnedXWingSashimi(n)) { use(Strategy::FinnedXWingSashimi, n); progress = true; continue; }
        if (applyFinnedFish(4, n))     { use(Strategy::FinnedJellyfish, n); progress = true; continue; }
        if (applyFrankenMutantFish(3, n)) { use(Strategy::FrankenMutantFish, n); progress = true; continue; }
        if (applyEmptyRectangle(n))    { use(Strategy::EmptyRectangle, n); progress = true; continue; }
        if (applyUniqueRectangleType1(n)) { use(Strategy::UniqueRectangle, n); progress = true; continue; }
        if (applyUniqueRectangleType2to6(n)) { use(Strategy::UniqueRectangle, n); progress = true; continue; }
        if (applyUniqueLoop(n))        { use(Strategy::UniqueLoop, n); progress = true; continue; }
        if (applyBivalueOddagon(n))    { use(Strategy::BivalueOddagon, n); progress = true; continue; }
        if (applyAvoidableRectangle(n)) { use(Strategy::AvoidableRectangle, n); progress = true; continue; }
        if (applyBUGPlus1(n))          { use(Strategy::BUGPlus1, n); progress = true; continue; }
        if (applyRemotePairs(n))       { use(Strategy::RemotePairs, n); progress = true; continue; }
        if (applyWWing(n))             { use(Strategy::WWing, n); progress = true; continue; }
        if (applyGroupedXCycle(n))     { use(Strategy::GroupedXCycle, n); progress = true; continue; }
        if (applyXChain(n))            { use(Strategy::XChain, n); progress = true; continue; }
        if (applyXYChain(n))           { use(Strategy::XYChain, n); progress = true; continue; }
        if (applyGroupedAIC(n))        { use(Strategy::GroupedAIC, n); progress = true; continue; }
        if (applyAIC(n))               { use(Strategy::AIC, n); progress = true; continue; }
        if (applyContinuousNiceLoop(n)) { use(Strategy::ContinuousNiceLoop, n); progress = true; continue; }
        if (applySKLoop(n))            { use(Strategy::SKLoop, n); progress = true; continue; }
        if (applyALSXZ(n))             { use(Strategy::ALSXZ, n); progress = true; continue; }
        if (applyALSXYWing(n))         { use(Strategy::ALSXYWing, n); progress = true; continue; }
        if (applyALSChain(n))          { use(Strategy::ALSChain, n); progress = true; continue; }
        if (applyDeathBlossom(n))      { use(Strategy::DeathBlossom, n); progress = true; continue; }
        if (applySueDeCoq(n))          { use(Strategy::SueDeCoq, n); progress = true; continue; }
        if (applyMSLS(n))              { use(Strategy::MSLS, n); progress = true; continue; }
        if (applyExocet(n))            { use(Strategy::Exocet, n); progress = true; continue; }
        if (applySeniorExocet(n))      { use(Strategy::SeniorExocet, n); progress = true; continue; }
        if (applyPatternOverlayMethod(n)) { use(Strategy::PatternOverlayMethod, n); progress = true; continue; }
        if (applyForcingChains(n))     { use(Strategy::ForcingChains, n); progress = true; continue; }
    }
}

AnalysisReport SudokuAnalyzer::run() {
    AnalysisReport r;
    debug_logic_logs_.clear();
    debug_logic_truncated_ = false;
    r.initial_clues = cluesCount();
    if (!contradiction_) logicalSolve();
    r.contradiction = contradiction_;
    r.solved_logically = (!contradiction_ && solved());

    if (!r.contradiction && !r.solved_logically) {
        const BacktrackingSolveStats bt = solveWithBacktracking(b_, grid_);
        r.solved_with_backtracking = bt.solved;
        r.backtracking_nodes = bt.nodes;
        r.backtracking_decisions = bt.decisions;
        r.backtracking_backtracks = bt.backtracks;
        if (bt.solved) {
            const int btUsage = static_cast<int>(std::max(1LL, bt.decisions));
            use(Strategy::Backtracking, btUsage);
        }
    }
    r.requires_guessing = (!contradiction_ && !r.solved_logically);
    std::copy(std::begin(usage_), std::end(usage_), std::begin(r.strategy_usage));
    r.debug_logic_logs = debug_logic_logs_;

    if (r.contradiction) {
        r.hardest_strategy = "Sprzeczna plansza (bledne dane wejsciowe)";
        r.hardest_rank = 0;
    } else if (r.solved_with_backtracking) {
        r.hardest_strategy = "Backtracking";
        r.hardest_rank = strategyRank(Strategy::Backtracking);
    } else if (r.requires_guessing) {
        r.hardest_strategy = "Wymaga zgadywania/Backtrackingu";
        r.hardest_rank = kDifficultyMaxLevel;
    } else if (hardest_rank_ == 0) {
        r.hardest_strategy = "Brak (same wskazowki)";
        r.hardest_rank = 0;
    } else {
        r.hardest_strategy = hardest_name_;
        r.hardest_rank = hardest_rank_;
    }
    return r;
}

BacktrackingCounter::BacktrackingCounter(int br, int bc, int n, std::vector<int> grid)
    : BR_(br), BC_(bc), N_(n), NN_(n * n), all_((n >= 63) ? 0ULL : ((1ULL << n) - 1ULL)), grid_(std::move(grid)) {}

uint64_t BacktrackingCounter::allowed(int idx) const {
    if (grid_[idx] != 0) return bit(grid_[idx]);
    const int r = row(idx), c = col(idx), b = box(r, c);
    uint64_t m = all_;
    for (int i = 0; i < N_; ++i) {
        const int ri = r * N_ + i, ci = i * N_ + c;
        if (grid_[ri] != 0) m &= ~bit(grid_[ri]);
        if (grid_[ci] != 0) m &= ~bit(grid_[ci]);
    }
    const int bpr = N_ / BC_;
    const int sr = (b / bpr) * BR_, sc = (b % bpr) * BC_;
    for (int dr = 0; dr < BR_; ++dr)
        for (int dc = 0; dc < BC_; ++dc) {
            const int i = (sr + dr) * N_ + (sc + dc);
            if (grid_[i] != 0) m &= ~bit(grid_[i]);
        }
    return m;
}

bool BacktrackingCounter::validState() const {
    for (int r = 0; r < N_; ++r) {
        uint64_t seen = 0ULL;
        for (int c = 0; c < N_; ++c) {
            const int v = grid_[r * N_ + c];
            if (v == 0) continue;
            const uint64_t b = bit(v);
            if (seen & b) return false;
            seen |= b;
        }
    }
    for (int c = 0; c < N_; ++c) {
        uint64_t seen = 0ULL;
        for (int r = 0; r < N_; ++r) {
            const int v = grid_[r * N_ + c];
            if (v == 0) continue;
            const uint64_t b = bit(v);
            if (seen & b) return false;
            seen |= b;
        }
    }
    const int bpr = N_ / BC_;
    for (int b = 0; b < N_; ++b) {
        uint64_t seen = 0ULL;
        const int sr = (b / bpr) * BR_, sc = (b % bpr) * BC_;
        for (int dr = 0; dr < BR_; ++dr) {
            for (int dc = 0; dc < BC_; ++dc) {
                const int v = grid_[(sr + dr) * N_ + (sc + dc)];
                if (v == 0) continue;
                const uint64_t bt = bit(v);
                if (seen & bt) return false;
                seen |= bt;
            }
        }
    }
    return true;
}

void BacktrackingCounter::search() {
    if (aborted_ || solutions_ >= limit_) return;
    if (generationAttemptDeadlineReached()) {
        aborted_ = true;
        return;
    }
    ++nodes_;
    if (generationAttemptNodeBudgetReached(nodes_)) {
        aborted_ = true;
        return;
    }
    int best = -1, bestc = INT_MAX;
    uint64_t bm = 0ULL;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] != 0) continue;
        const uint64_t m = allowed(i);
        const int c = bits(m);
        if (c == 0) return;
        if (c < bestc) {
            best = i; bestc = c; bm = m;
            if (c == 1) break;
        }
    }
    if (best == -1) { ++solutions_; return; }
    while (bm) {
        const uint64_t one = bm & (~bm + 1ULL);
        grid_[best] = firstDigit(one);
        search();
        grid_[best] = 0;
        if (solutions_ >= limit_) return;
        bm &= (bm - 1ULL);
    }
}

int BacktrackingCounter::countSolutions(int limit) {
    limit_ = limit;
    solutions_ = 0;
    nodes_ = 0;
    aborted_ = false;
    if (!validState()) return 0;
    search();
    if (aborted_) {
        return std::max(2, limit_);
    }
    return solutions_;
}

BacktrackingSolver::BacktrackingSolver(int br, int bc, int n, std::vector<int> grid)
    : BR_(br), BC_(bc), N_(n), NN_(n * n), all_((n >= 63) ? 0ULL : ((1ULL << n) - 1ULL)), grid_(std::move(grid)) {}

uint64_t BacktrackingSolver::allowed(int idx) const {
    if (grid_[idx] != 0) return bit(grid_[idx]);
    const int r = row(idx), c = col(idx), b = box(r, c);
    uint64_t m = all_;
    for (int i = 0; i < N_; ++i) {
        const int ri = r * N_ + i, ci = i * N_ + c;
        if (grid_[ri] != 0) m &= ~bit(grid_[ri]);
        if (grid_[ci] != 0) m &= ~bit(grid_[ci]);
    }
    const int bpr = N_ / BC_;
    const int sr = (b / bpr) * BR_, sc = (b % bpr) * BC_;
    for (int dr = 0; dr < BR_; ++dr) {
        for (int dc = 0; dc < BC_; ++dc) {
            const int i = (sr + dr) * N_ + (sc + dc);
            if (grid_[i] != 0) m &= ~bit(grid_[i]);
        }
    }
    return m;
}

bool BacktrackingSolver::validState() const {
    for (int r = 0; r < N_; ++r) {
        uint64_t seen = 0ULL;
        for (int c = 0; c < N_; ++c) {
            const int v = grid_[r * N_ + c];
            if (v == 0) continue;
            const uint64_t b = bit(v);
            if (seen & b) return false;
            seen |= b;
        }
    }
    for (int c = 0; c < N_; ++c) {
        uint64_t seen = 0ULL;
        for (int r = 0; r < N_; ++r) {
            const int v = grid_[r * N_ + c];
            if (v == 0) continue;
            const uint64_t b = bit(v);
            if (seen & b) return false;
            seen |= b;
        }
    }
    const int bpr = N_ / BC_;
    for (int b = 0; b < N_; ++b) {
        uint64_t seen = 0ULL;
        const int sr = (b / bpr) * BR_, sc = (b % bpr) * BC_;
        for (int dr = 0; dr < BR_; ++dr) {
            for (int dc = 0; dc < BC_; ++dc) {
                const int v = grid_[(sr + dr) * N_ + (sc + dc)];
                if (v == 0) continue;
                const uint64_t bt = bit(v);
                if (seen & bt) return false;
                seen |= bt;
            }
        }
    }
    return true;
}

bool BacktrackingSolver::search() {
    if (generationAttemptDeadlineReached()) {
        return false;
    }
    ++stats_.nodes;
    if (generationAttemptNodeBudgetReached(stats_.nodes)) {
        return false;
    }

    int best = -1;
    int bestc = INT_MAX;
    uint64_t bm = 0ULL;
    for (int i = 0; i < NN_; ++i) {
        if (grid_[i] != 0) continue;
        const uint64_t m = allowed(i);
        const int c = bits(m);
        if (c == 0) return false;
        if (c < bestc) {
            best = i;
            bestc = c;
            bm = m;
            if (c == 1) break;
        }
    }

    if (best == -1) {
        return true;
    }

    while (bm) {
        const uint64_t one = bm & (~bm + 1ULL);
        grid_[best] = firstDigit(one);
        ++stats_.decisions;
        if (search()) {
            return true;
        }
        grid_[best] = 0;
        ++stats_.backtracks;
        bm &= (bm - 1ULL);
    }

    return false;
}

BacktrackingSolveStats BacktrackingSolver::solve() {
    stats_ = BacktrackingSolveStats{};
    if (!validState()) {
        return stats_;
    }
    stats_.solved = search();
    return stats_;
}

static BacktrackingSolveStats solveWithBacktracking(const SudokuBoard& b, const std::vector<int>& initialGrid);
// Definition moved after DancingLinksSolver class

// =============================================================================
// Dancing Links (Algorithm X) Solver - Knuth's DLX for exact cover
// =============================================================================
class DancingLinksSolver {
    struct Node {
        int L, R, U, D;
        int col;
        int rowId; // cell * N + (digit - 1)
    };

    int BR_, BC_, N_, NN_;
    int numActiveCols_;
    int nodeCount_;
    int solutions_;
    int limit_;
    long long nodes_;
    long long decisions_;
    long long backtracks_;
    bool aborted_;
    bool recordSolution_;

    std::vector<Node> nd_;
    std::vector<int> sz_;
    std::vector<int> solRows_;
    std::vector<int> resultGrid_;

    int boxIdx(int r, int c) const {
        return (r / BR_) * (N_ / BC_) + (c / BC_);
    }

    int newNode(int colHeader, int rowId) {
        const int x = nodeCount_++;
        nd_[x].col = colHeader;
        nd_[x].rowId = rowId;
        nd_[x].U = nd_[colHeader].U;
        nd_[x].D = colHeader;
        nd_[nd_[colHeader].U].D = x;
        nd_[colHeader].U = x;
        nd_[x].L = nd_[x].R = x;
        ++sz_[colHeader];
        return x;
    }

    void linkRow(int a, int b, int c, int d) {
        nd_[a].R = b; nd_[b].L = a;
        nd_[b].R = c; nd_[c].L = b;
        nd_[c].R = d; nd_[d].L = c;
        nd_[d].R = a; nd_[a].L = d;
    }

    void cover(int c) {
        nd_[nd_[c].R].L = nd_[c].L;
        nd_[nd_[c].L].R = nd_[c].R;
        for (int i = nd_[c].D; i != c; i = nd_[i].D) {
            for (int j = nd_[i].R; j != i; j = nd_[j].R) {
                nd_[nd_[j].D].U = nd_[j].U;
                nd_[nd_[j].U].D = nd_[j].D;
                --sz_[nd_[j].col];
            }
        }
    }

    void uncover(int c) {
        for (int i = nd_[c].U; i != c; i = nd_[i].U) {
            for (int j = nd_[i].L; j != i; j = nd_[j].L) {
                ++sz_[nd_[j].col];
                nd_[nd_[j].D].U = j;
                nd_[nd_[j].U].D = j;
            }
        }
        nd_[nd_[c].R].L = c;
        nd_[nd_[c].L].R = c;
    }

    void search(int depth) {
        if (aborted_ || solutions_ >= limit_) return;

        ++nodes_;
        if (generationAttemptDeadlineReached() || generationAttemptNodeBudgetReached(nodes_)) {
            aborted_ = true;
            return;
        }

        if (nd_[0].R == 0) {
            ++solutions_;
            if (recordSolution_ && solutions_ == 1) {
                for (int i = 0; i < depth; ++i) {
                    const int rowId = solRows_[i];
                    const int cell = rowId / N_;
                    const int digit = (rowId % N_) + 1;
                    resultGrid_[cell] = digit;
                }
            }
            return;
        }

        int minCol = nd_[0].R;
        int minSz = sz_[minCol];
        for (int j = nd_[minCol].R; j != 0; j = nd_[j].R) {
            if (sz_[j] < minSz) {
                minSz = sz_[j];
                minCol = j;
            }
        }

        if (minSz == 0) return;

        cover(minCol);
        for (int i = nd_[minCol].D; i != minCol; i = nd_[i].D) {
            solRows_[depth] = nd_[i].rowId;
            ++decisions_;
            for (int j = nd_[i].R; j != i; j = nd_[j].R) {
                cover(nd_[j].col);
            }
            search(depth + 1);
            for (int j = nd_[i].L; j != i; j = nd_[j].L) {
                uncover(nd_[j].col);
            }
            ++backtracks_;
            if (aborted_ || solutions_ >= limit_) break;
        }
        uncover(minCol);
    }

public:
    DancingLinksSolver(int br, int bc, int n, const std::vector<int>& grid)
        : BR_(br), BC_(bc), N_(n), NN_(n * n),
          numActiveCols_(0), nodeCount_(0),
          solutions_(0), limit_(2), nodes_(0), decisions_(0), backtracks_(0),
          aborted_(false), recordSolution_(false)
    {
        const int totalConstraints = 4 * NN_;

        // Determine which constraints are already satisfied by pre-filled cells
        std::vector<bool> satisfied(totalConstraints, false);
        std::vector<uint64_t> rowUsed(N_, 0ULL), colUsed(N_, 0ULL), boxUsed(N_, 0ULL);

        for (int idx = 0; idx < NN_; ++idx) {
            if (grid[idx] != 0) {
                const int d = grid[idx];
                const int r = idx / N_, c = idx % N_;
                const int b = boxIdx(r, c);
                rowUsed[r] |= 1ULL << (d - 1);
                colUsed[c] |= 1ULL << (d - 1);
                boxUsed[b] |= 1ULL << (d - 1);
                satisfied[idx] = true;
                satisfied[NN_ + r * N_ + (d - 1)] = true;
                satisfied[2 * NN_ + c * N_ + (d - 1)] = true;
                satisfied[3 * NN_ + b * N_ + (d - 1)] = true;
            }
        }

        // Map constraint index → column header index (1-indexed, 0 = root)
        std::vector<int> constraintToCol(totalConstraints, -1);
        numActiveCols_ = 0;
        for (int i = 0; i < totalConstraints; ++i) {
            if (!satisfied[i]) {
                constraintToCol[i] = ++numActiveCols_;
            }
        }

        // Allocate: root + column headers + max data nodes (4 per possible row)
        int emptyCells = 0;
        for (int idx = 0; idx < NN_; ++idx) if (grid[idx] == 0) ++emptyCells;
        const int maxDataNodes = 4 * emptyCells * N_;
        const int totalNodes = 1 + numActiveCols_ + maxDataNodes + 10;

        nd_.resize(totalNodes);
        sz_.assign(numActiveCols_ + 1, 0);
        solRows_.resize(NN_);

        // Root node (index 0)
        nd_[0] = {0, 0, 0, 0, 0, -1};
        nodeCount_ = 1;

        // Create column headers
        if (numActiveCols_ > 0) {
            for (int c = 1; c <= numActiveCols_; ++c) {
                nd_[c] = {c - 1, (c < numActiveCols_) ? c + 1 : 0, c, c, c, -1};
            }
            nd_[0].R = 1;
            nd_[0].L = numActiveCols_;
            nd_[1].L = 0;
            nd_[numActiveCols_].R = 0;
            nodeCount_ = numActiveCols_ + 1;
        }

        // Add rows for empty cells with valid digits
        for (int idx = 0; idx < NN_; ++idx) {
            if (grid[idx] != 0) continue;
            const int r = idx / N_, cc = idx % N_;
            const int b = boxIdx(r, cc);
            const uint64_t used = rowUsed[r] | colUsed[cc] | boxUsed[b];

            for (int d = 1; d <= N_; ++d) {
                if (used & (1ULL << (d - 1))) continue;

                const int c1 = constraintToCol[idx];
                const int c2 = constraintToCol[NN_ + r * N_ + (d - 1)];
                const int c3 = constraintToCol[2 * NN_ + cc * N_ + (d - 1)];
                const int c4 = constraintToCol[3 * NN_ + b * N_ + (d - 1)];

                if (c1 < 0 || c2 < 0 || c3 < 0 || c4 < 0) continue;

                const int rowId = idx * N_ + (d - 1);
                const int n1 = newNode(c1, rowId);
                const int n2 = newNode(c2, rowId);
                const int n3 = newNode(c3, rowId);
                const int n4 = newNode(c4, rowId);
                linkRow(n1, n2, n3, n4);
            }
        }

        resultGrid_ = grid;
    }

    int countSolutions(int limit) {
        limit_ = limit;
        solutions_ = 0;
        nodes_ = 0;
        decisions_ = 0;
        backtracks_ = 0;
        aborted_ = false;
        recordSolution_ = false;
        search(0);
        if (aborted_) return std::max(2, limit_);
        return solutions_;
    }

    bool solve() {
        limit_ = 1;
        solutions_ = 0;
        nodes_ = 0;
        decisions_ = 0;
        backtracks_ = 0;
        aborted_ = false;
        recordSolution_ = true;
        search(0);
        return solutions_ >= 1;
    }

    const std::vector<int>& solutionGrid() const { return resultGrid_; }
    long long nodesCount() const { return nodes_; }
    long long decisionsCount() const { return decisions_; }
    long long backtracksCount() const { return backtracks_; }
};

// solveWithBacktracking definition (uses DLX)
static BacktrackingSolveStats solveWithBacktracking(const SudokuBoard& b, const std::vector<int>& initialGrid) {
    DancingLinksSolver dlx(b.block_rows, b.block_cols, b.side_size, initialGrid);
    BacktrackingSolveStats stats;
    stats.solved = dlx.solve();
    stats.nodes = dlx.nodesCount();
    stats.decisions = dlx.decisionsCount();
    stats.backtracks = dlx.backtracksCount();
    return stats;
}

static SudokuBoard parseSudokuLine(const std::string& line) {
    SudokuBoard b;
    std::stringstream ss(line);
    std::string seg;
    std::vector<std::string> t;
    while (std::getline(ss, seg, ',')) {
        const std::string x = trim(seg);
        t.push_back(x);
    }
    if (t.size() < 4U) { b.error = "Za malo tokenow"; return b; }
    if (!parseLLStrict(t[0], b.seed)) { b.error = "Niepoprawny seed"; return b; }
    if (!parseIntStrict(t[1], b.block_rows) || !parseIntStrict(t[2], b.block_cols)) {
        b.error = "Niepoprawne Rows/Cols"; return b;
    }
    if (b.block_rows <= 0 || b.block_cols <= 0) { b.error = "Rows/Cols musza byc > 0"; return b; }
    b.side_size = b.block_rows * b.block_cols;
    b.total_cells = b.side_size * b.side_size;
    if (b.side_size <= 0 || b.side_size > 36) { b.error = "Nieobslugiwany rozmiar"; return b; }
    if (static_cast<int>(t.size()) < 3 + b.total_cells) { b.error = "Za malo danych"; return b; }

    b.cells.reserve(b.total_cells);
    for (int i = 0; i < b.total_cells; ++i) {
        const std::string tok = t[3 + i];
        Cell c;
        if (tok.empty() || tok == "0" || tok == "x" || tok == "X") {
            c.value = 0; c.revealed = false;
        } else if (tok[0] == 't' || tok[0] == 'T') {
            // Nowy, wspolny format: tN/TN = komorka stala (dana dla gracza).
            int v = 0;
            if (!parseIntStrict(tok.substr(1), v) || v < 1 || v > b.side_size) {
                b.error = "Niepoprawna dana"; b.cells.clear(); return b;
            }
            c.value = v; c.revealed = true;
        } else {
            // Nowy, wspolny format: N = komorka do odgadniecia (zapisana z poprawna wartoscia).
            int v = 0;
            if (!parseIntStrict(tok, v) || v < 1 || v > b.side_size) {
                b.error = "Niepoprawna wartosc"; b.cells.clear(); return b;
            }
            c.value = v; c.revealed = false;
        }
        b.cells.push_back(c);
    }
    b.valid = true;
    return b;
}

static int countSolutionsWithBacktracking(const SudokuBoard& b, int limit) {
    std::vector<int> g(b.total_cells, 0);
    for (int i = 0; i < b.total_cells; ++i) if (b.cells[i].revealed) g[i] = b.cells[i].value;
    DancingLinksSolver dlx(b.block_rows, b.block_cols, b.side_size, g);
    return dlx.countSolutions(limit);
}

static std::string selectFolderModern() {
    std::string path;
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&dialog));
    if (FAILED(hr) || dialog == nullptr) return path;

    DWORD opts = 0;
    if (SUCCEEDED(dialog->GetOptions(&opts))) dialog->SetOptions(opts | FOS_PICKFOLDERS);
    dialog->SetTitle(L"Wybierz folder z plikami Sudoku (.txt)");

    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR selected = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected)) && selected != nullptr) {
                const std::wstring ws(selected);
                path.assign(ws.begin(), ws.end());
                CoTaskMemFree(selected);
            }
            item->Release();
        }
    }
    dialog->Release();
    return path;
}

static bool isTxtFile(const fs::path& p) {
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return e == ".txt";
}

static bool isPathWithin(const fs::path& path, const fs::path& parent) {
    std::string fullPath = fs::absolute(path).lexically_normal().generic_string();
    std::string fullParent = fs::absolute(parent).lexically_normal().generic_string();

    std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(),
                   [](unsigned char c) { return static_cast<char>(tolower(c)); });
    std::transform(fullParent.begin(), fullParent.end(), fullParent.begin(),
                   [](unsigned char c) { return static_cast<char>(tolower(c)); });

    if (!fullParent.empty() && fullParent.back() == '/') {
        fullParent.pop_back();
    }
    if (fullPath == fullParent) {
        return true;
    }
    fullParent.push_back('/');
    return fullPath.rfind(fullParent, 0) == 0;
}

static std::vector<fs::path> collectTxtFilesRecursive(const fs::path& root, const fs::path& excludedRoot) {
    std::vector<fs::path> files;
    std::error_code ec;

    fs::recursive_directory_iterator end;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::path currentPath = it->path();
        if (it->is_directory(ec)) {
            if (!ec && isPathWithin(currentPath, excludedRoot)) {
                it.disable_recursion_pending();
            }
            ec.clear();
            it.increment(ec);
            continue;
        }

        if (!it->is_regular_file(ec)) {
            ec.clear();
            it.increment(ec);
            continue;
        }
        ec.clear();

        if (isPathWithin(currentPath, excludedRoot)) {
            it.increment(ec);
            continue;
        }
        if (isTxtFile(currentPath)) {
            files.push_back(currentPath);
        }
        it.increment(ec);
    }

    std::sort(files.begin(), files.end());
    return files;
}

static long long countNonEmptyLinesInTxtFiles(const std::vector<fs::path>& files) {
    long long total = 0;
    for (const fs::path& p : files) {
        std::ifstream in(p);
        if (!in.is_open()) {
            continue;
        }

        std::string line;
        while (std::getline(in, line)) {
            if (!trim(line).empty()) {
                ++total;
            }
        }
    }
    return total;
}

static std::string folderKeyFromRelativePath(const fs::path& rel) {
    const std::string key = rel.generic_string();
    if (key.empty() || key == ".") {
        return "ROOT";
    }
    return key;
}

static std::string sanitizeFileName(const std::string& name) {
    std::string out = name;
    for (char& ch : out) {
        const bool bad = (ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' ||
                          ch == '\\' || ch == '|' || ch == '?' || ch == '*');
        if (bad) ch = '_';
    }
    if (out.empty()) out = "ROOT";
    return out;
}

static std::string csvEscape(const std::string& field) {
    std::string out = "\"";
    for (char ch : field) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('"');
    return out;
}

static int difficultyLevelFromReport(const AnalysisReport& report) {
    if (report.contradiction) {
        return 0;
    }
    if (report.requires_guessing || report.hardest_rank >= 100) {
        return kDifficultyMaxLevel;
    }
    if (report.hardest_rank <= 0) {
        return kDifficultyMinLevel;
    }
    return std::clamp(report.hardest_rank, kDifficultyMinLevel, kDifficultyMaxLevel);
}

static std::string difficultyTypeFromReport(const AnalysisReport& report) {
    if (report.contradiction) {
        return "Sprzeczne";
    }
    if (report.requires_guessing || report.hardest_rank >= 100) {
        return "Poziom 9 - Backtracking/Brutalny";
    }
    const int level = difficultyLevelFromReport(report);
    switch (level) {
        case 1: return "Poziom 1 - Podstawowy";
        case 2: return "Poziom 2 - Sredniozaawansowany-1";
        case 3: return "Poziom 3 - Sredniozaawansowany-2";
        case 4: return "Poziom 4 - Zaawansowany";
        case 5: return "Poziom 5 - Ekspert";
        case 6: return "Poziom 6 - Mistrzowski";
        case 7: return "Poziom 7 - Diabelski";
        case 8: return "Poziom 8 - Teoretyczny";
        case 9: return "Poziom 9 - Backtracking/Brutalny";
        default: return "Nieznany";
    }
}

static int interpolateClueAnchor(int side, int at6, int at9, int at16) {
    auto roundLerp = [](int a, int b, int num, int den) -> int {
        if (den <= 0) return a;
        const int delta = b - a;
        const int add = (delta >= 0) ? (den / 2) : -(den / 2);
        return a + (delta * num + add) / den;
    };

    if (side <= 6) return at6;
    if (side < 9) return roundLerp(at6, at9, side - 6, 3);
    if (side == 9) return at9;
    if (side < 16) return roundLerp(at9, at16, side - 9, 7);
    return at16;
}

static ClueRange recommendedClueRangeForLevel(int sideSize, int level) {
    static constexpr int kMin[5][3] = {
        {14, 36, 135}, // L1
        {10, 30, 110}, // L2-L3
        {8, 25, 90},   // L4-L5
        {7, 22, 75},   // L6-L8
        {5, 17, 60}    // L9 (Backtracking)
    };
    static constexpr int kMax[5][3] = {
        {18, 45, 160}, // L1 (9x9 mozna podnosic recznie ponad 45)
        {14, 35, 134}, // L2-L3
        {10, 29, 109}, // L4-L5
        {9, 25, 89},   // L6-L8
        {7, 22, 74}    // L9 (Backtracking)
    };

    const int lv = std::clamp(level, kDifficultyMinLevel, kDifficultyMaxLevel);
    int group = 0;
    if (lv == 1) {
        group = 0;
    } else if (lv <= 3) {
        group = 1;
    } else if (lv <= 5) {
        group = 2;
    } else if (lv <= 8) {
        group = 3;
    } else {
        group = 4;
    }

    const int side = std::clamp(sideSize, 6, 16);
    ClueRange out;
    out.min_clues = interpolateClueAnchor(side, kMin[group][0], kMin[group][1], kMin[group][2]);
    out.max_clues = interpolateClueAnchor(side, kMax[group][0], kMax[group][1], kMax[group][2]);
    if (out.min_clues > out.max_clues) {
        std::swap(out.min_clues, out.max_clues);
    }
    const int nn = side * side;
    out.min_clues = std::clamp(out.min_clues, 0, nn);
    out.max_clues = std::clamp(out.max_clues, 0, nn);
    return out;
}

static ClueRange recommendedClueRangeForDifficultyRange(int sideSize, int difficultyMin, int difficultyMax) {
    const int lo = std::clamp(std::min(difficultyMin, difficultyMax), kDifficultyMinLevel, kDifficultyMaxLevel);
    const int hi = std::clamp(std::max(difficultyMin, difficultyMax), kDifficultyMinLevel, kDifficultyMaxLevel);
    ClueRange out;
    out.min_clues = INT_MAX;
    out.max_clues = 0;
    for (int level = lo; level <= hi; ++level) {
        const ClueRange one = recommendedClueRangeForLevel(sideSize, level);
        out.min_clues = std::min(out.min_clues, one.min_clues);
        out.max_clues = std::max(out.max_clues, one.max_clues);
    }
    if (out.min_clues == INT_MAX) {
        out.min_clues = 0;
        out.max_clues = 0;
    }
    return out;
}

static bool reportUsesStrategyAtLevel(const AnalysisReport& report, int level) {
    const int target = std::clamp(level, kDifficultyMinLevel, kDifficultyMaxLevel);
    for (int si = 0; si < kNumStrategies; ++si) {
        if (report.strategy_usage[si] <= 0) continue;
        if (strategyRank(static_cast<Strategy>(si)) == target) {
            return true;
        }
    }
    return false;
}

static std::string boardTypeFromBoard(const SudokuBoard& board) {
    if (board.side_size <= 0 || board.block_rows <= 0 || board.block_cols <= 0) {
        return "Nieznany";
    }
    std::ostringstream ss;
    ss << board.side_size << "x" << board.side_size
       << " (" << board.block_rows << "x" << board.block_cols << ")";
    return ss.str();
}

static PuzzleResult analyzePuzzleTask(const PuzzleTask& task) {
    PuzzleResult result;
    result.processed = true;
    result.folder_key = task.folder_key;
    result.relative_folder = task.relative_folder;
    result.source_file = task.source_file;
    result.source_path = task.source_path;
    result.line_no = task.line_no;

    try {
        SudokuBoard board = parseSudokuLine(task.clean_line);
        if (!board.valid) {
            result.valid = false;
            result.error = board.error;
            return result;
        }

        SudokuAnalyzer analyzer(board);
        AnalysisReport report = analyzer.run();
        report.solution_count = countSolutionsWithBacktracking(board, 2);
        report.unique_solution = (report.solution_count == 1);

        result.valid = true;
        result.board = std::move(board);
        result.report = std::move(report);
        return result;
    } catch (const std::exception& ex) {
        result.valid = false;
        result.error = std::string("Wyjatek analizy: ") + ex.what();
    } catch (...) {
        result.valid = false;
        result.error = "Nieznany wyjatek analizy";
    }
    return result;
}

static void appendInvalidPuzzleReport(FolderStats& stats, const std::string& sourceFile, int lineNo,
                                      const std::string& parseError) {
    PuzzleReportEntry entry;
    entry.source_file = sourceFile;
    entry.line_no = lineNo;
    entry.valid = false;
    entry.sudoku_type = "Niepoprawne dane";
    entry.parse_error = parseError;
    entry.hardest_strategy = "Brak - blad danych";
    std::fill(std::begin(entry.strategy_usage), std::end(entry.strategy_usage), 0);
    entry.debug_logic_logs.clear();
    stats.puzzle_reports.push_back(std::move(entry));
}

static void appendValidPuzzleReport(FolderStats& stats, const std::string& sourceFile, int lineNo,
                                    const SudokuBoard& board, const AnalysisReport& report) {
    PuzzleReportEntry entry;
    entry.source_file = sourceFile;
    entry.line_no = lineNo;
    entry.valid = true;
    entry.sudoku_type = difficultyTypeFromReport(report);
    entry.board_type = boardTypeFromBoard(board);
    entry.initial_clues = report.initial_clues;
    entry.difficulty_level = difficultyLevelFromReport(report);
    entry.solved_logically = report.solved_logically;
    entry.requires_guessing = report.requires_guessing;
    entry.solved_with_backtracking = report.solved_with_backtracking;
    entry.contradiction = report.contradiction;
    entry.solution_count = report.solution_count;
    entry.backtracking_nodes = report.backtracking_nodes;
    entry.backtracking_decisions = report.backtracking_decisions;
    entry.backtracking_backtracks = report.backtracking_backtracks;
    std::copy(std::begin(report.strategy_usage), std::end(report.strategy_usage), std::begin(entry.strategy_usage));
    entry.hardest_strategy = report.hardest_strategy;
    entry.debug_logic_logs = report.debug_logic_logs;
    stats.puzzle_reports.push_back(std::move(entry));
}

static void updateFolderStats(FolderStats& stats, const AnalysisReport& report) {
    ++stats.analyzed_puzzles;
    stats.clues_sum += report.initial_clues;

    if (report.contradiction) ++stats.contradictions;
    if (report.solved_logically) ++stats.solved_logically;
    if (report.requires_guessing) ++stats.requires_guessing;
    if (report.solved_with_backtracking) ++stats.solved_with_backtracking;

    if (report.solution_count == 1) ++stats.unique_solutions;
    else if (report.solution_count == 0) ++stats.no_solution;
    else ++stats.multiple_solutions;

    stats.backtracking_nodes_sum += report.backtracking_nodes;
    stats.backtracking_decisions_sum += report.backtracking_decisions;
    stats.backtracking_backtracks_sum += report.backtracking_backtracks;

    for (int si = 0; si < kNumStrategies; ++si) {
        stats.strategy_usage[si] += report.strategy_usage[si];
    }
    stats.hardest_histogram[report.hardest_strategy] += 1;

    const int difficulty = difficultyLevelFromReport(report);
    if (difficulty > 0) {
        stats.difficulty_sum += difficulty;
        ++stats.difficulty_count;
        if (difficulty > stats.max_difficulty) {
            stats.max_difficulty = difficulty;
        }
    }

    if (report.hardest_rank > stats.hardest_rank_seen) {
        stats.hardest_rank_seen = report.hardest_rank;
        stats.hardest_name_seen = report.hardest_strategy;
    }
}

static void writeFolderReport(const fs::path& outDir, const std::string& folderKey, const FolderStats& stats) {
    fs::path relFolder = stats.relative_folder;
    if (relFolder.empty() || relFolder == fs::path(".")) {
        relFolder = fs::path("ROOT");
    }
    const fs::path folderOutDir = outDir / relFolder;
    std::error_code ec;
    fs::create_directories(folderOutDir, ec);
    const fs::path outPath = folderOutDir / "statystyki_folder.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) {
        std::cerr << "Nie mozna zapisac raportu folderu: " << outPath.string() << "\n";
        return;
    }

    out << "=== Statystyki Folderu Sudoku ===\n";
    out << "Folder: " << folderKey << "\n";
    out << "Niepuste wpisy (linie): " << stats.non_empty_lines << "\n";
    out << "Poprawnie przeanalizowane plansze: " << stats.analyzed_puzzles << "\n";
    out << "Bledne wpisy: " << stats.invalid_lines << "\n\n";

    out << "Wynik solvera:\n";
    out << "- Rozwiazane logicznie: " << stats.solved_logically << "\n";
    out << "- Wymaga zgadywania/backtrackingu: " << stats.requires_guessing << "\n";
    out << "- Rozwiazane z uzyciem backtrackingu: " << stats.solved_with_backtracking << "\n";
    out << "- Sprzeczne plansze: " << stats.contradictions << "\n\n";

    out << "Metryki backtrackingu (suma):\n";
    out << "- Decyzje: " << stats.backtracking_decisions_sum << "\n";
    out << "- Backtracki: " << stats.backtracking_backtracks_sum << "\n";
    out << "- Nody rekursji: " << stats.backtracking_nodes_sum << "\n\n";

    out << "Unikalnosc rozwiazania:\n";
    out << "- Unikalne: " << stats.unique_solutions << "\n";
    out << "- Wiele rozwiazan: " << stats.multiple_solutions << "\n";
    out << "- Brak rozwiazania: " << stats.no_solution << "\n\n";

    out << "Srednia liczba clues: ";
    if (stats.analyzed_puzzles > 0) {
        const double avg = static_cast<double>(stats.clues_sum) / static_cast<double>(stats.analyzed_puzzles);
        out << std::fixed << std::setprecision(2) << avg << "\n";
    } else {
        out << "0.00\n";
    }

    out << "Poziom trudnosci sudoku (1-8): ";
    if (stats.difficulty_count > 0) {
        const double avgDifficulty = static_cast<double>(stats.difficulty_sum) / static_cast<double>(stats.difficulty_count);
        out << std::fixed << std::setprecision(2) << avgDifficulty
            << " (max: " << stats.max_difficulty << ")\n";
    } else {
        out << "brak danych\n";
    }

    out << "\nUzycie strategii (suma):\n";
    const std::vector<Strategy> order = {
        Strategy::NakedSingle, Strategy::HiddenSingle,
        Strategy::NakedPair, Strategy::HiddenPair,
        Strategy::PointingPairsTriples, Strategy::BoxLineReduction,
        Strategy::NakedTriple, Strategy::HiddenTriple,
        Strategy::NakedQuad, Strategy::HiddenQuad,
        Strategy::XWing, Strategy::YWing, Strategy::XYZWing, Strategy::WXYZWing, Strategy::Swordfish, Strategy::Jellyfish, Strategy::FrankenMutantFish, Strategy::KrakenFish,
        Strategy::Skyscraper, Strategy::TwoStringKite, Strategy::SimpleColoring, Strategy::ThreeDMedusa,
        Strategy::FinnedXWingSashimi, Strategy::FinnedSwordfish, Strategy::FinnedJellyfish, Strategy::EmptyRectangle,
        Strategy::UniqueRectangle, Strategy::UniqueLoop, Strategy::BivalueOddagon, Strategy::AvoidableRectangle, Strategy::BUGPlus1,
        Strategy::RemotePairs, Strategy::WWing, Strategy::GroupedXCycle, Strategy::XChain, Strategy::XYChain,
        Strategy::GroupedAIC, Strategy::AIC, Strategy::ContinuousNiceLoop,
        Strategy::ALSXZ, Strategy::ALSXYWing, Strategy::ALSChain, Strategy::DeathBlossom,
        Strategy::SueDeCoq, Strategy::MSLS, Strategy::Exocet, Strategy::SeniorExocet, Strategy::SKLoop, Strategy::PatternOverlayMethod,
        Strategy::ForcingChains, Strategy::Backtracking
    };
    bool any = false;
    for (Strategy s : order) {
        const long long cnt = stats.strategy_usage[static_cast<int>(s)];
        if (cnt <= 0) continue;
        any = true;
        out << "- " << strategyName(s) << ": " << cnt << "\n";
    }
    if (!any) {
        out << "- Brak\n";
    }

    out << "\nStatus implementacji technik:\n";
    for (Strategy s : order) {
        out << "- " << strategyName(s) << ": " << strategyImplementationStatus(s) << "\n";
    }

    out << "\nNajtrudniejsza technika zaobserwowana w folderze: " << stats.hardest_name_seen << "\n";
    out << "Rozklad najtrudniejszej techniki (na plansze):\n";
    for (const auto& it : stats.hardest_histogram) {
        out << "- " << it.first << ": " << it.second << "\n";
    }

    out << "\n=== Raporty Sudoku w folderze (zbiorczo) ===\n";
    if (stats.puzzle_reports.empty()) {
        out << "Brak wpisow Sudoku.\n";
    }

    for (std::size_t i = 0; i < stats.puzzle_reports.size(); ++i) {
        const PuzzleReportEntry& entry = stats.puzzle_reports[i];
        out << "\n[" << (i + 1) << "] Plik: " << entry.source_file
            << " | Linia: " << entry.line_no << "\n";

        if (!entry.valid) {
            out << "Typ: " << entry.sudoku_type << "\n";
            out << "Blad: " << entry.parse_error << "\n";
            continue;
        }

        out << "Typ: " << entry.sudoku_type << "\n";
        out << "Rozmiar: " << entry.board_type << "\n";
        out << "Najtrudniejsza technika: " << entry.hardest_strategy << "\n";
        out << "Poziom trudnosci (1-8): " << entry.difficulty_level << "\n";
        out << "Liczba clues: " << entry.initial_clues << "\n";
        out << "Rozwiazane logicznie: " << (entry.solved_logically ? "TAK" : "NIE") << "\n";
        out << "Wymaga zgadywania: " << (entry.requires_guessing ? "TAK" : "NIE") << "\n";
        out << "Rozwiazane backtrackingiem: " << (entry.solved_with_backtracking ? "TAK" : "NIE") << "\n";
        out << "Sprzeczne: " << (entry.contradiction ? "TAK" : "NIE") << "\n";
        out << "Liczba rozwiazan: " << entry.solution_count << "\n";
        out << "Backtracking - decyzje: " << entry.backtracking_decisions << "\n";
        out << "Backtracking - backtracki: " << entry.backtracking_backtracks << "\n";
        out << "Backtracking - nody rekursji: " << entry.backtracking_nodes << "\n";
        out << "Uzycie metod:\n";
        for (Strategy s : order) {
            const long long used = static_cast<long long>(entry.strategy_usage[static_cast<int>(s)]);
            out << "  - " << strategyName(s) << ": " << used << "\n";
        }
        out << "Debug log (strategie zaawansowane):\n";
        if (entry.debug_logic_logs.empty()) {
            out << "  - Brak wpisow\n";
        } else {
            for (const std::string& line : entry.debug_logic_logs) {
                out << "  - " << line << "\n";
            }
        }
    }

    auto writeFilteredList = [&](const std::string& fileName, const std::string& title, auto predicate) {
        const fs::path listPath = folderOutDir / fileName;
        std::ofstream listOut(listPath);
        if (!listOut.is_open()) {
            std::cerr << "Nie mozna zapisac raportu: " << listPath.string() << "\n";
            return;
        }

        listOut << title << "\n";
        listOut << "Folder: " << folderKey << "\n\n";

        bool anyMatch = false;
        for (const PuzzleReportEntry& entry : stats.puzzle_reports) {
            if (!entry.valid) continue;
            if (!predicate(entry)) continue;
            anyMatch = true;
            listOut << entry.source_file << " | Linia: " << entry.line_no << "\n";
        }

        if (!anyMatch) {
            listOut << "Brak wpisow.\n";
        }
    };

    writeFilteredList(
        "sprzeczne_tak.txt",
        "=== Sudoku ze statusem: Sprzeczne = TAK ===",
        [](const PuzzleReportEntry& entry) { return entry.contradiction; }
    );
    writeFilteredList(
        "liczba_rozwiazan_wiecej_niz_1.txt",
        "=== Sudoku ze statusem: Liczba rozwiazan > 1 ===",
        [](const PuzzleReportEntry& entry) { return entry.solution_count > 1; }
    );
    writeFilteredList(
        "wymaga_zgadywania_tak.txt",
        "=== Sudoku ze statusem: Wymaga zgadywania = TAK ===",
        [](const PuzzleReportEntry& entry) { return entry.requires_guessing; }
    );
}

static void writeGlobalSummary(const fs::path& outDir, const std::map<std::string, FolderStats>& allStats,
                               long long txtFilesScanned) {
    const fs::path outPath = outDir / "podsumowanie_folderow.txt";
    std::ofstream out(outPath);
    if (!out.is_open()) {
        std::cerr << "Nie mozna zapisac podsumowania globalnego: " << outPath.string() << "\n";
        return;
    }

    long long allNonEmpty = 0;
    long long allInvalid = 0;
    long long allAnalyzed = 0;
    long long allSolved = 0;
    long long allGuess = 0;
    long long allSolvedWithBacktracking = 0;
    long long allContradictions = 0;
    long long allUnique = 0;
    long long allMultiple = 0;
    long long allNoSolution = 0;
    long long allBacktrackingNodes = 0;
    long long allBacktrackingDecisions = 0;
    long long allBacktrackingBacktracks = 0;
    long long allDifficultySum = 0;
    long long allDifficultyCount = 0;
    int allMaxDifficulty = 0;

    for (const auto& entry : allStats) {
        const FolderStats& st = entry.second;
        allNonEmpty += st.non_empty_lines;
        allInvalid += st.invalid_lines;
        allAnalyzed += st.analyzed_puzzles;
        allSolved += st.solved_logically;
        allGuess += st.requires_guessing;
        allSolvedWithBacktracking += st.solved_with_backtracking;
        allContradictions += st.contradictions;
        allUnique += st.unique_solutions;
        allMultiple += st.multiple_solutions;
        allNoSolution += st.no_solution;
        allBacktrackingNodes += st.backtracking_nodes_sum;
        allBacktrackingDecisions += st.backtracking_decisions_sum;
        allBacktrackingBacktracks += st.backtracking_backtracks_sum;
        allDifficultySum += st.difficulty_sum;
        allDifficultyCount += st.difficulty_count;
        if (st.max_difficulty > allMaxDifficulty) {
            allMaxDifficulty = st.max_difficulty;
        }
    }

    out << "=== Podsumowanie Globalne ===\n";
    out << "Liczba folderow: " << allStats.size() << "\n";
    out << "Przeskanowane pliki .txt: " << txtFilesScanned << "\n";
    out << "Niepuste wpisy (linie): " << allNonEmpty << "\n";
    out << "Poprawnie przeanalizowane plansze: " << allAnalyzed << "\n";
    out << "Bledne wpisy: " << allInvalid << "\n\n";
    out << "Rozwiazane logicznie: " << allSolved << "\n";
    out << "Wymaga zgadywania/backtrackingu: " << allGuess << "\n";
    out << "Rozwiazane z uzyciem backtrackingu: " << allSolvedWithBacktracking << "\n";
    out << "Sprzeczne plansze: " << allContradictions << "\n";
    out << "Unikalne rozwiazania: " << allUnique << "\n";
    out << "Wiele rozwiazan: " << allMultiple << "\n";
    out << "Brak rozwiazania: " << allNoSolution << "\n";
    out << "Backtracking - decyzje: " << allBacktrackingDecisions << "\n";
    out << "Backtracking - backtracki: " << allBacktrackingBacktracks << "\n";
    out << "Backtracking - nody rekursji: " << allBacktrackingNodes << "\n\n";
    out << "Poziom trudnosci sudoku (1-8): ";
    if (allDifficultyCount > 0) {
        const double avgDifficulty = static_cast<double>(allDifficultySum) / static_cast<double>(allDifficultyCount);
        out << std::fixed << std::setprecision(2) << avgDifficulty
            << " (max: " << allMaxDifficulty << ")\n\n";
    } else {
        out << "brak danych\n\n";
    }

    out << "=== Szczegoly per folder ===\n";
    for (const auto& entry : allStats) {
        const FolderStats& st = entry.second;
        const double avgDifficulty = (st.difficulty_count > 0)
            ? static_cast<double>(st.difficulty_sum) / static_cast<double>(st.difficulty_count)
            : 0.0;
        out << "- " << entry.first
            << " | analyzed=" << st.analyzed_puzzles
            << " | invalid=" << st.invalid_lines
            << " | logic=" << st.solved_logically
            << " | guess=" << st.requires_guessing
            << " | bt_solved=" << st.solved_with_backtracking
            << " | unique=" << st.unique_solutions
            << " | diff=" << std::fixed << std::setprecision(2) << avgDifficulty
            << " | hardest=" << st.hardest_name_seen
            << "\n";
    }
}

static void writeFolderCsv(const fs::path& outDir, const std::map<std::string, FolderStats>& allStats) {
    const fs::path csvPath = outDir / "statystyki_folderow.csv";
    std::ofstream out(csvPath);
    if (!out.is_open()) {
        std::cerr << "Nie mozna zapisac CSV: " << csvPath.string() << "\n";
        return;
    }

    out << "folder,non_empty_lines,analyzed_puzzles,invalid_lines,solved_logically,requires_guessing,"
        << "solved_with_backtracking,contradictions,unique_solutions,multiple_solutions,no_solution,"
        << "backtracking_decisions,backtracking_backtracks,backtracking_nodes,avg_clues,hardest_seen,"
        << "avg_difficulty,max_difficulty,"
        << "naked_single,hidden_single,naked_pair,hidden_pair,pointing_pairs_triples,box_line_reduction,"
        << "naked_triple,hidden_triple,naked_quad,hidden_quad,"
        << "x_wing,y_wing,xyz_wing,wxyz_wing,swordfish,jellyfish,franken_mutant_fish,kraken_fish,skyscraper,two_string_kite,simple_coloring,three_d_medusa,"
        << "finned_x_wing_sashimi,finned_swordfish,finned_jellyfish,empty_rectangle,unique_rectangle,unique_loop,bivalue_oddagon,avoidable_rectangle,bug_plus_1,"
        << "remote_pairs,w_wing,grouped_x_cycle,x_chain,xy_chain,grouped_aic,aic,continuous_nice_loop,"
        << "als_xz,als_xy_wing,als_chain,death_blossom,sue_de_coq,msls,exocet,senior_exocet,sk_loop,pattern_overlay_method,forcing_chains,backtracking\n";

    for (const auto& entry : allStats) {
        const std::string& folder = entry.first;
        const FolderStats& st = entry.second;
        const double avgClues = (st.analyzed_puzzles > 0)
            ? static_cast<double>(st.clues_sum) / static_cast<double>(st.analyzed_puzzles)
            : 0.0;
        const double avgDifficulty = (st.difficulty_count > 0)
            ? static_cast<double>(st.difficulty_sum) / static_cast<double>(st.difficulty_count)
            : 0.0;

        auto countUsage = [&](Strategy s) -> long long {
            return st.strategy_usage[static_cast<int>(s)];
        };

        out << csvEscape(folder) << ","
            << st.non_empty_lines << ","
            << st.analyzed_puzzles << ","
            << st.invalid_lines << ","
            << st.solved_logically << ","
            << st.requires_guessing << ","
            << st.solved_with_backtracking << ","
            << st.contradictions << ","
            << st.unique_solutions << ","
            << st.multiple_solutions << ","
            << st.no_solution << ","
            << st.backtracking_decisions_sum << ","
            << st.backtracking_backtracks_sum << ","
            << st.backtracking_nodes_sum << ","
            << std::fixed << std::setprecision(2) << avgClues << ","
            << csvEscape(st.hardest_name_seen) << ","
            << std::fixed << std::setprecision(2) << avgDifficulty << ","
            << st.max_difficulty << ","
            << countUsage(Strategy::NakedSingle) << ","
            << countUsage(Strategy::HiddenSingle) << ","
            << countUsage(Strategy::NakedPair) << ","
            << countUsage(Strategy::HiddenPair) << ","
            << countUsage(Strategy::PointingPairsTriples) << ","
            << countUsage(Strategy::BoxLineReduction) << ","
            << countUsage(Strategy::NakedTriple) << ","
            << countUsage(Strategy::HiddenTriple) << ","
            << countUsage(Strategy::NakedQuad) << ","
            << countUsage(Strategy::HiddenQuad) << ","
            << countUsage(Strategy::XWing) << ","
            << countUsage(Strategy::YWing) << ","
            << countUsage(Strategy::XYZWing) << ","
            << countUsage(Strategy::WXYZWing) << ","
            << countUsage(Strategy::Swordfish) << ","
            << countUsage(Strategy::Jellyfish) << ","
            << countUsage(Strategy::FrankenMutantFish) << ","
            << countUsage(Strategy::KrakenFish) << ","
            << countUsage(Strategy::Skyscraper) << ","
            << countUsage(Strategy::TwoStringKite) << ","
            << countUsage(Strategy::SimpleColoring) << ","
            << countUsage(Strategy::ThreeDMedusa) << ","
            << countUsage(Strategy::FinnedXWingSashimi) << ","
            << countUsage(Strategy::FinnedSwordfish) << ","
            << countUsage(Strategy::FinnedJellyfish) << ","
            << countUsage(Strategy::EmptyRectangle) << ","
            << countUsage(Strategy::UniqueRectangle) << ","
            << countUsage(Strategy::UniqueLoop) << ","
            << countUsage(Strategy::BivalueOddagon) << ","
            << countUsage(Strategy::AvoidableRectangle) << ","
            << countUsage(Strategy::BUGPlus1) << ","
            << countUsage(Strategy::RemotePairs) << ","
            << countUsage(Strategy::WWing) << ","
            << countUsage(Strategy::GroupedXCycle) << ","
            << countUsage(Strategy::XChain) << ","
            << countUsage(Strategy::XYChain) << ","
            << countUsage(Strategy::GroupedAIC) << ","
            << countUsage(Strategy::AIC) << ","
            << countUsage(Strategy::ContinuousNiceLoop) << ","
            << countUsage(Strategy::ALSXZ) << ","
            << countUsage(Strategy::ALSXYWing) << ","
            << countUsage(Strategy::ALSChain) << ","
            << countUsage(Strategy::DeathBlossom) << ","
            << countUsage(Strategy::SueDeCoq) << ","
            << countUsage(Strategy::MSLS) << ","
            << countUsage(Strategy::Exocet) << ","
            << countUsage(Strategy::SeniorExocet) << ","
            << countUsage(Strategy::SKLoop) << ","
            << countUsage(Strategy::PatternOverlayMethod) << ","
            << countUsage(Strategy::ForcingChains) << ","
            << countUsage(Strategy::Backtracking)
            << "\n";
    }
}

static std::size_t parseThreadOverrideFromEnv() {
    const char* raw = std::getenv("SUDOKU_ANALYZER_THREADS");
    if (raw == nullptr || *raw == '\0') {
        return 0;
    }

    char* end = nullptr;
    const unsigned long long value = std::strtoull(raw, &end, 10);
    if (end == raw || *end != '\0' || value == 0ULL) {
        return 0;
    }

    constexpr std::size_t kMaxThreads = 1024;
    if (value > static_cast<unsigned long long>(kMaxThreads)) {
        return kMaxThreads;
    }
    return static_cast<std::size_t>(value);
}

static std::size_t computeWorkerCount(std::size_t taskCount) {
    return computeWorkerCountWithPreferred(taskCount, 0);
}

static std::size_t computeWorkerCountWithPreferred(std::size_t taskCount, std::size_t preferred) {
    if (taskCount == 0) {
        return 0;
    }

    const unsigned int hw = std::thread::hardware_concurrency();
    std::size_t workerCount = static_cast<std::size_t>(hw > 0 ? hw : 4U);
    if (preferred > 0) {
        workerCount = preferred;
    } else {
        const std::size_t envOverride = parseThreadOverrideFromEnv();
        if (envOverride > 0) {
            workerCount = envOverride;
        }
    }
    if (workerCount == 0) {
        workerCount = 1;
    }
    if (workerCount > taskCount) {
        workerCount = taskCount;
    }
    return workerCount;
}

static std::size_t computeChunkSize(std::size_t workerCount) {
    if (workerCount <= 1) {
        return 1;
    }
    const std::size_t scaled = workerCount * 2;
    return std::clamp<std::size_t>(scaled, 4, 32);
}

static std::string normalizeToken(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    return out;
}

static const std::vector<Strategy>& strategyOrder() {
    static const std::vector<Strategy> order = {
        Strategy::NakedSingle, Strategy::HiddenSingle,
        Strategy::NakedPair, Strategy::HiddenPair,
        Strategy::PointingPairsTriples, Strategy::BoxLineReduction,
        Strategy::NakedTriple, Strategy::HiddenTriple,
        Strategy::NakedQuad, Strategy::HiddenQuad,
        Strategy::XWing, Strategy::YWing, Strategy::XYZWing, Strategy::WXYZWing, Strategy::Swordfish, Strategy::Jellyfish, Strategy::FrankenMutantFish, Strategy::KrakenFish,
        Strategy::Skyscraper, Strategy::TwoStringKite, Strategy::SimpleColoring, Strategy::ThreeDMedusa, Strategy::FinnedXWingSashimi, Strategy::FinnedSwordfish, Strategy::FinnedJellyfish, Strategy::EmptyRectangle,
        Strategy::UniqueRectangle, Strategy::UniqueLoop, Strategy::BivalueOddagon, Strategy::AvoidableRectangle, Strategy::BUGPlus1,
        Strategy::RemotePairs, Strategy::WWing, Strategy::GroupedXCycle, Strategy::XChain, Strategy::XYChain, Strategy::GroupedAIC, Strategy::AIC, Strategy::ContinuousNiceLoop,
        Strategy::ALSXZ, Strategy::ALSXYWing, Strategy::ALSChain, Strategy::DeathBlossom, Strategy::SueDeCoq, Strategy::MSLS, Strategy::Exocet, Strategy::SeniorExocet, Strategy::SKLoop, Strategy::PatternOverlayMethod,
        Strategy::ForcingChains, Strategy::Backtracking
    };
    return order;
}

static std::vector<Strategy> strategiesForDifficultyLevel(int level) {
    std::vector<Strategy> out;
    const int target = std::clamp(level, kDifficultyMinLevel, kDifficultyMaxLevel);
    for (Strategy s : strategyOrder()) {
        if (strategyRank(s) == target) {
            out.push_back(s);
        }
    }
    return out;
}

static std::optional<Strategy> parseStrategyToken(const std::string& text) {
    static std::unordered_map<std::string, Strategy> strategyMap;
    if (strategyMap.empty()) {
        for (Strategy s : strategyOrder()) {
            strategyMap.emplace(normalizeToken(strategyName(s)), s);
        }
        strategyMap.emplace("xwing", Strategy::XWing);
        strategyMap.emplace("xywing", Strategy::YWing);
        strategyMap.emplace("xyzwing", Strategy::XYZWing);
        strategyMap.emplace("als", Strategy::ALSXZ);
        strategyMap.emplace("aic", Strategy::AIC);
    }

    const auto it = strategyMap.find(normalizeToken(text));
    if (it == strategyMap.end()) {
        return std::nullopt;
    }
    if (it->second == Strategy::Backtracking) {
        return std::nullopt;
    }
    return it->second;
}

static std::string selectOutputTxtFileDialog() {
    IFileSaveDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileSaveDialog, reinterpret_cast<void**>(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        return "";
    }

    dialog->SetTitle(L"Wybierz plik wyjsciowy generatora Sudoku");
    dialog->SetDefaultExtension(L"txt");
    dialog->SetFileName(L"generated_sudoku.txt");
    const COMDLG_FILTERSPEC filters[] = {
        {L"Pliki TXT", L"*.txt"},
        {L"Wszystkie pliki", L"*.*"}
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

    std::string outPath;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR selected = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected)) && selected != nullptr) {
                const std::wstring ws(selected);
                outPath.assign(ws.begin(), ws.end());
                CoTaskMemFree(selected);
            }
            item->Release();
        }
    }
    dialog->Release();
    return outPath;
}

static std::string selectOutputFolderDialog() {
    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        return "";
    }

    DWORD opts = 0;
    if (SUCCEEDED(dialog->GetOptions(&opts))) {
        dialog->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    }
    dialog->SetTitle(L"Wybierz folder wyjsciowy (<nazwa>_1...<nazwa>_N)");

    std::string outPath;
    if (SUCCEEDED(dialog->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR selected = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected)) && selected != nullptr) {
                const std::wstring ws(selected);
                outPath.assign(ws.begin(), ws.end());
                CoTaskMemFree(selected);
            }
            item->Release();
        }
    }
    dialog->Release();
    return outPath;
}

static bool isIndexedOutputFileName(const std::string& name, const std::string& baseName, const std::string& extension) {
    if (baseName.empty() || extension.empty()) return false;
    const std::string prefix = baseName + "_";
    if (name.size() <= prefix.size() + extension.size()) return false;
    if (name.rfind(prefix, 0) != 0) return false;
    if (name.substr(name.size() - extension.size()) != extension) return false;

    const std::size_t begin = prefix.size();
    const std::size_t end = name.size() - extension.size();
    if (begin >= end) return false;
    for (std::size_t i = begin; i < end; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return false;
    }
    return true;
}

struct GeneratorConfigDialogState {
    GenerateRunConfig cfg;
    bool accepted = false;
    bool finished = false;
    HWND hwnd = nullptr;
    HWND edit_box_rows = nullptr;
    HWND edit_box_cols = nullptr;
    HWND edit_count = nullptr;
    HWND edit_min_clues = nullptr;
    HWND edit_max_clues = nullptr;
    HWND combo_difficulty = nullptr;
    HWND edit_threads = nullptr;
    HWND edit_seed = nullptr;
    HWND edit_reseed_interval = nullptr;
    HWND edit_attempt_time_budget_s = nullptr;
    HWND edit_attempt_node_budget_s = nullptr;
    HWND edit_max_attempts = nullptr;
    HWND combo_required_strategy = nullptr;
    HWND edit_output_folder = nullptr;
    HWND edit_output_file = nullptr;
    HWND chk_symmetry = nullptr;
    HWND chk_unique = nullptr;
};

enum : int {
    IDC_BOX_ROWS = 2001,
    IDC_BOX_COLS = 2002,
    IDC_COUNT = 2003,
    IDC_MIN_CLUES = 2004,
    IDC_MAX_CLUES = 2005,
    IDC_DIFFICULTY_REQUIRED = 2006,
    IDC_THREADS = 2008,
    IDC_SEED = 2009,
    IDC_MAX_ATTEMPTS = 2010,
    IDC_REQUIRED_STRATEGY = 2011,
    IDC_OUTPUT_FOLDER = 2012,
    IDC_OUTPUT_FILE = 2013,
    IDC_SYMMETRY = 2014,
    IDC_UNIQUE = 2015,
    IDC_BROWSE_OUTPUT_FOLDER = 2016,
    IDC_BROWSE_OUTPUT_FILE = 2017,
    IDC_START = 2018,
    IDC_CANCEL = 2019,
    IDC_APPLY_CLUES_PRESET = 2020,
    IDC_RESEED_INTERVAL = 2021,
    IDC_ATTEMPT_TIME_BUDGET_S = 2022,
    IDC_ATTEMPT_NODE_BUDGET_S = 2023
};

static std::string getWindowTextStringA(HWND hwnd) {
    if (hwnd == nullptr) return "";
    const int len = GetWindowTextLengthA(hwnd);
    std::string out;
    if (len > 0) {
        out.resize(static_cast<std::size_t>(len) + 1ULL);
        GetWindowTextA(hwnd, out.data(), len + 1);
        out.resize(static_cast<std::size_t>(len));
    }
    return out;
}

static void setWindowTextStringA(HWND hwnd, const std::string& text) {
    if (hwnd != nullptr) {
        SetWindowTextA(hwnd, text.c_str());
    }
}

static LRESULT CALLBACK GeneratorConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GeneratorConfigDialogState* st = reinterpret_cast<GeneratorConfigDialogState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            const auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
            auto* initState = reinterpret_cast<GeneratorConfigDialogState*>(cs->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(initState));
            return TRUE;
        }
        case WM_CREATE: {
            st = reinterpret_cast<GeneratorConfigDialogState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            if (st == nullptr) return -1;
            st->hwnd = hwnd;

            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            auto mkLabel = [&](int x, int y, int w, int h, const char* text) {
                HWND c = CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE,
                                         x, y, w, h, hwnd, nullptr, nullptr, nullptr);
                SendMessageA(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            };
            auto mkEdit = [&](int id, int x, int y, int w, int h, const std::string& text) -> HWND {
                HWND c = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text.c_str(),
                                         WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                         x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                SendMessageA(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                return c;
            };
            auto mkBtn = [&](int id, int x, int y, int w, int h, const char* text, DWORD style) -> HWND {
                HWND c = CreateWindowExA(0, "BUTTON", text, WS_CHILD | WS_VISIBLE | style,
                                         x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                SendMessageA(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                return c;
            };
            auto mkCombo = [&](int id, int x, int y, int w, int dropH) -> HWND {
                HWND c = CreateWindowExA(WS_EX_CLIENTEDGE, "COMBOBOX", "",
                                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         x, y, w, dropH, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                SendMessageA(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                return c;
            };

            int y = 16;
            constexpr int LX = 16;
            constexpr int EX = 210;
            constexpr int W = 340;
            constexpr int H = 24;
            constexpr int STEP = 30;

            mkLabel(LX, y, 180, H, "box_rows:");
            st->edit_box_rows = mkEdit(IDC_BOX_ROWS, EX, y, 80, H, std::to_string(st->cfg.box_rows));
            y += STEP;

            mkLabel(LX, y, 180, H, "box_cols:");
            st->edit_box_cols = mkEdit(IDC_BOX_COLS, EX, y, 80, H, std::to_string(st->cfg.box_cols));
            y += STEP;

            mkLabel(LX, y, 180, H, "target_puzzles:");
            st->edit_count = mkEdit(IDC_COUNT, EX, y, 120, H, std::to_string(st->cfg.target_puzzles));
            y += STEP;

            mkLabel(LX, y, 180, H, "min_clues:");
            st->edit_min_clues = mkEdit(IDC_MIN_CLUES, EX, y, 120, H, std::to_string(st->cfg.min_clues));
            y += STEP;

            mkLabel(LX, y, 180, H, "max_clues:");
            st->edit_max_clues = mkEdit(IDC_MAX_CLUES, EX, y, 120, H, std::to_string(st->cfg.max_clues));
            y += STEP;

            mkLabel(LX, y, 180, H, "difficulty_level_required:");
            st->combo_difficulty = mkCombo(IDC_DIFFICULTY_REQUIRED, EX, y, W, 300);
            {
                const char* diffNames[] = {
                    "1 - Podstawowy",
                    "2 - Sredniozaawansowany-1",
                    "3 - Sredniozaawansowany-2",
                    "4 - Zaawansowany",
                    "5 - Ekspert",
                    "6 - Mistrzowski",
                    "7 - Diabelski",
                    "8 - Teoretyczny",
                    "9 - Backtracking/Brutalny"
                };
                for (int i = 0; i < 9; ++i) {
                    SendMessageA(st->combo_difficulty, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(diffNames[i]));
                }
                const int selIdx = std::clamp(st->cfg.difficulty_required, 1, 9) - 1;
                SendMessageA(st->combo_difficulty, CB_SETCURSEL, static_cast<WPARAM>(selIdx), 0);
            }
            y += STEP;

            mkLabel(LX, y, 180, H, "clues_preset:");
            mkBtn(IDC_APPLY_CLUES_PRESET, EX, y, 260, H, "Auto (wg difficulty + rozmiaru)", BS_PUSHBUTTON);
            y += STEP;

            mkLabel(LX, y, 180, H, "threads (0=auto):");
            st->edit_threads = mkEdit(IDC_THREADS, EX, y, 120, H, std::to_string(st->cfg.explicit_threads));
            y += STEP;

            mkLabel(LX, y, 180, H, "seed (0=random):");
            st->edit_seed = mkEdit(IDC_SEED, EX, y, 180, H, std::to_string(st->cfg.seed));
            y += STEP;

            mkLabel(LX, y, 180, H, "reseed_interval_s (0=off, full worker reset):");
            st->edit_reseed_interval = mkEdit(IDC_RESEED_INTERVAL, EX, y, 120, H, std::to_string(st->cfg.reseed_interval_seconds));
            y += STEP;

            mkLabel(LX, y, 180, H, "attempt_time_budget_s (0=auto):");
            st->edit_attempt_time_budget_s = mkEdit(IDC_ATTEMPT_TIME_BUDGET_S, EX, y, 120, H, std::to_string(st->cfg.attempt_time_budget_s));
            y += STEP;

            mkLabel(LX, y, 180, H, "attempt_node_budget_s (0=auto):");
            st->edit_attempt_node_budget_s = mkEdit(IDC_ATTEMPT_NODE_BUDGET_S, EX, y, 180, H, std::to_string(st->cfg.attempt_node_budget_s));
            y += STEP;

            mkLabel(LX, y, 180, H, "max_attempts (0=bez limitu):");
            st->edit_max_attempts = mkEdit(IDC_MAX_ATTEMPTS, EX, y, 180, H, std::to_string(st->cfg.max_attempts));
            y += STEP;

            mkLabel(LX, y, 180, H, "required_strategy (opcjonalnie):");
            st->combo_required_strategy = mkCombo(IDC_REQUIRED_STRATEGY, EX, y, W, 400);
            {
                SendMessageA(st->combo_required_strategy, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("(brak)"));
                int selIdx = 0;
                int idx = 1;
                for (Strategy s : strategyOrder()) {
                    if (s == Strategy::Backtracking) continue;
                    const std::string name = strategyName(s);
                    SendMessageA(st->combo_required_strategy, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
                    if (st->cfg.required_strategy.has_value() && *st->cfg.required_strategy == s) {
                        selIdx = idx;
                    }
                    ++idx;
                }
                SendMessageA(st->combo_required_strategy, CB_SETCURSEL, static_cast<WPARAM>(selIdx), 0);
            }
            y += STEP;

            mkLabel(LX, y, 180, H, "output_folder:");
            st->edit_output_folder = mkEdit(IDC_OUTPUT_FOLDER, EX, y, W, H, st->cfg.output_folder.string());
            mkBtn(IDC_BROWSE_OUTPUT_FOLDER, EX + W + 8, y, 70, H, "...", BS_PUSHBUTTON);
            y += STEP;

            mkLabel(LX, y, 180, H, "output_file:");
            st->edit_output_file = mkEdit(IDC_OUTPUT_FILE, EX, y, W, H, st->cfg.output_file.string());
            mkBtn(IDC_BROWSE_OUTPUT_FILE, EX + W + 8, y, 70, H, "...", BS_PUSHBUTTON);
            y += STEP;

            st->chk_symmetry = mkBtn(IDC_SYMMETRY, LX, y, 260, H, "symmetry_center", BS_AUTOCHECKBOX);
            st->chk_unique = mkBtn(IDC_UNIQUE, LX + 280, y, 220, H, "require_unique (wymuszone)", BS_AUTOCHECKBOX);
            SendMessageA(st->chk_symmetry, BM_SETCHECK, st->cfg.symmetry_center ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageA(st->chk_unique, BM_SETCHECK, BST_CHECKED, 0);
            EnableWindow(st->chk_unique, FALSE);
            y += STEP + 8;

            mkBtn(IDC_START, EX, y, 140, 30, "Start", BS_DEFPUSHBUTTON);
            mkBtn(IDC_CANCEL, EX + 160, y, 140, 30, "Anuluj", BS_PUSHBUTTON);

            return 0;
        }
        case WM_COMMAND: {
            if (st == nullptr) break;
            const int id = LOWORD(wParam);
            if (id == IDC_BROWSE_OUTPUT_FOLDER) {
                const std::string outDir = selectOutputFolderDialog();
                if (!outDir.empty()) {
                    setWindowTextStringA(st->edit_output_folder, outDir);
                }
                return 0;
            }
            if (id == IDC_BROWSE_OUTPUT_FILE) {
                const std::string out = selectOutputTxtFileDialog();
                if (!out.empty()) {
                    setWindowTextStringA(st->edit_output_file, out);
                }
                return 0;
            }
            if (id == IDC_APPLY_CLUES_PRESET) {
                int boxRows = 0, boxCols = 0;
                if (!parseIntStrict(trim(getWindowTextStringA(st->edit_box_rows)), boxRows) || boxRows <= 0) {
                    MessageBoxA(hwnd, "Niepoprawne box_rows.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!parseIntStrict(trim(getWindowTextStringA(st->edit_box_cols)), boxCols) || boxCols <= 0) {
                    MessageBoxA(hwnd, "Niepoprawne box_cols.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                const int requiredLevel = static_cast<int>(SendMessageA(st->combo_difficulty, CB_GETCURSEL, 0, 0)) + 1;

                const int side = boxRows * boxCols;
                if (side <= 0 || side > 36) {
                    MessageBoxA(hwnd, "Niepoprawny rozmiar planszy (N=box_rows*box_cols, 1..36).", "Blad", MB_ICONERROR | MB_OK); return 0;
                }

                const ClueRange preset = recommendedClueRangeForLevel(side, requiredLevel);
                setWindowTextStringA(st->edit_min_clues, std::to_string(preset.min_clues));
                setWindowTextStringA(st->edit_max_clues, std::to_string(preset.max_clues));
                return 0;
            }
            if (id == IDC_CANCEL) {
                st->accepted = false;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_START) {
                auto readInt = [&](HWND edit, int& out) -> bool {
                    return parseIntStrict(trim(getWindowTextStringA(edit)), out);
                };
                auto readLL = [&](HWND edit, long long& out) -> bool {
                    return parseLLStrict(trim(getWindowTextStringA(edit)), out);
                };

                GenerateRunConfig c = st->cfg;
                int tmpI = 0;
                long long tmpLL = 0;

                if (!readInt(st->edit_box_rows, c.box_rows) || c.box_rows <= 0) {
                    MessageBoxA(hwnd, "Niepoprawne box_rows.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readInt(st->edit_box_cols, c.box_cols) || c.box_cols <= 0) {
                    MessageBoxA(hwnd, "Niepoprawne box_cols.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readLL(st->edit_count, c.target_puzzles) || c.target_puzzles <= 0) {
                    MessageBoxA(hwnd, "Niepoprawne target_puzzles.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readInt(st->edit_min_clues, c.min_clues) || !readInt(st->edit_max_clues, c.max_clues)) {
                    MessageBoxA(hwnd, "Niepoprawne min/max clues.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                c.difficulty_required = static_cast<int>(SendMessageA(st->combo_difficulty, CB_GETCURSEL, 0, 0)) + 1;
                if (!readLL(st->edit_seed, c.seed)) {
                    MessageBoxA(hwnd, "Niepoprawny seed.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readInt(st->edit_reseed_interval, c.reseed_interval_seconds) || c.reseed_interval_seconds < 0) {
                    MessageBoxA(hwnd, "Niepoprawne reseed_interval_s (>=0).", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readInt(st->edit_attempt_time_budget_s, c.attempt_time_budget_s) || c.attempt_time_budget_s < 0) {
                    MessageBoxA(hwnd, "Niepoprawne attempt_time_budget_s (>=0).", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readInt(st->edit_attempt_node_budget_s, c.attempt_node_budget_s) || c.attempt_node_budget_s < 0) {
                    MessageBoxA(hwnd, "Niepoprawne attempt_node_budget_s (>=0).", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readLL(st->edit_max_attempts, c.max_attempts)) {
                    MessageBoxA(hwnd, "Niepoprawny max_attempts.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                if (!readInt(st->edit_threads, tmpI) || tmpI < 0) {
                    MessageBoxA(hwnd, "Niepoprawne threads (>=0).", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                c.explicit_threads = static_cast<std::size_t>(tmpI);

                {
                    const int stratSel = static_cast<int>(SendMessageA(st->combo_required_strategy, CB_GETCURSEL, 0, 0));
                    c.required_strategy.reset();
                    c.required_strategy_text.clear();
                    if (stratSel > 0) {
                        int idx = 1;
                        for (Strategy s : strategyOrder()) {
                            if (s == Strategy::Backtracking) continue;
                            if (idx == stratSel) {
                                c.required_strategy = s;
                                c.required_strategy_text = strategyName(s);
                                break;
                            }
                            ++idx;
                        }
                    }
                }

                const std::string outDirPath = trim(getWindowTextStringA(st->edit_output_folder));
                if (outDirPath.empty()) {
                    MessageBoxA(hwnd, "Podaj output_folder.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                c.output_folder = fs::path(outDirPath);

                const std::string outPath = trim(getWindowTextStringA(st->edit_output_file));
                if (outPath.empty()) {
                    MessageBoxA(hwnd, "Podaj output_file.", "Blad", MB_ICONERROR | MB_OK); return 0;
                }
                c.output_file = fs::path(outPath);

                c.symmetry_center = (SendMessageA(st->chk_symmetry, BM_GETCHECK, 0, 0) == BST_CHECKED);
                c.require_unique = true;

                st->cfg = c;
                st->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (st != nullptr) st->accepted = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (st != nullptr) st->finished = true;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static bool showGeneratorConfigWindow(GenerateRunConfig& cfg) {
    static const char* kClassName = "SudokuBulkGeneratorConfigWnd";
    static bool classRegistered = false;

    if (!classRegistered) {
        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = GeneratorConfigWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kClassName;
        if (!RegisterClassExA(&wc)) {
            MessageBoxA(nullptr, "Nie mozna zarejestrowac klasy okna konfiguracji.", "Blad", MB_ICONERROR | MB_OK);
            return false;
        }
        classRegistered = true;
    }

    GeneratorConfigDialogState state;
    state.cfg = cfg;

    HWND hwnd = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        kClassName,
        "Bulk Generator Sudoku - Konfiguracja",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 670, 660,
        nullptr, nullptr, GetModuleHandleA(nullptr), &state
    );
    if (hwnd == nullptr) {
        MessageBoxA(nullptr, "Nie mozna utworzyc okna konfiguracji.", "Blad", MB_ICONERROR | MB_OK);
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (!state.finished) {
        const BOOL gm = GetMessageA(&msg, nullptr, 0, 0);
        if (gm <= 0) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (state.accepted) {
        cfg = state.cfg;
    }
    return state.accepted;
}

static SudokuBoard boardFromGrid(int boxRows, int boxCols, long long seed, const std::vector<int>& puzzleGrid) {
    SudokuBoard board;
    board.seed = seed;
    board.block_rows = boxRows;
    board.block_cols = boxCols;
    board.side_size = boxRows * boxCols;
    board.total_cells = board.side_size * board.side_size;

    if (board.block_rows <= 0 || board.block_cols <= 0 || board.side_size <= 0 || board.side_size > 36) {
        board.error = "Niepoprawna geometria planszy";
        return board;
    }
    if (static_cast<int>(puzzleGrid.size()) != board.total_cells) {
        board.error = "Niepoprawny rozmiar danych planszy";
        return board;
    }

    board.cells.resize(board.total_cells);
    for (int i = 0; i < board.total_cells; ++i) {
        const int v = puzzleGrid[i];
        if (v >= 1 && v <= board.side_size) {
            // Wewnetrznie: liczba > 0 w puzzleGrid oznacza dana (komorka stala).
            board.cells[i].value = v;
            board.cells[i].revealed = true;
        } else {
            board.cells[i].value = 0;
            board.cells[i].revealed = false;
        }
    }
    board.valid = true;
    return board;
}

static std::string puzzleLineFromPuzzleAndSolution(long long seed, int boxRows, int boxCols,
                                                   const std::vector<int>& puzzleGrid,
                                                   const std::vector<int>& solvedGrid) {
    const int N = boxRows * boxCols;
    const int NN = N * N;
    std::ostringstream out;
    out << seed << "," << boxRows << "," << boxCols;
    for (int i = 0; i < NN; ++i) {
        const int given = (i < static_cast<int>(puzzleGrid.size())) ? puzzleGrid[i] : 0;
        const int solved = (i < static_cast<int>(solvedGrid.size())) ? solvedGrid[i] : 0;
        if (given > 0) {
            // Nowy, wspolny format: tN/TN = dana.
            out << ",t" << given;
        } else if (solved > 0) {
            // Nowy, wspolny format: N = pole do odgadniecia.
            out << "," << solved;
        } else {
            out << ",0";
        }
    }
    return out.str();
}

static bool verifyGeneratedPuzzleLineStrict(const std::string& puzzleLine, int boxRows, int boxCols,
                                            const std::vector<int>& solvedGrid,
                                            AnalysisReport* outReport) {
    const int N = boxRows * boxCols;
    const int NN = N * N;
    if (N <= 0 || N > 36) {
        return false;
    }
    if (static_cast<int>(solvedGrid.size()) != NN) {
        return false;
    }
    if (!isCompleteGridValid(boxRows, boxCols, solvedGrid)) {
        return false;
    }

    const SudokuBoard reparsedBoard = parseSudokuLine(puzzleLine);
    if (!reparsedBoard.valid) {
        return false;
    }
    if (reparsedBoard.block_rows != boxRows || reparsedBoard.block_cols != boxCols) {
        return false;
    }
    if (reparsedBoard.total_cells != NN || static_cast<int>(reparsedBoard.cells.size()) != NN) {
        return false;
    }

    for (int i = 0; i < NN; ++i) {
        const int expected = solvedGrid[i];
        const Cell& c = reparsedBoard.cells[i];
        if (expected < 1 || expected > N) {
            return false;
        }
        if (c.value != expected) {
            return false;
        }
    }

    SudokuAnalyzer verificationAnalyzer(reparsedBoard);
    AnalysisReport verificationReport = verificationAnalyzer.run();
    verificationReport.solution_count = countSolutionsWithBacktracking(reparsedBoard, 2);
    verificationReport.unique_solution = (verificationReport.solution_count == 1);
    if (outReport != nullptr) {
        *outReport = verificationReport;
    }

    if (verificationReport.contradiction) {
        return false;
    }
    if (verificationReport.solution_count != 1) {
        return false;
    }
    if (!verificationReport.solved_logically) {
        return false;
    }
    if (verificationReport.requires_guessing) {
        return false;
    }
    if (verificationReport.solved_with_backtracking) {
        return false;
    }
    return true;
}

static bool generateSolvedGridRandom(int boxRows, int boxCols, std::mt19937_64& rng, std::vector<int>& outGrid) {
    const int N = boxRows * boxCols;
    if (N <= 0 || N > 36) {
        return false;
    }
    const int NN = N * N;
    const int boxesPerRow = N / boxCols;
    const uint64_t allMask = (N >= 63) ? 0ULL : ((1ULL << N) - 1ULL);
    auto bitFor = [](int d) -> uint64_t { return 1ULL << (d - 1); };
    auto boxIndex = [&](int r, int c) -> int { return (r / boxRows) * boxesPerRow + (c / boxCols); };

    outGrid.assign(NN, 0);
    std::vector<uint64_t> rowUsed(N, 0ULL), colUsed(N, 0ULL), boxUsed(N, 0ULL);

    std::function<bool()> dfs = [&]() -> bool {
        if (generationAttemptDeadlineReached()) {
            return false;
        }
        int bestCell = -1;
        int bestCount = INT_MAX;
        uint64_t bestMask = 0ULL;

        for (int idx = 0; idx < NN; ++idx) {
            if (outGrid[idx] != 0) continue;
            const int r = idx / N;
            const int c = idx % N;
            const int b = boxIndex(r, c);
            const uint64_t allowed = allMask & ~(rowUsed[r] | colUsed[c] | boxUsed[b]);
            const int cnt = bits(allowed);
            if (cnt == 0) return false;
            if (cnt < bestCount) {
                bestCount = cnt;
                bestCell = idx;
                bestMask = allowed;
                if (cnt == 1) break;
            }
        }

        if (bestCell < 0) {
            return true;
        }

        std::array<int, 32> candidates{};
        int candCount = 0;
        uint64_t mask = bestMask;
        while (mask) {
            const uint64_t one = mask & (~mask + 1ULL);
            candidates[candCount++] = firstDigit(one);
            mask &= (mask - 1ULL);
        }
        std::shuffle(candidates.begin(), candidates.begin() + candCount, rng);

        const int r = bestCell / N;
        const int c = bestCell % N;
        const int b = boxIndex(r, c);
        for (int i = 0; i < candCount; ++i) {
            const int d = candidates[i];
            const uint64_t bt = bitFor(d);
            outGrid[bestCell] = d;
            rowUsed[r] |= bt;
            colUsed[c] |= bt;
            boxUsed[b] |= bt;

            if (dfs()) return true;

            outGrid[bestCell] = 0;
            rowUsed[r] &= ~bt;
            colUsed[c] &= ~bt;
            boxUsed[b] &= ~bt;
        }
        return false;
    };

    return dfs();
}

static bool isCompleteGridValid(int boxRows, int boxCols, const std::vector<int>& grid) {
    const int N = boxRows * boxCols;
    const int NN = N * N;
    if (N <= 0 || N > 36 || static_cast<int>(grid.size()) != NN) {
        return false;
    }
    const int boxesPerRow = N / boxCols;

    auto boxIdx = [&](int r, int c) -> int {
        return (r / boxRows) * boxesPerRow + (c / boxCols);
    };

    std::vector<uint64_t> rowMask(N, 0ULL), colMask(N, 0ULL), boxMask(N, 0ULL);
    for (int idx = 0; idx < NN; ++idx) {
        const int v = grid[idx];
        if (v < 1 || v > N) {
            return false;
        }
        const int r = idx / N;
        const int c = idx % N;
        const int b = boxIdx(r, c);
        const uint64_t bitMask = 1ULL << (v - 1);
        if ((rowMask[r] & bitMask) || (colMask[c] & bitMask) || (boxMask[b] & bitMask)) {
            return false;
        }
        rowMask[r] |= bitMask;
        colMask[c] |= bitMask;
        boxMask[b] |= bitMask;
    }
    return true;
}

static bool buildPuzzleByDiggingHoles(const std::vector<int>& solvedGrid, const GenerateRunConfig& cfg,
                                      std::mt19937_64& rng, std::vector<int>& outPuzzleGrid,
                                      int minClues, int maxClues) {
    if (generationAttemptDeadlineReached()) {
        return false;
    }
    const int N = cfg.box_rows * cfg.box_cols;
    const int NN = N * N;
    if (static_cast<int>(solvedGrid.size()) != NN) {
        return false;
    }
    if (minClues < 0 || maxClues > NN || minClues > maxClues) {
        return false;
    }

    outPuzzleGrid = solvedGrid;
    std::vector<int> order(NN);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    std::vector<char> touched(NN, 0);

    auto symmetricCell = [&](int idx) -> int {
        const int r = idx / N;
        const int c = idx % N;
        return (N - 1 - r) * N + (N - 1 - c);
    };

    int clues = NN;
    for (int idx : order) {
        if (generationAttemptDeadlineReached()) {
            return false;
        }
        if (touched[idx]) continue;

        const int pairIdx = cfg.symmetry_center ? symmetricCell(idx) : idx;
        touched[idx] = 1;
        touched[pairIdx] = 1;

        const bool hasA = outPuzzleGrid[idx] != 0;
        const bool hasB = (pairIdx != idx) && (outPuzzleGrid[pairIdx] != 0);
        const int removed = static_cast<int>(hasA) + static_cast<int>(hasB);
        if (removed == 0) continue;
        if (clues - removed < minClues) continue;

        const int backupA = outPuzzleGrid[idx];
        const int backupB = outPuzzleGrid[pairIdx];
        outPuzzleGrid[idx] = 0;
        if (pairIdx != idx) {
            outPuzzleGrid[pairIdx] = 0;
        }

        bool keepRemoval = true;
        if (generationAttemptDeadlineReached()) {
            return false;
        }
        const SudokuBoard testBoard = boardFromGrid(cfg.box_rows, cfg.box_cols, 0, outPuzzleGrid);
        if (!testBoard.valid || countSolutionsWithBacktracking(testBoard, 2) != 1) {
            keepRemoval = false;
        }

        if (keepRemoval) {
            clues -= removed;
            if (clues <= minClues) {
                break;
            }
        } else {
            outPuzzleGrid[idx] = backupA;
            outPuzzleGrid[pairIdx] = backupB;
        }
    }

    if (clues < minClues || clues > maxClues) {
        return false;
    }
    return true;
}

static bool puzzleMatchesDifficulty(const SudokuBoard& board, const GenerateRunConfig& cfg, AnalysisReport& outReport,
                                    const std::optional<Strategy>& attemptRequiredStrategy,
                                    bool* outFailedRequiredStrategy) {
    if (outFailedRequiredStrategy != nullptr) {
        *outFailedRequiredStrategy = false;
    }
    if (!board.valid) {
        return false;
    }
    SudokuAnalyzer analyzer(board);
    outReport = analyzer.run();
    if (generationAttemptDeadlineReached()) {
        return false;
    }
    outReport.solution_count = countSolutionsWithBacktracking(board, 2);
    if (generationAttemptDeadlineReached()) {
        return false;
    }
    outReport.unique_solution = (outReport.solution_count == 1);

    if (outReport.solution_count <= 0) {
        return false;
    }
    if (outReport.solution_count != 1) {
        return false;
    }
    const int level = difficultyLevelFromReport(outReport);
    if (level > cfg.difficulty_required) {
        return false;
    }
    const std::optional<Strategy> effectiveRequiredStrategy = cfg.required_strategy.has_value()
        ? cfg.required_strategy
        : attemptRequiredStrategy;
    if (effectiveRequiredStrategy.has_value()) {
        const int stratUsageCnt = outReport.strategy_usage[static_cast<int>(*effectiveRequiredStrategy)];
        if (stratUsageCnt <= 0) {
            if (outFailedRequiredStrategy != nullptr) {
                *outFailedRequiredStrategy = true;
            }
            return false;
        }
    } else if (!reportUsesStrategyAtLevel(outReport, cfg.difficulty_required)) {
        return false;
    }
    return true;
}

struct GenerationStatusWindowState {
    HWND hwnd = nullptr;
    HWND edit_status = nullptr;
    std::atomic<bool>* stop_requested = nullptr;
};

static LRESULT CALLBACK GenerationStatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GenerationStatusWindowState* st = reinterpret_cast<GenerationStatusWindowState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            const auto* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
            auto* initState = reinterpret_cast<GenerationStatusWindowState*>(cs->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(initState));
            return TRUE;
        }
        case WM_CREATE: {
            st = reinterpret_cast<GenerationStatusWindowState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            if (st == nullptr) return -1;
            st->hwnd = hwnd;

            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            st->edit_status = CreateWindowExA(
                WS_EX_CLIENTEDGE,
                "EDIT",
                "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                8, 8, 760, 500,
                hwnd, nullptr, nullptr, nullptr
            );
            SendMessageA(st->edit_status, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return 0;
        }
        case WM_SIZE: {
            if (st != nullptr && st->edit_status != nullptr) {
                const int width = std::max<int>(40, LOWORD(lParam) - 16);
                const int height = std::max<int>(40, HIWORD(lParam) - 16);
                MoveWindow(st->edit_status, 8, 8, width, height, TRUE);
            }
            return 0;
        }
        case WM_CLOSE: {
            if (st != nullptr && st->stop_requested != nullptr) {
                st->stop_requested->store(true, std::memory_order_relaxed);
            }
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            if (st != nullptr) {
                st->hwnd = nullptr;
                st->edit_status = nullptr;
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static bool showGenerationStatusWindow(GenerationStatusWindowState& state) {
    static const char* kClassName = "SudokuBulkGeneratorProgressWnd";
    static bool classRegistered = false;

    if (!classRegistered) {
        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = GenerationStatusWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kClassName;
        if (!RegisterClassExA(&wc)) {
            return false;
        }
        classRegistered = true;
    }

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        kClassName,
        "Bulk Generator Sudoku - Status",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 820, 620,
        nullptr, nullptr, GetModuleHandleA(nullptr), &state
    );
    if (hwnd == nullptr) {
        return false;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

static void pumpGenerationUiMessages() {
    MSG msg{};
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static GenerateRunResult runGenerateMode(const GenerateRunConfig& cfg) {
    const int N = cfg.box_rows * cfg.box_cols;
    const int NN = N * N;
    if (cfg.box_rows <= 0 || cfg.box_cols <= 0 || N <= 0 || N > 36) {
        std::cerr << "Niepoprawna geometria. Dozwolone: box_rows > 0, box_cols > 0, N <= 16.\n";
        return GenerateRunResult{1};
    }
    if (cfg.target_puzzles <= 0) {
        std::cerr << "target_puzzles musi byc > 0.\n";
        return GenerateRunResult{1};
    }
    if (cfg.min_clues < 0 || cfg.max_clues > NN || cfg.min_clues > cfg.max_clues) {
        std::cerr << "Niepoprawny zakres clues.\n";
        return GenerateRunResult{1};
    }
    if (cfg.difficulty_required < kDifficultyMinLevel ||
        cfg.difficulty_required > kDifficultyMaxLevel) {
        std::cerr << "Niepoprawny difficulty_level_required (1-9).\n";
        return GenerateRunResult{1};
    }
    if (cfg.reseed_interval_seconds < 0) {
        std::cerr << "Niepoprawny reseed_interval_s (>=0).\n";
        return GenerateRunResult{1};
    }
    if (cfg.attempt_time_budget_s < 0) {
        std::cerr << "Niepoprawny attempt_time_budget_s (>=0).\n";
        return GenerateRunResult{1};
    }
    if (cfg.attempt_node_budget_s < 0) {
        std::cerr << "Niepoprawny attempt_node_budget_s (>=0).\n";
        return GenerateRunResult{1};
    }
    if (!cfg.required_strategy_text.empty() && !cfg.required_strategy.has_value()) {
        std::cerr << "Nieznana wymagana strategia: " << cfg.required_strategy_text << "\n";
        return GenerateRunResult{1};
    }

    std::error_code ec;
    std::string singleBaseName = cfg.output_file.filename().stem().string();
    std::string singleExt = cfg.output_file.filename().extension().string();
    if (singleBaseName.empty()) {
        singleBaseName = "sudoku";
    }
    if (singleExt.empty()) {
        singleExt = ".txt";
    }

    fs::create_directories(cfg.output_folder, ec);
    if (ec) {
        std::cerr << "Nie mozna utworzyc folderu wyjsciowego: " << cfg.output_folder.string() << "\n";
        return GenerateRunResult{1};
    }

    for (fs::directory_iterator it(cfg.output_folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        const std::string name = it->path().filename().string();
        if (!isIndexedOutputFileName(name, singleBaseName, singleExt)) continue;
        std::error_code removeEc;
        fs::remove(it->path(), removeEc);
    }
    ec.clear();

    if (!cfg.output_file.parent_path().empty()) {
        fs::create_directories(cfg.output_file.parent_path(), ec);
    }
    {
        std::ofstream initOut(cfg.output_file, std::ios::trunc);
        if (!initOut.is_open()) {
            std::cerr << "Nie mozna utworzyc pliku wyjsciowego: " << cfg.output_file.string() << "\n";
            return GenerateRunResult{1};
        }
    }

    const std::size_t target = static_cast<std::size_t>(cfg.target_puzzles);
    const std::size_t workerCount = std::max<std::size_t>(1, computeWorkerCountWithPreferred(target, cfg.explicit_threads));
    const std::size_t queueCapacity = std::max<std::size_t>(128, workerCount * 16);

    GeneratedOutputQueue outputQueue(queueCapacity);
    std::atomic<std::size_t> accepted{0};
    std::atomic<std::size_t> written{0};
    std::atomic<std::size_t> writeErrors{0};
    std::atomic<std::size_t> rejectedAtVerification{0};
    std::atomic<long long> attempts{0};
    std::atomic<std::size_t> activeWorkers{workerCount};
    std::atomic<bool> stopRequested{false};
    std::vector<int> workerCurrentClues(workerCount, -1);
    std::vector<unsigned long long> workerCurrentSeeds(workerCount, 0ULL);
    std::vector<unsigned long long> workerResetCounts(workerCount, 0ULL);
    std::vector<unsigned long long> workerAppliedResetCounts(workerCount, 0ULL);
    std::vector<unsigned long long> workerResetLagMax(workerCount, 0ULL);
    std::vector<long long> workerResetRemainingSeconds(workerCount, -1LL);
    std::vector<unsigned char> workerRunning(workerCount, 0U);
    std::vector<std::chrono::steady_clock::time_point> workerNextResetAt(workerCount);
    std::mutex workerCurrentCluesMutex;

    GenerationStatusWindowState statusWindow;
    statusWindow.stop_requested = &stopRequested;
    showGenerationStatusWindow(statusWindow);

    std::jthread writer([&](std::stop_token) {
        std::ofstream out(cfg.output_file, std::ios::app);
        if (!out.is_open()) {
            stopRequested.store(true, std::memory_order_relaxed);
            return;
        }
        GeneratedOutputItem item;
        while (outputQueue.pop(item)) {
            out << item.line << "\n";

            const fs::path singlePath = cfg.output_folder / (singleBaseName + "_" + std::to_string(item.index) + singleExt);
            std::ofstream single(singlePath, std::ios::trunc);
            if (!single.is_open()) {
                writeErrors.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            single << item.line << "\n";
            written.fetch_add(1, std::memory_order_relaxed);
        }
    });

    auto mixSeed64 = [](unsigned long long x) -> unsigned long long {
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    };

    const unsigned long long seedBase = (cfg.seed != 0)
        ? static_cast<unsigned long long>(cfg.seed)
        : static_cast<unsigned long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::atomic<unsigned long long> reseedEvents{0ULL};
    const bool workerResetEnabled = cfg.reseed_interval_seconds > 0;
    const auto workerResetInterval = std::chrono::seconds(cfg.reseed_interval_seconds);
    const std::vector<Strategy> autoRequiredStrategyPool = cfg.required_strategy.has_value()
        ? std::vector<Strategy>{}
        : strategiesForDifficultyLevel(cfg.difficulty_required);

    std::vector<std::jthread> workers;
    workers.reserve(workerCount);
    for (std::size_t workerId = 0; workerId < workerCount; ++workerId) {
        workers.emplace_back([&, workerId](std::stop_token st) {
            auto workerSeedFrom = [&](unsigned long long baseSeed, unsigned long long resetNo) -> unsigned long long {
                const unsigned long long workerSalt = 0x9E3779B97F4A7C15ULL * (workerId + 1ULL);
                const unsigned long long resetSalt = 0xD2B74407B1CE6E93ULL * resetNo;
                return mixSeed64(baseSeed + workerSalt + resetSalt);
            };

            std::mt19937_64 rng;
            std::uniform_int_distribution<int> clueDist(cfg.min_clues, cfg.max_clues);
            unsigned long long appliedWorkerResetNo = 0ULL;
            unsigned long long currentWorkerSeed = 0ULL;
            int currentTargetClues = cfg.min_clues;
            std::vector<Strategy> workerStrategyCycle = autoRequiredStrategyPool;
            std::size_t workerStrategyIndex = 0;
            std::optional<Strategy> workerRequiredStrategy = cfg.required_strategy;

            auto resetWorkerStrategy = [&]() {
                if (cfg.required_strategy.has_value()) {
                    workerRequiredStrategy = cfg.required_strategy;
                    return;
                }
                if (workerStrategyCycle.empty()) {
                    workerRequiredStrategy.reset();
                    return;
                }
                std::shuffle(workerStrategyCycle.begin(), workerStrategyCycle.end(), rng);
                workerStrategyIndex = 0;
                workerRequiredStrategy = workerStrategyCycle[workerStrategyIndex];
            };

            auto advanceWorkerStrategy = [&]() {
                if (cfg.required_strategy.has_value() || workerStrategyCycle.empty()) {
                    return;
                }
                workerStrategyIndex = (workerStrategyIndex + 1) % workerStrategyCycle.size();
                workerRequiredStrategy = workerStrategyCycle[workerStrategyIndex];
            };

            auto resetWorkerState = [&](unsigned long long resetNo) {
                unsigned long long baseForReset = seedBase;
                if (resetNo > 0ULL) {
                    const unsigned long long ticks = static_cast<unsigned long long>(
                        std::chrono::high_resolution_clock::now().time_since_epoch().count()
                    );
                    baseForReset = mixSeed64(seedBase ^ ticks ^ (resetNo * 0x9E3779B97F4A7C15ULL));
                }
                currentWorkerSeed = workerSeedFrom(baseForReset, resetNo);
                rng.seed(currentWorkerSeed);
                currentTargetClues = clueDist(rng);
                resetWorkerStrategy();
                {
                    std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                    workerCurrentClues[workerId] = currentTargetClues;
                    workerCurrentSeeds[workerId] = currentWorkerSeed;
                    workerAppliedResetCounts[workerId] = resetNo;
                    workerRunning[workerId] = 1U;
                    if (workerResetEnabled) {
                        const auto now = std::chrono::steady_clock::now();
                        // Timer resetu dziala od startu workera i po kazdym kolejnym resecie.
                        workerNextResetAt[workerId] = now + workerResetInterval;
                        const auto nextAt = workerNextResetAt[workerId];
                        if (now >= nextAt) {
                            workerResetRemainingSeconds[workerId] = 0LL;
                        } else {
                            const auto msLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
                                nextAt - now
                            ).count();
                            workerResetRemainingSeconds[workerId] = (msLeft + 999LL) / 1000LL;
                        }
                        const unsigned long long scheduled = workerResetCounts[workerId];
                        const unsigned long long pendingLag = (scheduled >= workerAppliedResetCounts[workerId])
                            ? (scheduled - workerAppliedResetCounts[workerId])
                            : 0ULL;
                        if (pendingLag > workerResetLagMax[workerId]) {
                            workerResetLagMax[workerId] = pendingLag;
                        }
                    } else {
                        workerResetRemainingSeconds[workerId] = -1LL;
                    }
                }
            };

            auto restartResetTimerFromSuccessfulGeneration = [&]() {
                if (!workerResetEnabled) {
                    return;
                }
                const auto now = std::chrono::steady_clock::now();
                std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                workerNextResetAt[workerId] = now + workerResetInterval;
                // Sukces anuluje ewentualny oczekujacy reset: licz od nowa od ostatniej wygenerowanej planszy.
                workerResetCounts[workerId] = workerAppliedResetCounts[workerId];
                workerResetRemainingSeconds[workerId] = cfg.reseed_interval_seconds;
            };
            const std::chrono::milliseconds attemptTimeBudget = [&]() {
                if (cfg.attempt_time_budget_s > 0) {
                    return std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::seconds(cfg.attempt_time_budget_s)
                    );
                }
                if (!workerResetEnabled) {
                    return std::chrono::milliseconds(2500);
                }
                auto budget = std::chrono::duration_cast<std::chrono::milliseconds>(workerResetInterval) / 2;
                if (budget < std::chrono::milliseconds(500)) {
                    budget = std::chrono::milliseconds(500);
                }
                return budget;
            }();
            const long long attemptNodeBudget = [&]() -> long long {
                if (cfg.attempt_node_budget_s > 0) {
                    // Przyblizony przelicznik sekund->nody, zalezny od rozmiaru planszy.
                    const long long nodesPerSecond = (N >= 16) ? 40000LL : ((N >= 12) ? 80000LL : 200000LL);
                    return std::max(1000LL, static_cast<long long>(cfg.attempt_node_budget_s) * nodesPerSecond);
                }
                return (N >= 16) ? 200000LL : ((N >= 12) ? 400000LL : 1000000LL);
            }();
            resetWorkerState(appliedWorkerResetNo);
            while (!st.stop_requested() && !stopRequested.load(std::memory_order_relaxed)) {
                if (accepted.load(std::memory_order_relaxed) >= target) {
                    break;
                }

                if (workerResetEnabled) {
                    unsigned long long scheduledResetNo = appliedWorkerResetNo;
                    {
                        std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                        scheduledResetNo = workerResetCounts[workerId];
                    }
                    if (scheduledResetNo != appliedWorkerResetNo) {
                        appliedWorkerResetNo = scheduledResetNo;
                        reseedEvents.fetch_add(1ULL, std::memory_order_relaxed);
                        resetWorkerState(appliedWorkerResetNo);
                        continue;
                    }
                }

                const long long attemptNo = attempts.fetch_add(1, std::memory_order_relaxed) + 1LL;
                if (cfg.max_attempts > 0 && attemptNo > cfg.max_attempts) {
                    break;
                }

                GenerationAttemptLimitScope attemptLimits(attemptTimeBudget, attemptNodeBudget);
                std::vector<int> solvedGrid;
                if (!generateSolvedGridRandom(cfg.box_rows, cfg.box_cols, rng, solvedGrid)) {
                    continue;
                }
                if (generationAttemptDeadlineReached()) {
                    continue;
                }
                if (!isCompleteGridValid(cfg.box_rows, cfg.box_cols, solvedGrid)) {
                    continue;
                }
                std::vector<int> puzzleGrid;
                if (!buildPuzzleByDiggingHoles(
                        solvedGrid,
                        cfg,
                        rng,
                        puzzleGrid,
                        currentTargetClues,
                        currentTargetClues)) {
                    continue;
                }
                if (generationAttemptDeadlineReached()) {
                    continue;
                }

                SudokuBoard board = boardFromGrid(cfg.box_rows, cfg.box_cols, static_cast<long long>(rng()), puzzleGrid);
                if (!board.valid) {
                    continue;
                }

                AnalysisReport report;
                bool failedRequiredStrategy = false;
                if (!puzzleMatchesDifficulty(board, cfg, report, workerRequiredStrategy, &failedRequiredStrategy)) {
                    if (failedRequiredStrategy) {
                        advanceWorkerStrategy();
                    }
                    continue;
                }
                if (generationAttemptDeadlineReached()) {
                    continue;
                }

                const std::string puzzleLine = puzzleLineFromPuzzleAndSolution(
                    board.seed,
                    cfg.box_rows,
                    cfg.box_cols,
                    puzzleGrid,
                    solvedGrid
                );
                if (!verifyGeneratedPuzzleLineStrict(puzzleLine, cfg.box_rows, cfg.box_cols, solvedGrid, nullptr)) {
                    rejectedAtVerification.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (generationAttemptDeadlineReached()) {
                    continue;
                }

                const std::size_t slot = accepted.fetch_add(1, std::memory_order_relaxed);
                if (slot >= target) {
                    break;
                }
                restartResetTimerFromSuccessfulGeneration();
                GeneratedOutputItem item;
                item.index = slot + 1U;
                item.line = puzzleLine;
                bool pushed = false;
                while (!st.stop_requested() && !stopRequested.load(std::memory_order_relaxed)) {
                    const auto pushResult = outputQueue.push_for(std::move(item), std::chrono::milliseconds(100));
                    if (pushResult == GeneratedOutputQueue::PushWaitResult::Pushed) {
                        pushed = true;
                        break;
                    }
                    if (pushResult == GeneratedOutputQueue::PushWaitResult::Closed) {
                        break;
                    }
                    if (workerResetEnabled) {
                        unsigned long long scheduledResetNo = appliedWorkerResetNo;
                        {
                            std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                            scheduledResetNo = workerResetCounts[workerId];
                        }
                        if (scheduledResetNo != appliedWorkerResetNo) {
                            appliedWorkerResetNo = scheduledResetNo;
                            reseedEvents.fetch_add(1ULL, std::memory_order_relaxed);
                            resetWorkerState(appliedWorkerResetNo);
                        }
                    }
                }
                if (!pushed) {
                    break;
                }
            }
            {
                std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                workerCurrentClues[workerId] = -1;
                workerCurrentSeeds[workerId] = 0ULL;
                workerResetRemainingSeconds[workerId] = -1LL;
                workerRunning[workerId] = 0ULL;
                workerNextResetAt[workerId] = std::chrono::steady_clock::time_point{};
            }

            if (activeWorkers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                outputQueue.close();
            }
        });
    }

    std::jthread resetScheduler;
    if (workerResetEnabled) {
        resetScheduler = std::jthread([&](std::stop_token st) {
            while (!st.stop_requested() && !stopRequested.load(std::memory_order_relaxed)) {
                const auto now = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                    for (std::size_t i = 0; i < workerCount; ++i) {
                        if (workerRunning[i] == 0ULL) {
                            workerResetRemainingSeconds[i] = -1LL;
                            continue;
                        }

                        auto& nextAt = workerNextResetAt[i];
                        if (nextAt.time_since_epoch().count() <= 0) {
                            // Fallback: jesli timer nie jest ustawiony, uzbroj go od teraz.
                            nextAt = now + workerResetInterval;
                        }

                        unsigned long long scheduled = workerResetCounts[i];
                        const unsigned long long applied = workerAppliedResetCounts[i];
                        unsigned long long pendingLag = (scheduled >= applied) ? (scheduled - applied) : 0ULL;
                        if (pendingLag > 1ULL) {
                            scheduled = applied + 1ULL;
                            workerResetCounts[i] = scheduled;
                            pendingLag = 1ULL;
                        }

                        if (now >= nextAt) {
                            // Coalescing: trzymamy maks. 1 oczekujacy reset per worker.
                            // Następne "nextAt" ustawia dopiero worker przy realnym resecie.
                            if (pendingLag == 0ULL) {
                                workerResetCounts[i] += 1ULL;
                            }
                        }

                        if (now >= nextAt) {
                            workerResetRemainingSeconds[i] = 0LL;
                        } else {
                            const auto msLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
                                nextAt - now
                            ).count();
                            workerResetRemainingSeconds[i] = (msLeft + 999LL) / 1000LL;
                        }

                        const unsigned long long scheduledNow = workerResetCounts[i];
                        const unsigned long long appliedNow = workerAppliedResetCounts[i];
                        const unsigned long long pendingLagNow = (scheduledNow >= appliedNow) ? (scheduledNow - appliedNow) : 0ULL;
                        if (pendingLagNow > workerResetLagMax[i]) {
                            workerResetLagMax[i] = pendingLagNow;
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    const auto start = std::chrono::steady_clock::now();
    auto lastPrint = start;
    const auto consoleTablePrintInterval = std::chrono::seconds(10);
    auto lastConsoleTablePrint = start - consoleTablePrintInterval;
    while (true) {
        pumpGenerationUiMessages();
        const auto now = std::chrono::steady_clock::now();
        if (now - lastPrint >= std::chrono::milliseconds(500)) {
            lastPrint = now;
            {
                const auto curWritten = written.load(std::memory_order_relaxed);
                const double elSec = std::chrono::duration<double>(now - start).count();
                const long long rateH = (elSec > 0.0)
                    ? static_cast<long long>(3600.0 * static_cast<double>(curWritten) / elSec)
                    : 0LL;
                std::cout << "\rGenerowanie: accepted=" << accepted.load(std::memory_order_relaxed)
                          << "/" << target
                          << ", written=" << curWritten
                          << ", attempts=" << attempts.load(std::memory_order_relaxed)
                          << ", rejected=" << rejectedAtVerification.load(std::memory_order_relaxed)
                          << ", " << rateH << "/h"
                          << ", workers=" << activeWorkers.load(std::memory_order_relaxed)
                          << "      " << std::flush;
            }

            if (statusWindow.edit_status != nullptr) {
                std::vector<int> cluesSnapshot;
                std::vector<unsigned long long> seedsSnapshot;
                std::vector<unsigned long long> resetCountsSnapshot;
                std::vector<unsigned long long> appliedResetCountsSnapshot;
                std::vector<unsigned long long> resetLagMaxSnapshot;
                std::vector<long long> resetInSnapshot;
                {
                    std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                    cluesSnapshot = workerCurrentClues;
                    seedsSnapshot = workerCurrentSeeds;
                    resetCountsSnapshot = workerResetCounts;
                    appliedResetCountsSnapshot = workerAppliedResetCounts;
                    resetLagMaxSnapshot = workerResetLagMax;
                    resetInSnapshot = workerResetRemainingSeconds;
                }
                const auto guiWritten = written.load(std::memory_order_relaxed);
                const double guiElSec = std::chrono::duration<double>(now - start).count();
                const long long guiRateH = (guiElSec > 0.0)
                    ? static_cast<long long>(3600.0 * static_cast<double>(guiWritten) / guiElSec)
                    : 0LL;
                std::ostringstream statusText;
                statusText << "Generowanie Sudoku - status\n"
                           << "accepted: " << accepted.load(std::memory_order_relaxed) << "/" << target << "\n"
                           << "written: " << guiWritten << "\n"
                           << "attempts: " << attempts.load(std::memory_order_relaxed) << "\n"
                           << "odrzucone po weryfikacji: " << rejectedAtVerification.load(std::memory_order_relaxed) << "\n"
                           << "plansze/godz: " << guiRateH << "\n"
                           << "active_workers: " << activeWorkers.load(std::memory_order_relaxed) << "\n"
                           << "reseed_events: " << reseedEvents.load(std::memory_order_relaxed) << "\n"
                           << "seed_base: " << seedBase << "\n"
                           << "clues_range: [" << cfg.min_clues << ", " << cfg.max_clues << "]\n\n"
                           << "Workery (clues + seed + resety):\n";
                const std::size_t workerDigits = std::max<std::size_t>(2, std::to_string(cluesSnapshot.size()).size());
                statusText << std::left
                           << std::setw(static_cast<int>(7 + workerDigits)) << "worker"
                           << " | " << std::setw(5) << "clues"
                           << " | " << std::setw(20) << "seed"
                           << " | " << std::setw(6) << "resets"
                           << " | " << std::setw(7) << "applied"
                           << " | " << std::setw(9) << "reset_lag"
                           << " | " << std::setw(8) << "lag_max"
                           << " | " << std::setw(10) << "reset_in_s"
                           << " | status\n";
                statusText << std::string(static_cast<std::size_t>(92 + workerDigits), '-') << "\n";
                for (std::size_t i = 0; i < cluesSnapshot.size(); ++i) {
                    statusText << "worker_" << std::right << std::setw(static_cast<int>(workerDigits))
                               << std::setfill('0') << (i + 1) << std::setfill(' ') << std::left
                               << " | ";
                    const unsigned long long pendingLag = (resetCountsSnapshot[i] >= appliedResetCountsSnapshot[i])
                        ? (resetCountsSnapshot[i] - appliedResetCountsSnapshot[i])
                        : 0ULL;
                    if (cluesSnapshot[i] >= 0) {
                        const std::string resetInText = (resetInSnapshot[i] >= 0)
                            ? std::to_string(resetInSnapshot[i])
                            : std::string("-");
                        statusText << std::right << std::setw(5) << cluesSnapshot[i]
                                   << " | " << std::setw(20) << seedsSnapshot[i]
                                   << " | " << std::setw(6) << resetCountsSnapshot[i]
                                   << " | " << std::setw(7) << appliedResetCountsSnapshot[i]
                                   << " | " << std::setw(9) << pendingLag
                                   << " | " << std::setw(8) << resetLagMaxSnapshot[i]
                                   << " | " << std::setw(10) << resetInText
                                   << " | aktywny";
                    } else {
                        statusText << std::right << std::setw(5) << "-"
                                   << " | " << std::setw(20) << "-"
                                   << " | " << std::setw(6) << resetCountsSnapshot[i]
                                   << " | " << std::setw(7) << appliedResetCountsSnapshot[i]
                                   << " | " << std::setw(9) << pendingLag
                                   << " | " << std::setw(8) << resetLagMaxSnapshot[i]
                                   << " | " << std::setw(10) << "-"
                                   << " | zatrzymany";
                    }
                    statusText << std::left << "\n";
                }
                setWindowTextStringA(statusWindow.edit_status, statusText.str());
            }

            if (now - lastConsoleTablePrint >= consoleTablePrintInterval) {
                lastConsoleTablePrint = now;
                std::vector<int> cluesSnapshot;
                std::vector<unsigned long long> seedsSnapshot;
                std::vector<unsigned long long> resetCountsSnapshot;
                std::vector<unsigned long long> appliedResetCountsSnapshot;
                std::vector<unsigned long long> resetLagMaxSnapshot;
                std::vector<long long> resetInSnapshot;
                {
                    std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
                    cluesSnapshot = workerCurrentClues;
                    seedsSnapshot = workerCurrentSeeds;
                    resetCountsSnapshot = workerResetCounts;
                    appliedResetCountsSnapshot = workerAppliedResetCounts;
                    resetLagMaxSnapshot = workerResetLagMax;
                    resetInSnapshot = workerResetRemainingSeconds;
                }
                std::ostringstream table;
                const std::size_t workerDigits = std::max<std::size_t>(2, std::to_string(cluesSnapshot.size()).size());
                const auto tblWritten = written.load(std::memory_order_relaxed);
                const double tblElSec = std::chrono::duration<double>(now - start).count();
                const long long tblRateH = (tblElSec > 0.0)
                    ? static_cast<long long>(3600.0 * static_cast<double>(tblWritten) / tblElSec)
                    : 0LL;
                table << "\n\n=== Statystyki Generowania (console) ===\n";
                table << "accepted=" << accepted.load(std::memory_order_relaxed) << "/" << target
                      << ", written=" << tblWritten
                      << ", attempts=" << attempts.load(std::memory_order_relaxed)
                      << ", rejected=" << rejectedAtVerification.load(std::memory_order_relaxed)
                      << ", " << tblRateH << "/h"
                      << ", active_workers=" << activeWorkers.load(std::memory_order_relaxed)
                      << ", reseeds=" << reseedEvents.load(std::memory_order_relaxed) << "\n";
                table << std::left
                      << std::setw(static_cast<int>(7 + workerDigits)) << "worker"
                      << " | " << std::setw(5) << "clues"
                      << " | " << std::setw(20) << "seed"
                      << " | " << std::setw(6) << "resets"
                      << " | " << std::setw(7) << "applied"
                      << " | " << std::setw(9) << "reset_lag"
                      << " | " << std::setw(8) << "lag_max"
                      << " | " << std::setw(10) << "reset_in_s"
                      << " | status\n";
                table << std::string(static_cast<std::size_t>(92 + workerDigits), '-') << "\n";
                for (std::size_t i = 0; i < cluesSnapshot.size(); ++i) {
                    table << "worker_" << std::right << std::setw(static_cast<int>(workerDigits))
                          << std::setfill('0') << (i + 1) << std::setfill(' ') << std::left
                          << " | ";
                    const unsigned long long pendingLag = (resetCountsSnapshot[i] >= appliedResetCountsSnapshot[i])
                        ? (resetCountsSnapshot[i] - appliedResetCountsSnapshot[i])
                        : 0ULL;
                    if (cluesSnapshot[i] >= 0) {
                        const std::string resetInText = (resetInSnapshot[i] >= 0)
                            ? std::to_string(resetInSnapshot[i])
                            : std::string("-");
                        table << std::right << std::setw(5) << cluesSnapshot[i]
                              << " | " << std::setw(20) << seedsSnapshot[i]
                              << " | " << std::setw(6) << resetCountsSnapshot[i]
                              << " | " << std::setw(7) << appliedResetCountsSnapshot[i]
                              << " | " << std::setw(9) << pendingLag
                              << " | " << std::setw(8) << resetLagMaxSnapshot[i]
                              << " | " << std::setw(10) << resetInText
                              << " | aktywny";
                    } else {
                        table << std::right << std::setw(5) << "-"
                              << " | " << std::setw(20) << "-"
                              << " | " << std::setw(6) << resetCountsSnapshot[i]
                              << " | " << std::setw(7) << appliedResetCountsSnapshot[i]
                              << " | " << std::setw(9) << pendingLag
                              << " | " << std::setw(8) << resetLagMaxSnapshot[i]
                              << " | " << std::setw(10) << "-"
                              << " | zatrzymany";
                    }
                    table << std::left << "\n";
                }
                std::cout << table.str() << std::flush;
            }
        }

        const bool enough = accepted.load(std::memory_order_relaxed) >= target;
        const bool workersDone = activeWorkers.load(std::memory_order_relaxed) == 0;
        const bool stopByUi = stopRequested.load(std::memory_order_relaxed);
        if (enough || workersDone || stopByUi) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    stopRequested.store(true, std::memory_order_relaxed);
    pumpGenerationUiMessages();
    for (std::jthread& worker : workers) {
        worker.request_stop();
    }
    workers.clear();
    if (resetScheduler.joinable()) {
        resetScheduler.request_stop();
        resetScheduler.join();
    }
    outputQueue.close();
    if (writer.joinable()) {
        writer.request_stop();
        writer.join();
    }
    if (statusWindow.hwnd != nullptr) {
        DestroyWindow(statusWindow.hwnd);
        pumpGenerationUiMessages();
    }

    const auto end = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(end - start).count();
    const std::size_t finalAccepted = std::min(accepted.load(std::memory_order_relaxed), target);
    const std::size_t finalWritten = written.load(std::memory_order_relaxed);
    const std::size_t finalWriteErrors = writeErrors.load(std::memory_order_relaxed);
    const long long finalAttempts = attempts.load(std::memory_order_relaxed);
    const std::size_t finalRejectedAtVerification = rejectedAtVerification.load(std::memory_order_relaxed);
    const unsigned long long finalReseeds = reseedEvents.load(std::memory_order_relaxed);
    std::vector<unsigned long long> finalResetCountsSnapshot;
    std::vector<unsigned long long> finalAppliedResetCountsSnapshot;
    std::vector<unsigned long long> finalResetLagMaxSnapshot;
    {
        std::lock_guard<std::mutex> lock(workerCurrentCluesMutex);
        finalResetCountsSnapshot = workerResetCounts;
        finalAppliedResetCountsSnapshot = workerAppliedResetCounts;
        finalResetLagMaxSnapshot = workerResetLagMax;
    }

    std::cout << "\n\n=== Podsumowanie Generowania ===\n";
    std::cout << std::left << std::setw(36) << "Plik zbiorczy" << " : " << cfg.output_file.string() << "\n";
    std::cout << std::left << std::setw(36) << "Folder plikow" << " : " << cfg.output_folder.string() << "\n";
    std::cout << std::left << std::setw(36) << "Wzor pliku pojedynczego" << " : "
              << singleBaseName << "_N" << singleExt << "\n";
    std::cout << std::left << std::setw(36) << "Wygenerowane (accepted)" << " : " << finalAccepted << "\n";
    std::cout << std::left << std::setw(36) << "Zapisane pliki pojedyncze" << " : " << finalWritten << "\n";
    std::cout << std::left << std::setw(36) << "Bledy zapisu plikow pojedynczych" << " : " << finalWriteErrors << "\n";
    std::cout << std::left << std::setw(36) << "Proby" << " : " << finalAttempts << "\n";
    std::cout << std::left << std::setw(36) << "Odrzucone po weryfikacji" << " : " << finalRejectedAtVerification << "\n";
    if (cfg.reseed_interval_seconds > 0) {
        std::cout << std::left << std::setw(36) << "Reset workera [s]" << " : " << cfg.reseed_interval_seconds << "\n";
        std::cout << std::left << std::setw(36) << "Lacznie resetow" << " : " << finalReseeds << "\n";
    }
    std::cout << std::left << std::setw(36) << "Czas [s]" << " : " << std::fixed << std::setprecision(2) << sec << "\n";
    if (sec > 0.0) {
        std::cout << std::left << std::setw(36) << "Plansze/min" << " : " << std::fixed << std::setprecision(2)
                  << (60.0 * static_cast<double>(finalWritten) / sec) << "\n";
        std::cout << std::left << std::setw(36) << "Plansze/godz" << " : " << std::fixed << std::setprecision(0)
                  << (3600.0 * static_cast<double>(finalWritten) / sec) << "\n";
    }

    if (cfg.reseed_interval_seconds > 0 && !finalResetCountsSnapshot.empty()) {
        std::cout << "\nResety per worker:\n";
        const std::size_t workerDigits = std::max<std::size_t>(2, std::to_string(finalResetCountsSnapshot.size()).size());
        std::cout << std::left
                  << std::setw(static_cast<int>(7 + workerDigits)) << "worker"
                  << " | " << std::setw(6) << "resets"
                  << " | " << std::setw(7) << "applied"
                  << " | " << std::setw(9) << "reset_lag"
                  << " | " << std::setw(8) << "lag_max" << "\n";
        std::cout << std::string(static_cast<std::size_t>(46 + workerDigits), '-') << "\n";
        for (std::size_t i = 0; i < finalResetCountsSnapshot.size(); ++i) {
            const unsigned long long pendingLag = (finalResetCountsSnapshot[i] >= finalAppliedResetCountsSnapshot[i])
                ? (finalResetCountsSnapshot[i] - finalAppliedResetCountsSnapshot[i])
                : 0ULL;
            std::cout << "worker_" << std::right << std::setw(static_cast<int>(workerDigits))
                      << std::setfill('0') << (i + 1) << std::setfill(' ') << std::left
                      << " | " << std::right << std::setw(6) << finalResetCountsSnapshot[i]
                      << " | " << std::setw(7) << finalAppliedResetCountsSnapshot[i]
                      << " | " << std::setw(9) << pendingLag
                      << " | " << std::setw(8) << finalResetLagMaxSnapshot[i]
                      << std::left << "\n";
        }
    }

    GenerateRunResult result;
    result.accepted = finalAccepted;
    result.written = finalWritten;
    result.attempts = finalAttempts;
    result.rejected_at_verification = finalRejectedAtVerification;
    result.elapsed_seconds = sec;

    if (finalAccepted < target) {
        std::cerr << "Uwaga: nie osiagnieto target_puzzles. Poluzuj kryteria lub zwieksz max_attempts.\n";
        result.return_code = 2;
    } else {
        result.return_code = 0;
    }
    return result;
}

int main(int argc, char* argv[]) {
    // CLI test mode: --test [count] [level] [box_rows] [box_cols]
    if (argc >= 2 && std::string(argv[1]) == "--test") {
        GenerateRunConfig cfg;
        cfg.target_puzzles = (argc >= 3) ? std::atoi(argv[2]) : 5;
        cfg.difficulty_required = (argc >= 4) ? std::atoi(argv[3]) : 1;
        cfg.box_rows = (argc >= 5) ? std::atoi(argv[4]) : 3;
        cfg.box_cols = (argc >= 6) ? std::atoi(argv[5]) : 3;
        const int N = cfg.box_rows * cfg.box_cols;
        const ClueRange cr = recommendedClueRangeForLevel(N, cfg.difficulty_required);
        cfg.min_clues = cr.min_clues;
        cfg.max_clues = cr.max_clues;
        cfg.output_folder = fs::path("test_output");
        cfg.output_file = fs::path("test_output") / "test_sudoku.txt";

        std::cout << "=== v2.0 CLI Test Mode ===\n"
                  << "Board: " << N << "x" << N << " (" << cfg.box_rows << "x" << cfg.box_cols << " blocks)\n"
                  << "Level: " << cfg.difficulty_required << "\n"
                  << "Target: " << cfg.target_puzzles << " puzzles\n"
                  << "Clues: " << cfg.min_clues << "-" << cfg.max_clues << "\n"
                  << std::flush;

        auto t0 = std::chrono::steady_clock::now();
        const auto res = runGenerateMode(cfg);
        auto t1 = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(t1 - t0).count();

        std::cout << "\nResult code: " << res.return_code << "\n"
                  << "Elapsed: " << std::fixed << std::setprecision(2) << elapsed << " s\n";

        if (fs::exists(cfg.output_file)) {
            std::ifstream in(cfg.output_file);
            std::string line;
            int count = 0;
            while (std::getline(in, line)) {
                if (!line.empty() && line[0] != '#') ++count;
            }
            std::cout << "Puzzles in output: " << count << "\n";
        }
        return res.return_code;
    }

    // Normal GUI mode
    if (FAILED(CoInitialize(nullptr))) {
        MessageBoxA(nullptr, "Nie udalo sie zainicjalizowac COM.", "Blad", MB_ICONERROR | MB_OK);
        return 1;
    }

    GenerateRunConfig cfg;
    try {
        if (!showGeneratorConfigWindow(cfg)) {
            CoUninitialize();
            return 0;
        }

        const auto res = runGenerateMode(cfg);
        const int rc = res.return_code;

        auto fmtRate = [&](std::ostringstream& os) {
            os << "\n\nWygenerowane: " << res.written
               << "  |  Proby: " << res.attempts
               << "  |  Odrzucone: " << res.rejected_at_verification
               << "\nCzas: " << std::fixed << std::setprecision(1) << res.elapsed_seconds << " s";
            if (res.elapsed_seconds > 0.0) {
                os << "  |  " << std::setprecision(0)
                   << (3600.0 * static_cast<double>(res.written) / res.elapsed_seconds) << " plansz/godz";
            }
        };

        std::ostringstream done;
        if (rc == 0) {
            done << "Generowanie zakonczone pomyslnie.\n\n"
                 << "Plik zbiorczy: " << cfg.output_file.string()
                 << "\nFolder plikow: " << cfg.output_folder.string();
            fmtRate(done);
            MessageBoxA(nullptr, done.str().c_str(), "Bulk Generator Sudoku", MB_OK);
        } else if (rc == 2) {
            done << "Generowanie zakonczone, ale nie osiagnieto docelowej liczby plansz.\n"
                 << "Sprawdz kryteria trudnosci/strategii lub zwieksz max_attempts.\n\nPlik: "
                 << cfg.output_file.string()
                 << "\nFolder plikow: " << cfg.output_folder.string();
            fmtRate(done);
            MessageBoxA(nullptr, done.str().c_str(), "Bulk Generator Sudoku", MB_ICONWARNING | MB_OK);
        } else {
            done << "Generator zakonczyl sie bledem (kod: " << rc << ").";
            MessageBoxA(nullptr, done.str().c_str(), "Bulk Generator Sudoku", MB_ICONERROR | MB_OK);
        }

        CoUninitialize();
        return rc;
    } catch (const std::exception& ex) {
        MessageBoxA(nullptr, ex.what(), "Blad krytyczny", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }
}
