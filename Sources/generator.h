#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <random>
#include <sstream>
#include <thread>
#include <optional>
#include <set>
#include <functional>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cctype>
#include <unordered_map>
#include <cstdlib>
#include <type_traits>
#include <cstring>
#include <limits>
#include <charconv> // <-- Dodano dla Zero-Allocation Serialization

// Forward declaration for regression tests used from parse_args
namespace sudoku_testy {
    void run_all_regression_tests(const std::string& report_path);
}

namespace sudoku_hpc {

struct GenericPuzzleCandidate {
    std::vector<uint16_t> puzzle;
    std::vector<uint16_t> solution;
    int clues = 0;
};

// ============================================================================
// OPTYMALIZACJA 1: ZERO-ALLOCATION SERIALIZATION
// Zamiast std::ostringstream używamy bezpośredniego zapisu do bufora (std::to_chars).
// ============================================================================
static std::string serialize_line_generic(
    long long seed,
    const GenerateRunConfig& cfg,
    const GenericPuzzleCandidate& candidate,
    int nn) {
    
    std::string out;
    // Prealokacja maksymalnego rozmiaru by uniknąć realokacji
    out.resize(128 + static_cast<size_t>(nn) * 4); 
    char* ptr = out.data();
    char* end = ptr + out.size(); // <--- TUTAJ USUNIĘTO 'const'

    auto res1 = std::to_chars(ptr, end, seed); ptr = res1.ptr;
    *ptr++ = ',';
    auto res2 = std::to_chars(ptr, end, cfg.box_rows); ptr = res2.ptr;
    *ptr++ = ',';
    auto res3 = std::to_chars(ptr, end, cfg.box_cols); ptr = res3.ptr;

    const uint16_t* puz_ptr = candidate.puzzle.data();
    const uint16_t* sol_ptr = candidate.solution.data();

    for (int i = 0; i < nn; ++i) {
        *ptr++ = ',';
        const uint16_t v = puz_ptr[i];
        if (v != 0) {
            *ptr++ = 't';
            auto res = std::to_chars(ptr, end, v); ptr = res.ptr;
        } else {
            auto res = std::to_chars(ptr, end, sol_ptr[i]); ptr = res.ptr;
        }
    }
    
    // Obcinamy ciąg znaków do faktycznie zużytej wielkości
    out.resize(ptr - out.data());
    return out;
}

inline long long bounded_positive_seed_i64(uint64_t raw_seed) {
    constexpr uint64_t kSigned64Max = static_cast<uint64_t>(std::numeric_limits<long long>::max());
    const uint64_t bounded = (raw_seed % kSigned64Max) + 1ULL;
    return static_cast<long long>(bounded);
}

inline long long random_seed_i64() {
    uint64_t seed_state = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    seed_state ^= (static_cast<uint64_t>(rd()) << 32);
    seed_state ^= static_cast<uint64_t>(rd());
    return bounded_positive_seed_i64(splitmix64(seed_state));
}

static int strategy_to_hist_idx_shared(RequiredStrategy strategy) {
    switch (strategy) {
    case RequiredStrategy::None: return 0;
    case RequiredStrategy::NakedSingle: return 1;
    case RequiredStrategy::HiddenSingle: return 2;
    case RequiredStrategy::Backtracking: return 9;
    default: return 0;
    }
}

template <typename T>
struct alignas(64) PaddedAtomic {
    std::atomic<T> value;

    constexpr PaddedAtomic() noexcept : value() {}
    constexpr explicit PaddedAtomic(T init) noexcept : value(init) {}

    PaddedAtomic(const PaddedAtomic&) = delete;
    PaddedAtomic& operator=(const PaddedAtomic&) = delete;

    T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return value.load(order);
    }

    void store(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
        value.store(v, order);
    }

    T exchange(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
        return value.exchange(v, order);
    }

    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U> && !std::is_same_v<U, bool>>>
    U fetch_add(U v, std::memory_order order = std::memory_order_seq_cst) noexcept {
        return value.fetch_add(v, order);
    }

    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U> && !std::is_same_v<U, bool>>>
    U fetch_sub(U v, std::memory_order order = std::memory_order_seq_cst) noexcept {
        return value.fetch_sub(v, order);
    }
};

struct TelemetryDelta {
    uint64_t attempts = 0;
    uint64_t rejected = 0;
    uint64_t reject_prefilter = 0;
    uint64_t reject_logic = 0;
    uint64_t reject_uniqueness = 0;
    uint64_t reject_strategy = 0;
    uint64_t reject_replay = 0;
    uint64_t reject_distribution_bias = 0;
    uint64_t reject_uniqueness_budget = 0;
    uint64_t analyzed_required = 0;
    uint64_t required_hits = 0;
    uint64_t uniqueness_calls = 0;
    uint64_t uniqueness_nodes = 0;
    uint64_t uniqueness_elapsed_ns = 0;
    uint64_t logic_steps = 0;
    uint64_t naked_use = 0;
    uint64_t naked_hit = 0;
    uint64_t hidden_use = 0;
    uint64_t hidden_hit = 0;
    uint64_t reseeds = 0;

    bool empty() const noexcept {
        return attempts == 0 && rejected == 0 && reject_prefilter == 0 &&
               reject_logic == 0 && reject_uniqueness == 0 && reject_strategy == 0 &&
               reject_replay == 0 && reject_distribution_bias == 0 &&
               reject_uniqueness_budget == 0 && analyzed_required == 0 &&
               required_hits == 0 && uniqueness_calls == 0 && uniqueness_nodes == 0 &&
               uniqueness_elapsed_ns == 0 && logic_steps == 0 && naked_use == 0 &&
               naked_hit == 0 && hidden_use == 0 && hidden_hit == 0 && reseeds == 0;
    }
};

template <size_t CapacityPow2 = 16384>
class alignas(64) TelemetryMpscRing {
    static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "CapacityPow2 must be power-of-two");

    struct alignas(64) Slot {
        std::atomic<uint64_t> seq{0};
        TelemetryDelta payload{};
    };

public:
    TelemetryMpscRing() {
        for (size_t i = 0; i < CapacityPow2; ++i) {
            slots_[i].seq.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }
    }

    bool try_push(const TelemetryDelta& delta) noexcept {
        uint64_t pos = head_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = slots_[static_cast<size_t>(pos & kMask)];
            const uint64_t seq = slot.seq.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1ULL, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    slot.payload = delta;
                    slot.seq.store(pos + 1ULL, std::memory_order_release);
                    return true;
                }
                continue;
            }
            if (diff < 0) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            pos = head_.load(std::memory_order_relaxed);
        }
    }

    bool try_pop(TelemetryDelta& out) noexcept {
        const uint64_t pos = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[static_cast<size_t>(pos & kMask)];
        const uint64_t seq = slot.seq.load(std::memory_order_acquire);
        if (seq != (pos + 1ULL)) {
            return false;
        }
        out = slot.payload;
        slot.seq.store(pos + CapacityPow2, std::memory_order_release);
        tail_.store(pos + 1ULL, std::memory_order_relaxed);
        return true;
    }

    uint64_t dropped() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint64_t kMask = static_cast<uint64_t>(CapacityPow2 - 1);
    std::array<Slot, CapacityPow2> slots_{};
    alignas(64) std::atomic<uint64_t> head_{0};
    alignas(64) std::atomic<uint64_t> tail_{0};
    alignas(64) std::atomic<uint64_t> dropped_{0};
};

struct OutputLineEvent {
    static constexpr size_t kMaxLineBytes = 8192;
    uint64_t accepted_idx = 0;
    uint32_t len = 0;
    std::array<char, kMaxLineBytes> bytes{};
};

template <size_t CapacityPow2 = 2048>
class alignas(64) OutputLineMpscRing {
    static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "CapacityPow2 must be power-of-two");

    struct alignas(64) Slot {
        std::atomic<uint64_t> seq{0};
        OutputLineEvent payload{};
    };

public:
    OutputLineMpscRing() {
        for (size_t i = 0; i < CapacityPow2; ++i) {
            slots_[i].seq.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }
    }

    bool try_push(uint64_t accepted_idx, const std::string& line) noexcept {
        if (line.size() > OutputLineEvent::kMaxLineBytes) {
            oversize_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        uint64_t pos = head_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = slots_[static_cast<size_t>(pos & kMask)];
            const uint64_t seq = slot.seq.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1ULL, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    slot.payload.accepted_idx = accepted_idx;
                    slot.payload.len = static_cast<uint32_t>(line.size());
                    std::memcpy(slot.payload.bytes.data(), line.data(), line.size());
                    slot.seq.store(pos + 1ULL, std::memory_order_release);
                    return true;
                }
                continue;
            }
            if (diff < 0) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            pos = head_.load(std::memory_order_relaxed);
        }
    }

    bool try_pop(OutputLineEvent& out) noexcept {
        const uint64_t pos = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[static_cast<size_t>(pos & kMask)];
        const uint64_t seq = slot.seq.load(std::memory_order_acquire);
        if (seq != (pos + 1ULL)) {
            return false;
        }
        out = slot.payload;
        slot.seq.store(pos + CapacityPow2, std::memory_order_release);
        tail_.store(pos + 1ULL, std::memory_order_relaxed);
        return true;
    }

    bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

    uint64_t dropped() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

    uint64_t oversize() const noexcept {
        return oversize_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint64_t kMask = static_cast<uint64_t>(CapacityPow2 - 1);
    std::array<Slot, CapacityPow2> slots_{};
    alignas(64) std::atomic<uint64_t> head_{0};
    alignas(64) std::atomic<uint64_t> tail_{0};
    alignas(64) std::atomic<uint64_t> dropped_{0};
    alignas(64) std::atomic<uint64_t> oversize_{0};
};

// ============================================================================
// OPTYMALIZACJA 4: EXTREME LOW-LATENCY THREAD POOL (Zero shared_ptr, użyto atomic::wait)
// ============================================================================
class PersistentThreadPool {
public:
    static PersistentThreadPool& instance() {
        static PersistentThreadPool pool;
        return pool;
    }

    void run(int task_count, const std::function<void(int)>& fn) {
        if (task_count <= 0) {
            return;
        }
        std::lock_guard<std::mutex> run_guard(run_mu_);
        ensure_workers(task_count);

        job_fn_ = &fn;
        task_count_.store(task_count, std::memory_order_relaxed);
        next_task_.store(0, std::memory_order_relaxed);
        remaining_.store(task_count, std::memory_order_release);
        
        // Zbudź workery korzystając z szybkiego mechanizmu w C++20
        epoch_.fetch_add(1, std::memory_order_acq_rel);
        epoch_.notify_all();

        // Czekaj bez aktywnego spinowania aż ostatni worker zakończy zadanie.
        while (true) {
            const int rem = remaining_.load(std::memory_order_acquire);
            if (rem <= 0) break;
            remaining_.wait(rem, std::memory_order_acquire);
        }
    }

private:
    PersistentThreadPool() = default;

    ~PersistentThreadPool() {
        stop_.store(true, std::memory_order_release);
        epoch_.fetch_add(1, std::memory_order_acq_rel);
        epoch_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void ensure_workers(int min_workers) {
        const int base = std::max(1u, std::thread::hardware_concurrency());
        const int target = std::max(base, min_workers);
        if (static_cast<int>(workers_.size()) >= target) {
            return;
        }
        workers_.reserve(static_cast<size_t>(target));
        for (int i = static_cast<int>(workers_.size()); i < target; ++i) {
            workers_.emplace_back([this]() { worker_loop(); });
        }
    }

    void worker_loop() {
        // Start od 0 zabezpiecza przed "missed wake-up" dla workerów,
        // które wystartują już po podbiciu epoch przez pierwszy run().
        uint64_t seen_epoch = 0;
        while (true) {
            if (stop_.load(std::memory_order_acquire)) return;
            
            epoch_.wait(seen_epoch, std::memory_order_acquire);
            seen_epoch = epoch_.load(std::memory_order_acquire);
            
            if (stop_.load(std::memory_order_acquire)) return;

            while (true) {
                const int idx = next_task_.fetch_add(1, std::memory_order_relaxed);
                if (idx >= task_count_.load(std::memory_order_relaxed)) {
                    break;
                }
                
                (*job_fn_)(idx);
                
                if (remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    remaining_.notify_one();
                }
            }
        }
    }

    std::mutex run_mu_;
    std::vector<std::thread> workers_;
    
    const std::function<void(int)>* job_fn_{nullptr};
    alignas(64) std::atomic<int> task_count_{0};
    alignas(64) std::atomic<int> next_task_{0};
    alignas(64) std::atomic<int> remaining_{0};
    
    alignas(64) std::atomic<uint64_t> epoch_{0};
    alignas(64) std::atomic<bool> stop_{false};
};

struct ClueRange {
    int min_clues = 0;
    int max_clues = 0;
};

enum class GenerationMode {
    Full,
    Lite,
    TopologyOnly
};

inline const char* generation_mode_name(GenerationMode mode) {
    switch (mode) {
    case GenerationMode::Full: return "full";
    case GenerationMode::Lite: return "lite";
    case GenerationMode::TopologyOnly: return "topology_only";
    default: return "unknown";
    }
}

struct GenerationProfile {
    GenerationMode mode = GenerationMode::Full;
    ClueRange clue_range{};
    double suggested_budget_s = 0.0;
    double asymmetry_ratio = 1.0;
    bool is_symmetric = true;
    std::string reason;
};

inline double asymmetry_ratio_for_geometry(int box_rows, int box_cols) {
    const int safe_box_rows = std::max(1, box_rows);
    const int safe_box_cols = std::max(1, box_cols);
    return static_cast<double>(std::max(safe_box_rows, safe_box_cols)) /
           static_cast<double>(std::min(safe_box_rows, safe_box_cols));
}

GenerationProfile resolve_generation_profile(
    int box_rows,
    int box_cols,
    int level,
    RequiredStrategy strategy,
    const GenerateRunConfig* cfg_override = nullptr);

inline std::string normalize_profile_mode_policy(std::string policy_raw) {
    std::string key;
    key.reserve(policy_raw.size());
    for (unsigned char ch : policy_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "full") return "full";
    if (key == "legacy") return "legacy";
    return "adaptive";
}

inline std::string normalize_cpu_backend_policy(std::string policy_raw) {
    std::string key;
    key.reserve(policy_raw.size());
    for (unsigned char ch : policy_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "scalar") return "scalar";
    if (key == "avx2") return "avx2";
    if (key == "avx512" || key == "avx512f") return "avx512";
    return "auto";
}

inline std::string normalize_difficulty_engine(std::string engine_raw) {
    std::string key;
    key.reserve(engine_raw.size());
    for (unsigned char ch : engine_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "vip") return "vip";
    return "standard";
}

inline std::string normalize_asym_heuristics_mode(std::string mode_raw) {
    std::string key;
    key.reserve(mode_raw.size());
    for (unsigned char ch : mode_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "off" || key == "none") return "off";
    if (key == "aggressive" || key == "high") return "aggressive";
    return "balanced";
}

inline std::string normalize_vip_score_profile(std::string profile_raw) {
    std::string key;
    key.reserve(profile_raw.size());
    for (unsigned char ch : profile_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "strict") return "strict";
    if (key == "ultra") return "ultra";
    return "standard";
}

inline std::string normalize_vip_trace_level(std::string level_raw) {
    std::string key;
    key.reserve(level_raw.size());
    for (unsigned char ch : level_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "full" || key == "verbose") return "full";
    return "basic";
}

inline std::string normalize_vip_grade_target(std::string grade_raw) {
    std::string key;
    key.reserve(grade_raw.size());
    for (unsigned char ch : grade_raw) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key == "bronze") return "bronze";
    if (key == "silver") return "silver";
    if (key == "platinum") return "platinum";
    return "gold";
}

inline int vip_grade_rank(std::string grade) {
    const std::string g = normalize_vip_grade_target(std::move(grade));
    if (g == "bronze") return 1;
    if (g == "silver") return 2;
    if (g == "gold") return 3;
    if (g == "platinum") return 4;
    return 0;
}

struct RuntimeCpuContext {
    std::string requested_policy = "auto";
    std::string selected_backend = "scalar";
    bool avx2_supported = false;
    bool avx512_supported = false;
    std::string reason;
};

inline bool cpu_supports_avx2() {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

inline bool cpu_supports_avx512() {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    return __builtin_cpu_supports("avx512f");
#else
    return false;
#endif
}

inline RuntimeCpuContext resolve_runtime_cpu_context(const GenerateRunConfig& cfg) {
    RuntimeCpuContext ctx{};
    ctx.requested_policy = normalize_cpu_backend_policy(cfg.cpu_backend_policy);
    ctx.avx2_supported = cpu_supports_avx2();
    ctx.avx512_supported = cpu_supports_avx512();

    if (ctx.requested_policy == "scalar") {
        ctx.selected_backend = "scalar";
        ctx.reason = "policy=scalar";
        return ctx;
    }
    if (ctx.requested_policy == "avx2") {
        if (ctx.avx2_supported) {
            ctx.selected_backend = "avx2";
            ctx.reason = "policy=avx2_supported";
        } else {
            ctx.selected_backend = "scalar";
            ctx.reason = "policy=avx2_unsupported_fallback_scalar";
        }
        return ctx;
    }
    if (ctx.requested_policy == "avx512") {
        if (ctx.avx512_supported) {
            ctx.selected_backend = "avx512";
            ctx.reason = "policy=avx512_supported";
        } else if (ctx.avx2_supported) {
            ctx.selected_backend = "avx2";
            ctx.reason = "policy=avx512_unsupported_fallback_avx2";
        } else {
            ctx.selected_backend = "scalar";
            ctx.reason = "policy=avx512_unsupported_fallback_scalar";
        }
        return ctx;
    }

    if (ctx.avx512_supported) {
        ctx.selected_backend = "avx512";
        ctx.reason = "auto_pick_avx512";
    } else if (ctx.avx2_supported) {
        ctx.selected_backend = "avx2";
        ctx.reason = "auto_pick_avx2";
    } else {
        ctx.selected_backend = "scalar";
        ctx.reason = "auto_pick_scalar";
    }
    return ctx;
}

struct QualityMetrics {
    int clues = 0;
    int row_min = 0;
    int row_max = 0;
    int col_min = 0;
    int col_max = 0;
    int box_min = 0;
    int box_max = 0;
    int digit_min = 0;
    int digit_max = 0;
    double normalized_entropy = 0.0;
    double entropy_threshold = 0.0;
    bool symmetry_ok = true;
    bool distribution_balance_ok = true;
};

struct QualityContract {
    bool is_unique = true;
    bool logic_replay_ok = true;
    bool clue_range_ok = true;
    bool symmetry_ok = true;
    bool givens_entropy_ok = true;
    bool distribution_balance_ok = true;
    std::string generation_mode;
};

struct ReplayValidationResult {
    bool ok = false;
    bool solved = false;
    uint64_t puzzle_hash = 0;
    uint64_t expected_solution_hash = 0;
    uint64_t replay_solution_hash = 0;
    uint64_t trace_hash = 0;
};

// ============================================================================
// OPTYMALIZACJA 4: WEKTORYZACJA HASHOWANIA
// Procesor trawi pamięć paczkami 64-bitowymi zamiast bajtami.
// ============================================================================
inline uint64_t fnv1a64_bytes(const void* data, size_t len, uint64_t seed = 1469598103934665603ULL) {
    uint64_t h = seed;
    const size_t blocks = len / 8;
    const size_t remainder = len % 8;
    const uint64_t* ptr64 = static_cast<const uint64_t*>(data);
    
    // Pętla odwijana i pipelinowana przez kompilator dla 64-bitowych słów
    for (size_t i = 0; i < blocks; ++i) {
        h ^= ptr64[i];
        h *= 1099511628211ULL;
    }
    
    // Ewentualna reszta (max 7 bajtów)
    if (remainder > 0) {
        const uint8_t* ptr8 = reinterpret_cast<const uint8_t*>(ptr64 + blocks);
        for (size_t i = 0; i < remainder; ++i) {
            h ^= static_cast<uint64_t>(ptr8[i]);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

inline uint64_t hash_u16_vector(const std::vector<uint16_t>& data, uint64_t seed = 1469598103934665603ULL) {
    if (data.empty()) return seed;
    return fnv1a64_bytes(data.data(), data.size() * sizeof(uint16_t), seed);
}

inline bool check_center_symmetry_givens(const std::vector<uint16_t>& puzzle, const GenericTopology& topo) {
    const auto& sym = topo.cell_center_sym;
    const uint16_t* const puzzle_ptr = puzzle.data();
    for (int idx = 0; idx < topo.nn; ++idx) {
        const int sym_idx = sym[static_cast<size_t>(idx)];
        if (idx > sym_idx) continue;
        
        const bool given_a = puzzle_ptr[static_cast<size_t>(idx)] != 0;
        const bool given_b = puzzle_ptr[static_cast<size_t>(sym_idx)] != 0;
        if (given_a != given_b) {
            return false;
        }
    }
    return true;
}

inline double entropy_threshold_for_n(int n) {
    if (n <= 12) return 0.40;
    if (n <= 24) return 0.55;
    return 0.65;
}

// ============================================================================
// OPTYMALIZACJA 2: BŁYSKAWICZNA ENTROPIA
// Tworzymy Look-Up Table (LUT) przy starcie, aby ominąć ciężkie std::log2 w pętli.
// ============================================================================
inline const std::array<double, 82>& get_c_log2_c_lut() {
    static const std::array<double, 82> lut = []() {
        std::array<double, 82> arr{};
        arr[0] = 0.0;
        for (int i = 1; i <= 81; ++i) {
            arr[i] = static_cast<double>(i) * std::log2(static_cast<double>(i));
        }
        return arr;
    }();
    return lut;
}

inline QualityMetrics evaluate_quality_metrics(
    const std::vector<uint16_t>& puzzle,
    const GenericTopology& topo,
    const GenerateRunConfig& cfg) {
    QualityMetrics m{};
    GenericThreadScratch& scratch = generic_tls_for(topo);
    std::vector<int>& row_counts = scratch.row_count_tmp;
    std::vector<int>& col_counts = scratch.col_count_tmp;
    std::vector<int>& box_counts = scratch.box_count_tmp;
    std::vector<int>& digit_counts = scratch.digit_count_tmp;
    const auto& rcb = topo.cell_rcb_packed;
    std::fill(row_counts.begin(), row_counts.end(), 0);
    std::fill(col_counts.begin(), col_counts.end(), 0);
    std::fill(box_counts.begin(), box_counts.end(), 0);
    std::fill(digit_counts.begin(), digit_counts.end(), 0);
    int* const row_ptr = row_counts.data();
    int* const col_ptr = col_counts.data();
    int* const box_ptr = box_counts.data();
    int* const digit_ptr = digit_counts.data();
    const uint32_t* const packed = rcb.data();
    const uint16_t* const puzzle_ptr = puzzle.data();

    for (int idx = 0; idx < topo.nn; ++idx) {
        const int v = static_cast<int>(puzzle_ptr[static_cast<size_t>(idx)]);
        if (v <= 0) continue;
        
        ++m.clues;
        const uint32_t p = packed[static_cast<size_t>(idx)];
        const int r = GenericBoard::packed_row(p);
        const int c = GenericBoard::packed_col(p);
        const int b = GenericBoard::packed_box(p);
        ++row_ptr[static_cast<size_t>(r)];
        ++col_ptr[static_cast<size_t>(c)];
        ++box_ptr[static_cast<size_t>(b)];
        if (v <= topo.n) {
            ++digit_ptr[static_cast<size_t>(v - 1)];
        }
    }

    auto minmax_counts = [n = topo.n](const std::vector<int>& counts) -> std::pair<int, int> {
        if (n <= 0) return {0, 0};
        int mn = counts[0];
        int mx = counts[0];
        for (int i = 1; i < n; ++i) {
            const int x = counts[static_cast<size_t>(i)];
            mn = std::min(mn, x);
            mx = std::max(mx, x);
        }
        return {mn, mx};
    };

    const auto [row_min, row_max] = minmax_counts(row_counts);
    const auto [col_min, col_max] = minmax_counts(col_counts);
    const auto [box_min, box_max] = minmax_counts(box_counts);
    const auto [digit_min, digit_max] = minmax_counts(digit_counts);
    m.row_min = row_min;
    m.row_max = row_max;
    m.col_min = col_min;
    m.col_max = col_max;
    m.box_min = box_min;
    m.box_max = box_max;
    m.digit_min = digit_min;
    m.digit_max = digit_max;

    double entropy = 0.0;
    if (m.clues > 0) {
        double sum_c_log2_c = 0.0;
        const auto& lut = get_c_log2_c_lut();
        for (int c : digit_counts) {
            if (c > 0 && c <= 81) { // Chroni przed wyjściem poza LUT w skrajnych siatkach
                sum_c_log2_c += lut[static_cast<size_t>(c)];
            }
        }
        const double T = static_cast<double>(m.clues);
        entropy = std::log2(T) - (sum_c_log2_c / T);
        
        const double max_entropy = std::log2(static_cast<double>(topo.n));
        m.normalized_entropy = max_entropy > 0.0 ? (entropy / max_entropy) : 0.0;
    }
    
    m.entropy_threshold = entropy_threshold_for_n(topo.n);
    m.symmetry_ok = (!cfg.symmetry_center) || check_center_symmetry_givens(puzzle, topo);

    const double ideal = topo.n > 0 ? static_cast<double>(m.clues) / static_cast<double>(topo.n) : 0.0;
    const double allowed_dev = std::max(2.0, ideal * 0.80);
    const auto within_dev = [ideal, allowed_dev](int x) {
        return std::abs(static_cast<double>(x) - ideal) <= (allowed_dev + 1.0);
    };
    const bool rows_ok = within_dev(m.row_min) && within_dev(m.row_max);
    const bool cols_ok = within_dev(m.col_min) && within_dev(m.col_max);
    const bool boxes_ok = within_dev(m.box_min) && within_dev(m.box_max);
    const bool digits_ok = within_dev(m.digit_min) && within_dev(m.digit_max);
    m.distribution_balance_ok = rows_ok && cols_ok && boxes_ok && digits_ok;
    return m;
}

inline ReplayValidationResult run_replay_validation(
    const std::vector<uint16_t>& puzzle,
    const std::vector<uint16_t>& expected_solution,
    const GenericTopology& topo,
    const GenericLogicCertify& logic) {
    ReplayValidationResult out{};
    out.puzzle_hash = hash_u16_vector(puzzle);
    out.expected_solution_hash = hash_u16_vector(expected_solution);
    const GenericLogicCertifyResult replay = logic.certify(puzzle, topo, nullptr, true);
    out.solved = replay.solved;
    out.replay_solution_hash = hash_u16_vector(replay.solved_grid);

    uint64_t trace_seed = 1469598103934665603ULL;
    trace_seed = fnv1a64_bytes(&replay.steps, sizeof(replay.steps), trace_seed);
    for (const auto& st : replay.strategy_stats) {
        trace_seed = fnv1a64_bytes(&st.use_count, sizeof(st.use_count), trace_seed);
        trace_seed = fnv1a64_bytes(&st.hit_count, sizeof(st.hit_count), trace_seed);
        trace_seed = fnv1a64_bytes(&st.placements, sizeof(st.placements), trace_seed);
    }
    out.trace_hash = trace_seed;
    out.ok = replay.solved && replay.solved_grid == expected_solution;
    return out;
}

inline bool quality_contract_passed(const QualityContract& c, const GenerateRunConfig& cfg) {
    if (!cfg.enable_quality_contract) return true;
    if (!c.is_unique) return false;
    if (!c.clue_range_ok || !c.symmetry_ok) return false;
    if (!c.givens_entropy_ok) return false;
    if (cfg.enable_distribution_filter && !c.distribution_balance_ok) return false;
    if (cfg.enable_replay_validation && !c.logic_replay_ok) return false;
    return true;
}

static bool evaluate_required_strategy_contract_generic(
    const GenericLogicCertifyResult& logic_result,
    RequiredStrategy required,
    RequiredStrategyAttemptInfo& strategy_info) {
    strategy_info = {};
    if (required == RequiredStrategy::None) {
        return true;
    }
    switch (required) {
    case RequiredStrategy::NakedSingle:
        strategy_info.required_strategy_use_confirmed = logic_result.strategy_stats[0].use_count > 0;
        strategy_info.required_strategy_hit_confirmed = logic_result.strategy_stats[0].hit_count > 0;
        break;
    case RequiredStrategy::HiddenSingle:
        strategy_info.required_strategy_use_confirmed = logic_result.strategy_stats[1].use_count > 0;
        strategy_info.required_strategy_hit_confirmed = logic_result.strategy_stats[1].hit_count > 0;
        break;
    case RequiredStrategy::Backtracking:
        strategy_info.required_strategy_use_confirmed = !logic_result.solved;
        strategy_info.required_strategy_hit_confirmed = !logic_result.solved;
        break;
    case RequiredStrategy::None:
    default:
        break;
    }
    strategy_info.analyzed_required_strategy = strategy_info.required_strategy_use_confirmed;
    strategy_info.matched_required_strategy = strategy_info.required_strategy_use_confirmed && strategy_info.required_strategy_hit_confirmed;
    return strategy_info.matched_required_strategy;
}

struct AttemptPerfStats {
    uint64_t solved_elapsed_ns = 0;
    uint64_t dig_elapsed_ns = 0;
    uint64_t prefilter_elapsed_ns = 0;
    uint64_t logic_elapsed_ns = 0;
    uint64_t uniqueness_calls = 0;
    uint64_t uniqueness_nodes = 0;
    uint64_t uniqueness_elapsed_ns = 0;
    uint64_t logic_steps = 0;
    uint64_t strategy_naked_use = 0;
    uint64_t strategy_naked_hit = 0;
    uint64_t strategy_hidden_use = 0;
    uint64_t strategy_hidden_hit = 0;
};

static bool generate_one_generic(
    const GenerateRunConfig& cfg,
    const GenericTopology& topo,
    std::mt19937_64& rng,
    GenericPuzzleCandidate& candidate,
    RejectReason& reason,
    RequiredStrategyAttemptInfo& strategy_info,
    const GenericSolvedKernel& solved,
    const GenericDigKernel& dig,
    const GenericQuickPrefilter& prefilter,
    const GenericLogicCertify& logic,
    const GenericUniquenessCounter& uniq,
    const std::atomic<bool>* force_abort_ptr = nullptr,
    bool* timed_out = nullptr,
    const std::atomic<bool>* external_cancel_ptr = nullptr,
    const std::atomic<bool>* external_pause_ptr = nullptr,
    QualityContract* quality_contract_out = nullptr,
    QualityMetrics* quality_metrics_out = nullptr,
    ReplayValidationResult* replay_out = nullptr,
    AttemptPerfStats* perf_out = nullptr) {
    
    const bool has_timed_out_ptr = (timed_out != nullptr);
    const bool has_quality_contract_out = (quality_contract_out != nullptr);
    const bool has_quality_metrics_out = (quality_metrics_out != nullptr);
    const bool has_replay_out = (replay_out != nullptr);
    const bool collect_perf = (perf_out != nullptr);
    
    if (has_timed_out_ptr) *timed_out = false;
    strategy_info = {};
    if (has_quality_contract_out) {
        *quality_contract_out = {};
        const GenerationProfile profile =
            resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
        quality_contract_out->generation_mode = generation_mode_name(profile.mode);
    }
    if (has_quality_metrics_out) *quality_metrics_out = {};
    if (has_replay_out) *replay_out = {};
    if (collect_perf) *perf_out = {};
    
    const bool quality_contract_enabled = cfg.enable_quality_contract;
    const bool distribution_filter_enabled = quality_contract_enabled && cfg.enable_distribution_filter;
    const bool replay_validation_enabled = quality_contract_enabled && cfg.enable_replay_validation;
    const bool need_quality_metrics = quality_contract_enabled || quality_contract_out != nullptr || quality_metrics_out != nullptr;
    const bool budget_enabled = cfg.attempt_time_budget_s > 0.0 || cfg.attempt_node_budget > 0 || force_abort_ptr != nullptr;
    
    SearchAbortControl budget;
    if (cfg.attempt_time_budget_s > 0.0) {
        budget.time_enabled = true;
        budget.deadline = std::chrono::steady_clock::now() + 
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(cfg.attempt_time_budget_s));
    }
    if (cfg.attempt_node_budget > 0) {
        budget.node_enabled = true;
        budget.node_limit = cfg.attempt_node_budget;
    }
    if (force_abort_ptr != nullptr) budget.force_abort_ptr = force_abort_ptr;
    budget.cancel_ptr = external_cancel_ptr;
    budget.pause_ptr = external_pause_ptr;
    
    SearchAbortControl* budget_ptr = budget_enabled ? &budget : nullptr;

    const auto solved_t0 = std::chrono::steady_clock::now();
    const bool solved_ok = solved.generate(topo, rng, candidate.solution, budget_ptr);
    if (collect_perf) {
        perf_out->solved_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - solved_t0)
                .count());
    }
    if (!solved_ok) {
        if (budget_ptr != nullptr && budget_ptr->aborted()) {
            if (budget_ptr->aborted_by_pause) {
                reason = RejectReason::None;
                return false;
            }
            if (has_timed_out_ptr) {
                *timed_out = budget_ptr->aborted_by_time || budget_ptr->aborted_by_nodes;
            }
        }
        reason = RejectReason::Logic;
        return false;
    }
    
    const auto dig_t0 = std::chrono::steady_clock::now();
    dig.dig_into(candidate.solution, topo, cfg, rng, candidate.puzzle, candidate.clues);
    if (collect_perf) {
        perf_out->dig_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - dig_t0)
                .count());
    }

    const auto prefilter_t0 = std::chrono::steady_clock::now();
    const bool prefilter_ok = prefilter.check(candidate.puzzle, topo, cfg.min_clues, cfg.max_clues);
    if (collect_perf) {
        perf_out->prefilter_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - prefilter_t0)
                .count());
    }
    if (!prefilter_ok) {
        reason = RejectReason::Prefilter;
        return false;
    }
    
    QualityMetrics quality_metrics{};
    if (need_quality_metrics) {
        quality_metrics = evaluate_quality_metrics(candidate.puzzle, topo, cfg);
        if (has_quality_metrics_out) *quality_metrics_out = quality_metrics;
        if (has_quality_contract_out) {
            quality_contract_out->clue_range_ok = (candidate.clues >= cfg.min_clues && candidate.clues <= cfg.max_clues);
            quality_contract_out->symmetry_ok = quality_metrics.symmetry_ok;
            quality_contract_out->distribution_balance_ok = quality_metrics.distribution_balance_ok;
            quality_contract_out->givens_entropy_ok = quality_metrics.normalized_entropy >= quality_metrics.entropy_threshold;
        }

        if (quality_contract_enabled) {
            if (!quality_metrics.symmetry_ok) {
                reason = RejectReason::DistributionBias;
                return false;
            }
            if (distribution_filter_enabled) {
                if (!(quality_metrics.normalized_entropy >= quality_metrics.entropy_threshold) || 
                    !quality_metrics.distribution_balance_ok) {
                    reason = RejectReason::DistributionBias;
                    return false;
                }
            }
        }
    }
    
    const bool capture_logic_solution = replay_validation_enabled;
    const auto logic_t0 = std::chrono::steady_clock::now();
    const GenericLogicCertifyResult logic_result = logic.certify(candidate.puzzle, topo, budget_ptr, capture_logic_solution);
    if (collect_perf) {
        perf_out->logic_elapsed_ns += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - logic_t0)
                .count());
    }
    if (logic_result.timed_out) {
        if (budget_ptr != nullptr && budget_ptr->aborted_by_pause) {
            reason = RejectReason::None;
            return false;
        }
        if (has_timed_out_ptr) {
            *timed_out = (budget_ptr == nullptr) ? true : (budget_ptr->aborted_by_time || budget_ptr->aborted_by_nodes);
        }
        reason = RejectReason::Logic;
        return false;
    }
    
    if (collect_perf) {
        perf_out->logic_steps = static_cast<uint64_t>(std::max(0, logic_result.steps));
        perf_out->strategy_naked_use = logic_result.strategy_stats[0].use_count;
        perf_out->strategy_naked_hit = logic_result.strategy_stats[0].hit_count;
        perf_out->strategy_hidden_use = logic_result.strategy_stats[1].use_count;
        perf_out->strategy_hidden_hit = logic_result.strategy_stats[1].hit_count;
    }
    
    const bool contract_ok = evaluate_required_strategy_contract_generic(logic_result, cfg.required_strategy, strategy_info);
    if (cfg.required_strategy != RequiredStrategy::None && !contract_ok) {
        reason = RejectReason::Strategy;
        return false;
    }
    if (cfg.strict_logical && !logic_result.solved && cfg.required_strategy != RequiredStrategy::Backtracking) {
        reason = RejectReason::Logic;
        return false;
    }
    
    bool uniqueness_ok = true;
    if (cfg.require_unique) {
        auto record_uniqueness_perf = [&](const SearchAbortControl& b, uint64_t elapsed_ns) {
            if (!collect_perf) return;
            ++perf_out->uniqueness_calls;
            perf_out->uniqueness_nodes += b.nodes;
            perf_out->uniqueness_elapsed_ns += elapsed_ns;
        };
        SearchAbortControl uniq_budget = budget;
        SearchAbortControl* uniq_budget_ptr = budget_enabled ? &uniq_budget : nullptr;
        
        const auto uniq_t0 = std::chrono::steady_clock::now();
        const int solutions = uniq.count_solutions_limit2(candidate.puzzle, topo, uniq_budget_ptr);
        const auto uniq_elapsed_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - uniq_t0).count());
        record_uniqueness_perf(uniq_budget, uniq_elapsed_ns);
        
        if (solutions < 0) {
            if (uniq_budget_ptr != nullptr && uniq_budget_ptr->aborted_by_pause) {
                reason = RejectReason::None;
                return false;
            }
            if (has_timed_out_ptr) *timed_out = true;
            reason = RejectReason::Logic;
            return false;
        }
        if (solutions != 1) {
            reason = RejectReason::Uniqueness;
            return false;
        }
        
        if (quality_contract_enabled && topo.n >= 25) {
            SearchAbortControl confirm_budget;
            if (cfg.uniqueness_confirm_budget_s > 0.0) {
                confirm_budget.time_enabled = true;
                confirm_budget.deadline = std::chrono::steady_clock::now() + 
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(cfg.uniqueness_confirm_budget_s));
            }
            if (cfg.uniqueness_confirm_budget_nodes > 0) {
                confirm_budget.node_enabled = true;
                confirm_budget.node_limit = cfg.uniqueness_confirm_budget_nodes;
            }
            confirm_budget.force_abort_ptr = force_abort_ptr;
            confirm_budget.cancel_ptr = external_cancel_ptr;
            confirm_budget.pause_ptr = external_pause_ptr;

            const auto confirm_t0 = std::chrono::steady_clock::now();
            const int confirm_solutions = uniq.count_solutions_limit(candidate.puzzle, topo, 3, &confirm_budget);
            const auto confirm_elapsed_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - confirm_t0).count());
            record_uniqueness_perf(confirm_budget, confirm_elapsed_ns);
            
            if (confirm_solutions < 0) {
                if (confirm_budget.aborted_by_pause) {
                    reason = RejectReason::None;
                    return false;
                }
                reason = RejectReason::UniquenessBudget;
                return false;
            }
            if (confirm_solutions != 1) {
                reason = RejectReason::Uniqueness;
                return false;
            }
        }
    }

    ReplayValidationResult replay{};
    if (replay_validation_enabled) {
        replay.solved = logic_result.solved;
        replay.ok = replay.solved && logic_result.solved_grid == candidate.solution;
        if (has_replay_out || !replay.ok) {
            replay.puzzle_hash = hash_u16_vector(candidate.puzzle);
            replay.expected_solution_hash = hash_u16_vector(candidate.solution);
            replay.replay_solution_hash = hash_u16_vector(logic_result.solved_grid);
            uint64_t trace_seed = 1469598103934665603ULL;
            trace_seed = fnv1a64_bytes(&logic_result.steps, sizeof(logic_result.steps), trace_seed);
            // Optymalizacja haszowania strategii (unikanie pętli po elementach gdy to możliwe)
            trace_seed = fnv1a64_bytes(logic_result.strategy_stats.data(), logic_result.strategy_stats.size() * sizeof(StrategyStats), trace_seed);
            replay.trace_hash = trace_seed;
        }
        if (has_replay_out) *replay_out = replay;
        if (!replay.ok) {
            reason = RejectReason::Replay;
            return false;
        }
    }
    if (has_quality_contract_out) {
        quality_contract_out->is_unique = uniqueness_ok;
        quality_contract_out->logic_replay_ok = replay.ok || !replay_validation_enabled;
    }
    if (has_quality_contract_out && !quality_contract_passed(*quality_contract_out, cfg)) {
        reason = RejectReason::DistributionBias;
        return false;
    }
    
    reason = RejectReason::None;
    return true;
}

struct DifficultySignals {
    uint64_t attempts = 0;
    uint64_t accepted = 0;
    uint64_t rejected = 0;
    uint64_t logic_passes = 0;
    uint64_t candidate_eliminations = 0;
    uint64_t branching_proxy = 0;
    uint64_t max_depth_proxy = 0;
    uint64_t uniqueness_nodes = 0;
    double uniqueness_cost_ms = 0.0;
    double elapsed_s = 0.0;
    std::string mode;
};

inline DifficultySignals collect_difficulty_signals(const GenerateRunResult& result, const GenerationProfile& profile) {
    DifficultySignals s{};
    s.attempts = result.attempts;
    s.accepted = result.accepted;
    s.rejected = result.rejected;
    s.logic_passes = result.analyzed_required_strategy;
    s.candidate_eliminations = result.required_strategy_hits;
    s.branching_proxy = result.reject_logic + result.reject_uniqueness + result.reject_strategy;
    s.uniqueness_nodes = result.uniqueness_nodes;
    s.uniqueness_cost_ms = result.uniqueness_elapsed_ms;
    s.elapsed_s = result.elapsed_s;
    s.mode = generation_mode_name(profile.mode);
    return s;
}

inline double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

inline std::string vip_grade_from_score(double score) {
    if (score >= 700.0) return "platinum";
    if (score >= 500.0) return "gold";
    if (score >= 300.0) return "silver";
    if (score > 0.0) return "bronze";
    return "none";
}

struct VipScoreBreakdown {
    double logic_depth_norm = 0.0;
    double hidden_norm = 0.0;
    double naked_norm = 0.0;
    double uniqueness_norm = 0.0;
    double branching_norm = 0.0;
    double level_norm = 0.0;
    double weighted = 0.0;
    double asym_multiplier = 1.0;
    double final_score = 0.0;
    std::string profile = "standard";

    std::string to_json() const {
        std::ostringstream out;
        out << "{\"profile\":\"" << profile << "\","
            << "\"logic_depth_norm\":" << format_fixed(logic_depth_norm, 6) << ","
            << "\"hidden_norm\":" << format_fixed(hidden_norm, 6) << ","
            << "\"naked_norm\":" << format_fixed(naked_norm, 6) << ","
            << "\"uniqueness_norm\":" << format_fixed(uniqueness_norm, 6) << ","
            << "\"branching_norm\":" << format_fixed(branching_norm, 6) << ","
            << "\"level_norm\":" << format_fixed(level_norm, 6) << ","
            << "\"weighted\":" << format_fixed(weighted, 6) << ","
            << "\"asym_multiplier\":" << format_fixed(asym_multiplier, 6) << ","
            << "\"final_score\":" << format_fixed(final_score, 6) << "}";
        return out.str();
    }
};

inline std::string geometry_key_for_grade_target(int box_rows, int box_cols) {
    return std::to_string(box_rows) + "x" + std::to_string(box_cols);
}

inline std::unordered_map<std::string, std::string> load_vip_grade_target_overrides(const std::string& path_raw) {
    std::unordered_map<std::string, std::string> out_map;
    if (path_raw.empty()) return out_map;
    std::ifstream in(path_raw);
    if (!in) return out_map;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::replace(line.begin(), line.end(), ';', ',');
        std::istringstream iss(line);
        std::string n_s, br_s, bc_s, grade_s;
        if (std::getline(iss, n_s, ',') && std::getline(iss, br_s, ',') && 
            std::getline(iss, bc_s, ',') && std::getline(iss, grade_s, ',')) {
            try {
                out_map[geometry_key_for_grade_target(std::stoi(br_s), std::stoi(bc_s))] = normalize_vip_grade_target(grade_s);
            } catch (...) {}
        }
    }
    return out_map;
}

inline std::string resolve_vip_grade_target_for_geometry(const GenerateRunConfig& cfg) {
    const std::string default_grade = normalize_vip_grade_target(cfg.vip_grade_target);
    if (cfg.vip_min_grade_by_geometry_path.empty()) return default_grade;
    const auto overrides = load_vip_grade_target_overrides(cfg.vip_min_grade_by_geometry_path);
    const auto it = overrides.find(geometry_key_for_grade_target(cfg.box_rows, cfg.box_cols));
    return it != overrides.end() ? normalize_vip_grade_target(it->second) : default_grade;
}

inline VipScoreBreakdown compute_vip_score_breakdown(const GenerateRunResult& result, const GenerateRunConfig& cfg, const GenerationProfile& runtime_profile) {
    VipScoreBreakdown b{};
    b.profile = normalize_vip_score_profile(cfg.vip_score_profile);
    const int n = std::max(1, cfg.box_rows) * std::max(1, cfg.box_cols);
    const double attempts = static_cast<double>(std::max<uint64_t>(1, result.attempts));
    const double logic_steps_per_attempt = static_cast<double>(result.logic_steps_total) / attempts;
    const double hidden_rate = static_cast<double>(result.strategy_hidden_hit) / attempts;
    const double naked_rate = static_cast<double>(result.strategy_naked_hit) / attempts;
    const double uniqueness_nodes_per_attempt = static_cast<double>(result.uniqueness_nodes) / attempts;
    const double reject_branching_rate = static_cast<double>(result.reject_logic + result.reject_strategy + result.reject_uniqueness) / attempts;
    const double asymmetry_norm = clamp01((runtime_profile.asymmetry_ratio - 1.0) / 3.0);

    b.logic_depth_norm = clamp01(logic_steps_per_attempt / std::max(4.0, static_cast<double>(n) * 0.35));
    b.hidden_norm = clamp01(hidden_rate / 0.60);
    b.naked_norm = clamp01(naked_rate / 0.80);
    b.uniqueness_norm = clamp01(uniqueness_nodes_per_attempt / std::max(500.0, static_cast<double>(n * n) * 0.90));
    b.branching_norm = clamp01(reject_branching_rate / 0.85);
    b.level_norm = clamp01(static_cast<double>(std::clamp(cfg.difficulty_level_required, 1, 9) - 1) / 8.0);

    double w_logic = 0.30, w_hidden = 0.22, w_naked = 0.12, w_uniq = 0.20, w_branch = 0.10, w_level = 0.06;
    if (b.profile == "strict") {
        w_logic = 0.34; w_hidden = 0.24; w_naked = 0.08; w_uniq = 0.22; w_branch = 0.08; w_level = 0.04;
    } else if (b.profile == "ultra") {
        w_logic = 0.36; w_hidden = 0.25; w_naked = 0.06; w_uniq = 0.23; w_branch = 0.07; w_level = 0.03;
    }
    b.weighted = w_logic * b.logic_depth_norm + w_hidden * b.hidden_norm + w_naked * b.naked_norm +
                 w_uniq * b.uniqueness_norm + w_branch * b.branching_norm + w_level * b.level_norm;
    b.asym_multiplier = 1.0 + 0.15 * asymmetry_norm;
    if (b.profile == "ultra") b.asym_multiplier += 0.05 * asymmetry_norm;
    b.final_score = std::clamp(b.weighted * b.asym_multiplier * 1000.0, 0.0, 1000.0);
    return b;
}

inline double compute_vip_score(const GenerateRunResult& result, const GenerateRunConfig& cfg, const GenerationProfile& runtime_profile) {
    return compute_vip_score_breakdown(result, cfg, runtime_profile).final_score;
}

inline bool vip_contract_passed(double score, const std::string& target_grade) {
    return vip_grade_rank(vip_grade_from_score(score)) >= vip_grade_rank(target_grade);
}

inline uint64_t compute_premium_signature(const GenerateRunConfig& cfg, const GenerateRunResult& result, const std::string& cpu_backend_selected) {
    uint64_t h = 1469598103934665603ULL;
    const int n = std::max(1, cfg.box_rows) * std::max(1, cfg.box_cols);
    h = fnv1a64_bytes(&n, sizeof(n), h);
    h = fnv1a64_bytes(&cfg.box_rows, sizeof(cfg.box_rows), h);
    h = fnv1a64_bytes(&cfg.box_cols, sizeof(cfg.box_cols), h);
    h = fnv1a64_bytes(&cfg.difficulty_level_required, sizeof(cfg.difficulty_level_required), h);
    h = fnv1a64_bytes(&result.accepted, sizeof(result.accepted), h);
    h = fnv1a64_bytes(&result.attempts, sizeof(result.attempts), h);
    h = fnv1a64_bytes(&result.logic_steps_total, sizeof(result.logic_steps_total), h);
    h = fnv1a64_bytes(&result.uniqueness_nodes, sizeof(result.uniqueness_nodes), h);
    h = fnv1a64_bytes(&result.vip_score, sizeof(result.vip_score), h);
    h = fnv1a64_bytes(cpu_backend_selected.data(), cpu_backend_selected.size(), h);
    return h;
}

inline uint64_t compute_premium_signature_v2(const GenerateRunConfig& cfg, const GenerateRunResult& result, const std::string& cpu_backend_selected, const std::string& vip_grade_target_effective) {
    uint64_t h = compute_premium_signature(cfg, result, cpu_backend_selected);
    h = fnv1a64_bytes(result.vip_grade.data(), result.vip_grade.size(), h);
    h = fnv1a64_bytes(vip_grade_target_effective.data(), vip_grade_target_effective.size(), h);
    h = fnv1a64_bytes(result.vip_score_breakdown_json.data(), result.vip_score_breakdown_json.size(), h);
    h = fnv1a64_bytes(&result.kernel_time_ms, sizeof(result.kernel_time_ms), h);
    h = fnv1a64_bytes(&result.kernel_calls, sizeof(result.kernel_calls), h);
    return h;
}

inline void write_vip_trace_and_signature_if_requested(const GenerateRunConfig& cfg, const GenerateRunResult& result, const std::string& cpu_backend_selected) {
    if (!cfg.difficulty_trace_out.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(cfg.difficulty_trace_out).parent_path(), ec);
        std::ofstream out(cfg.difficulty_trace_out, std::ios::out | std::ios::trunc);
        if (out) {
            out << "difficulty_engine=" << normalize_difficulty_engine(cfg.difficulty_engine) << "\nvip_mode=" << (cfg.vip_mode ? "1" : "0")
                << "\ncpu_backend=" << cpu_backend_selected << "\nlogic_steps_total=" << result.logic_steps_total
                << "\nnaked_use=" << result.strategy_naked_use << " naked_hit=" << result.strategy_naked_hit
                << "\nhidden_use=" << result.strategy_hidden_use << " hidden_hit=" << result.strategy_hidden_hit
                << "\nvip_score=" << format_fixed(result.vip_score, 3) << "\nvip_grade=" << result.vip_grade
                << "\npremium_signature=" << result.premium_signature << "\npremium_signature_v2=" << result.premium_signature_v2 << "\n";
            if (normalize_vip_trace_level(cfg.vip_trace_level) == "full") {
                out << "vip_contract_fail_reason=" << result.vip_contract_fail_reason << "\nvip_score_breakdown_json=" << result.vip_score_breakdown_json
                    << "\nkernel_time_ms=" << format_fixed(result.kernel_time_ms, 6) << "\nkernel_calls=" << result.kernel_calls
                    << "\nbackend_efficiency_score=" << format_fixed(result.backend_efficiency_score, 6) << "\nasymmetry_efficiency_index=" << format_fixed(result.asymmetry_efficiency_index, 6) << "\n";
            }
        }
    }
    if (!cfg.vip_signature_out.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(cfg.vip_signature_out).parent_path(), ec);
        std::ofstream out(cfg.vip_signature_out, std::ios::out | std::ios::trunc);
        if (out) out << "premium_signature=" << result.premium_signature << "\npremium_signature_v2=" << result.premium_signature_v2
                     << "\nvip_grade=" << result.vip_grade << "\nvip_score=" << format_fixed(result.vip_score, 3) << "\ncpu_backend=" << cpu_backend_selected << "\n";
    }
}

inline void append_difficulty_signals_csv(const GenerateRunConfig& cfg, const GenerateRunResult& result, const DifficultySignals& signals, const std::string& csv_path = "plikiTMP/porownania/difficulty_signals.csv") {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(csv_path).parent_path(), ec);
    bool write_header = !std::filesystem::exists(csv_path);
    std::ofstream out(csv_path, std::ios::out | std::ios::app);
    if (out && write_header) {
        out << "ts,box_rows,box_cols,n,difficulty,required_strategy,mode,attempts,accepted,rejected,logic_passes,candidate_eliminations,branching_proxy,max_depth_proxy,reject_replay,reject_distribution_bias,reject_uniqueness_budget,uniqueness_nodes,uniqueness_cost_ms,elapsed_s,avg_clues\n";
    }
    if (out) {
        out << now_local_compact_string() << "," << cfg.box_rows << "," << cfg.box_cols << "," << (cfg.box_rows * cfg.box_cols) << ","
            << cfg.difficulty_level_required << "," << to_string(cfg.required_strategy) << "," << signals.mode << ","
            << signals.attempts << "," << signals.accepted << "," << signals.rejected << "," << signals.logic_passes << ","
            << signals.candidate_eliminations << "," << signals.branching_proxy << "," << signals.max_depth_proxy << ","
            << result.reject_replay << "," << result.reject_distribution_bias << "," << result.reject_uniqueness_budget << ","
            << signals.uniqueness_nodes << "," << format_fixed(signals.uniqueness_cost_ms, 6) << "," << format_fixed(signals.elapsed_s, 6) << ","
            << format_fixed(result.avg_clues, 6) << "\n";
    }
}

GenerateRunResult run_generic_sudoku(
    const GenerateRunConfig& cfg,
    ConsoleStatsMonitor* monitor,
    const std::atomic<bool>* external_cancel,
    const std::atomic<bool>* external_paused,
    std::function<void(uint64_t, uint64_t)> on_progress,
    std::function<void(const std::string&)> on_log) {
    
    GenerateRunResult result{};
    const auto topo_opt = GenericTopology::build(cfg.box_rows, cfg.box_cols);
    if (!topo_opt.has_value()) {
        std::cerr << "Unsupported geometry. Expected box_rows*box_cols in [4..36].\n";
        return result;
    }
    const GenericTopology topo = *topo_opt;
    const GenerationProfile runtime_profile = resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
    const RuntimeCpuContext cpu_ctx = resolve_runtime_cpu_context(cfg);
    
    GenerateRunConfig runtime_cfg = cfg;
    runtime_cfg.asym_heuristics_mode = normalize_asym_heuristics_mode(runtime_cfg.asym_heuristics_mode);
    runtime_cfg.vip_score_profile = normalize_vip_score_profile(runtime_cfg.vip_score_profile);
    runtime_cfg.vip_trace_level = normalize_vip_trace_level(runtime_cfg.vip_trace_level);
    
    if (!runtime_profile.is_symmetric) {
        if (runtime_cfg.asym_heuristics_mode == "aggressive") {
            runtime_cfg.min_clues = std::max(0, runtime_cfg.min_clues - 1);
            runtime_cfg.max_clues = std::max(runtime_cfg.min_clues, runtime_cfg.max_clues - 1);
            runtime_cfg.uniqueness_confirm_budget_nodes = std::max<uint64_t>(100000, static_cast<uint64_t>(runtime_cfg.uniqueness_confirm_budget_nodes * 0.85));
        } else if (runtime_cfg.asym_heuristics_mode == "off") {
            runtime_cfg.enable_distribution_filter = false;
        }
    }
    
    if (runtime_cfg.adaptive_budget) {
        const double suggested = std::max(0.05, runtime_profile.suggested_budget_s);
        if (runtime_cfg.attempt_time_budget_s <= 0.0) runtime_cfg.attempt_time_budget_s = std::min(5.0, suggested);
        if (runtime_cfg.attempt_node_budget == 0) runtime_cfg.attempt_node_budget = static_cast<uint64_t>(std::clamp<double>(static_cast<double>(topo.nn) * 4000.0, 200000.0, 10000000.0));
    }

    const long long seed = (cfg.seed == 0) ? random_seed_i64() : cfg.seed;
    const int thread_count = cfg.threads > 0 ? cfg.threads : std::max(1u, std::thread::hardware_concurrency());

    auto log_message = [&](const std::string& msg) {
        log_info("run_generic.userlog", msg);
        if (on_log) {
            on_log(msg);
        }
    };
    log_info(
        "run_generic",
        "start n=" + std::to_string(topo.n) +
            " target=" + std::to_string(cfg.target_puzzles) +
            " level=" + std::to_string(cfg.difficulty_level_required) +
            " required=" + to_string(cfg.required_strategy) +
            " difficulty_engine=" + normalize_difficulty_engine(cfg.difficulty_engine) +
            " vip_mode=" + std::string(cfg.vip_mode ? "1" : "0") +
            " profile_policy=" + normalize_profile_mode_policy(cfg.profile_mode_policy) +
            " full_for_n_ge=" + std::to_string(cfg.full_for_n_ge) +
            " asym_heuristics=" + runtime_cfg.asym_heuristics_mode +
            " adaptive_budget=" + std::string(runtime_cfg.adaptive_budget ? "1" : "0") +
            " cpu_backend=" + cpu_ctx.selected_backend +
            " cpu_reason=" + cpu_ctx.reason +
            " threads=" + std::to_string(thread_count) +
            " seed=" + std::to_string(seed));
    log_message(
        "Start: n=" + std::to_string(topo.n) +
        " level=" + std::to_string(cfg.difficulty_level_required) +
        " required=" + to_string(cfg.required_strategy) +
        " backend=" + cpu_ctx.selected_backend +
        " threads=" + std::to_string(thread_count) +
        " seed=" + std::to_string(seed));
    log_message(
        "Parametry runtime: min_clues=" + std::to_string(runtime_cfg.min_clues) +
        " max_clues=" + std::to_string(runtime_cfg.max_clues) +
        " attempt_time_budget_s=" + format_fixed(runtime_cfg.attempt_time_budget_s, 3) +
        " attempt_node_budget=" + std::to_string(runtime_cfg.attempt_node_budget) +
        " asym_heuristics=" + runtime_cfg.asym_heuristics_mode +
        " adaptive_budget=" + std::string(runtime_cfg.adaptive_budget ? "1" : "0"));
    
    std::error_code mkdir_ec;
    std::filesystem::create_directories(cfg.output_folder, mkdir_ec);
    if (mkdir_ec) {
        const std::string msg = "Cannot create output folder: " + cfg.output_folder + " error=" + mkdir_ec.message();
        log_error("run_generic", msg);
        log_message(msg);
        return result;
    }
    const std::filesystem::path out_path = std::filesystem::path(cfg.output_folder) / cfg.output_file;

    if (monitor != nullptr) {
        monitor->set_active_workers(thread_count);
        monitor->set_grid_info(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required);
        MonitorTotalsSnapshot initial_totals{};
        initial_totals.target = cfg.target_puzzles;
        initial_totals.active_workers = static_cast<uint64_t>(thread_count);
        monitor->set_totals_snapshot(initial_totals);
        monitor->set_background_status(
            "Generowanie: start mode=" + std::string(generation_mode_name(runtime_profile.mode)) +
            " class=" + (runtime_profile.is_symmetric ? std::string("sym") : std::string("asym")) +
            " backend=" + cpu_ctx.selected_backend);
    }

    PaddedAtomic<uint64_t> accepted{0}, attempts{0}, attempt_ticket{0}, rejected{0}, reject_prefilter{0};
    PaddedAtomic<uint64_t> reject_logic{0}, reject_uniqueness{0}, reject_strategy{0}, reject_replay{0};
    PaddedAtomic<uint64_t> reject_distribution_bias{0}, reject_uniqueness_budget{0}, timeout_global_count{0};
    PaddedAtomic<uint64_t> timeout_per_attempt_count{0}, uniqueness_calls_total{0}, uniqueness_nodes_total{0};
    PaddedAtomic<uint64_t> uniqueness_elapsed_ns_total{0}, logic_steps_total{0}, strategy_naked_use_total{0};
    PaddedAtomic<uint64_t> strategy_naked_hit_total{0}, strategy_hidden_use_total{0}, strategy_hidden_hit_total{0};
    PaddedAtomic<uint64_t> premium_signature_last{0}, written{0}, reseeds_total{0}, clue_sum{0};
    PaddedAtomic<uint64_t> analyzed_required_strategy{0}, required_strategy_hits{0}, written_required_strategy{0}, clue_sum_required_strategy{0};
    PaddedAtomic<bool> force_abort{false}, io_failed{false};
    PaddedAtomic<int> started_workers{0};
    
    auto telemetry_ring = std::make_unique<TelemetryMpscRing<4096>>();
    auto apply_telemetry_delta = [&](const TelemetryDelta& d) {
        if (d.attempts > 0) attempts.fetch_add(d.attempts, std::memory_order_relaxed);
        if (d.rejected > 0) rejected.fetch_add(d.rejected, std::memory_order_relaxed);
        if (d.reject_prefilter > 0) reject_prefilter.fetch_add(d.reject_prefilter, std::memory_order_relaxed);
        if (d.reject_logic > 0) reject_logic.fetch_add(d.reject_logic, std::memory_order_relaxed);
        if (d.reject_uniqueness > 0) reject_uniqueness.fetch_add(d.reject_uniqueness, std::memory_order_relaxed);
        if (d.reject_strategy > 0) reject_strategy.fetch_add(d.reject_strategy, std::memory_order_relaxed);
        if (d.reject_replay > 0) reject_replay.fetch_add(d.reject_replay, std::memory_order_relaxed);
        if (d.reject_distribution_bias > 0) reject_distribution_bias.fetch_add(d.reject_distribution_bias, std::memory_order_relaxed);
        if (d.reject_uniqueness_budget > 0) reject_uniqueness_budget.fetch_add(d.reject_uniqueness_budget, std::memory_order_relaxed);
        if (d.analyzed_required > 0) analyzed_required_strategy.fetch_add(d.analyzed_required, std::memory_order_relaxed);
        if (d.required_hits > 0) required_strategy_hits.fetch_add(d.required_hits, std::memory_order_relaxed);
        if (d.uniqueness_calls > 0) uniqueness_calls_total.fetch_add(d.uniqueness_calls, std::memory_order_relaxed);
        if (d.uniqueness_nodes > 0) uniqueness_nodes_total.fetch_add(d.uniqueness_nodes, std::memory_order_relaxed);
        if (d.uniqueness_elapsed_ns > 0) uniqueness_elapsed_ns_total.fetch_add(d.uniqueness_elapsed_ns, std::memory_order_relaxed);
        if (d.logic_steps > 0) logic_steps_total.fetch_add(d.logic_steps, std::memory_order_relaxed);
        if (d.naked_use > 0) strategy_naked_use_total.fetch_add(d.naked_use, std::memory_order_relaxed);
        if (d.naked_hit > 0) strategy_naked_hit_total.fetch_add(d.naked_hit, std::memory_order_relaxed);
        if (d.hidden_use > 0) strategy_hidden_use_total.fetch_add(d.hidden_use, std::memory_order_relaxed);
        if (d.hidden_hit > 0) strategy_hidden_hit_total.fetch_add(d.hidden_hit, std::memory_order_relaxed);
        if (d.reseeds > 0) reseeds_total.fetch_add(d.reseeds, std::memory_order_relaxed);
    };
    auto drain_telemetry_ring = [&]() {
        TelemetryDelta d{};
        while (telemetry_ring->try_pop(d)) {
            apply_telemetry_delta(d);
        }
    };
    const auto start = std::chrono::steady_clock::now();
    auto push_monitor_totals = [&]() {
        if (monitor == nullptr) {
            return;
        }
        MonitorTotalsSnapshot snapshot{};
        snapshot.target = cfg.target_puzzles;
        snapshot.accepted = std::min<uint64_t>(accepted.load(std::memory_order_relaxed), cfg.target_puzzles);
        snapshot.written = written.load(std::memory_order_relaxed);
        snapshot.attempts = attempts.load(std::memory_order_relaxed);
        snapshot.analyzed_required_strategy = analyzed_required_strategy.load(std::memory_order_relaxed);
        snapshot.required_strategy_hits = required_strategy_hits.load(std::memory_order_relaxed);
        snapshot.written_required_strategy = written_required_strategy.load(std::memory_order_relaxed);
        snapshot.rejected = rejected.load(std::memory_order_relaxed);
        snapshot.active_workers = static_cast<uint64_t>(thread_count);
        snapshot.reseeds = reseeds_total.load(std::memory_order_relaxed);
        monitor->set_totals_snapshot(snapshot);
    };
    auto push_strategy_snapshot = [&]() {
        if (monitor == nullptr) {
            return;
        }
        StrategyRow row{};
        row.strategy = to_string(cfg.required_strategy);
        row.lvl = cfg.difficulty_level_required;
        row.max_attempts = cfg.max_attempts;
        row.analyzed = analyzed_required_strategy.load(std::memory_order_relaxed);
        row.required_strategy_hits = required_strategy_hits.load(std::memory_order_relaxed);
        const double elapsed_min =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / 60.0;
        row.analyzed_per_min = elapsed_min > 0.0 ? static_cast<double>(row.analyzed) / elapsed_min : 0.0;
        row.est_5min = static_cast<uint64_t>(row.analyzed_per_min * 5.0);
        row.written = written_required_strategy.load(std::memory_order_relaxed);
        row.avg_clues = row.written > 0
            ? static_cast<double>(clue_sum_required_strategy.load(std::memory_order_relaxed)) / static_cast<double>(row.written)
            : 0.0;
        monitor->update_strategy_row(row);
    };

    auto output_ring = std::make_unique<OutputLineMpscRing<256>>();
    std::ofstream batch_out(out_path, std::ios::out | std::ios::trunc);
    if (!batch_out) {
        const std::string msg = "Cannot open output file: " + out_path.string();
        log_error("run_generic", msg);
        log_message(msg);
        return result;
    }
    
    std::atomic<bool> writer_stop{false};
    std::thread writer_thread([&]() {
        OutputLineEvent ev{};
        while (!writer_stop.load(std::memory_order_relaxed) || !output_ring->empty()) {
            if (!output_ring->try_pop(ev)) {
                std::this_thread::yield();
                continue;
            }
            batch_out.write(ev.bytes.data(), static_cast<std::streamsize>(ev.len));
            batch_out.put('\n');
            if (!batch_out) {
                io_failed.store(true, std::memory_order_relaxed);
                force_abort.store(true, std::memory_order_relaxed);
                break;
            }
            written.fetch_add(1, std::memory_order_relaxed);

            if (cfg.write_individual_files) {
                std::ostringstream one_name;
                one_name << "sudoku_" << std::setw(6) << std::setfill('0') << ev.accepted_idx << ".txt";
                std::ofstream one_out(std::filesystem::path(cfg.output_folder) / one_name.str(), std::ios::out | std::ios::trunc);
                if (one_out) {
                    one_out.write(ev.bytes.data(), static_cast<std::streamsize>(ev.len));
                    one_out.put('\n');
                }
            }
        }
        batch_out.flush();
    });

    const GenericSolvedKernel solved(GenericSolvedKernel::backend_from_string(cpu_ctx.selected_backend));
    const GenericDigKernel dig;
    const GenericQuickPrefilter prefilter;
    const GenericLogicCertify logic;

    push_monitor_totals();
    push_strategy_snapshot();

    auto worker_task = [&](int tid) {
        uint64_t worker_seed_state = static_cast<uint64_t>(seed) ^ (0x9e3779b97f4a7c15ULL * (tid + 1));
        long long local_seed = bounded_positive_seed_i64(splitmix64(worker_seed_state));
        std::mt19937_64 rng(static_cast<uint64_t>(local_seed));
        GenericUniquenessCounter uniq;
        
        const bool reporter_thread = (tid == 0);
        const bool monitor_enabled = (monitor != nullptr);
        const bool has_external_cancel = (external_cancel != nullptr);
        const bool has_external_pause = (external_paused != nullptr);
        const bool reseed_enabled = cfg.reseed_interval_s > 0;
        const auto reseed_interval = std::chrono::seconds(cfg.reseed_interval_s);
        const bool has_global_time_limit = (cfg.max_total_time_s > 0 || cfg.max_attempts_s > 0);
        
        auto last_reseed_time = std::chrono::steady_clock::now();
        auto last_monitor_push = std::chrono::steady_clock::now();
        auto last_worker_push = std::chrono::steady_clock::now();
        auto current_attempt_start = std::chrono::steady_clock::now();
        bool attempt_in_progress = false;
        uint64_t reseed_counter = 0;
        uint64_t worker_applied = 0;
        uint64_t worker_dead_ends = 0;
        uint64_t worker_attempts_finished = 0;
        uint64_t worker_uniqueness_nodes_sum = 0;
        uint64_t worker_uniqueness_elapsed_ns_sum = 0;
        double worker_attempt_elapsed_ms_sum = 0.0;
        double worker_stage_solved_ms_sum = 0.0;
        double worker_stage_dig_ms_sum = 0.0;
        double worker_stage_prefilter_ms_sum = 0.0;
        double worker_stage_logic_ms_sum = 0.0;
        double worker_stage_uniqueness_ms_sum = 0.0;
        bool paused_state = false;

        WorkerRow worker{};
        {
            std::ostringstream name;
            name << "worker_" << std::setw(2) << std::setfill('0') << tid;
            worker.worker = name.str();
        }
        worker.seed = local_seed;
        worker.status = "running";
        worker.last_reseed_steady_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(last_reseed_time.time_since_epoch()).count());
        worker.reseed_interval_s = cfg.reseed_interval_s;
        worker.attempt_time_budget_s = 0.0;
        worker.attempt_node_budget = 0;
        auto ns_to_ms = [](uint64_t ns) -> double {
            return static_cast<double>(ns) / 1'000'000.0;
        };
        auto refresh_worker_dynamic = [&](const std::chrono::steady_clock::time_point& now_tp) {
            const double elapsed_since_reseed_s = std::chrono::duration<double>(now_tp - last_reseed_time).count();
            worker.last_reseed_steady_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(last_reseed_time.time_since_epoch()).count());
            if (reseed_enabled) {
                const double interval_s = static_cast<double>(cfg.reseed_interval_s);
                worker.reset_in_s = std::max(0.0, interval_s - elapsed_since_reseed_s);
                worker.reset_lag = std::max(0.0, elapsed_since_reseed_s - interval_s);
                worker.lag_max = std::max(worker.lag_max, worker.reset_lag);
            } else {
                worker.reset_in_s = 0.0;
                worker.reset_lag = 0.0;
                worker.lag_max = 0.0;
            }
            if (attempt_in_progress) {
                worker.attempt_time_budget_s = std::chrono::duration<double>(now_tp - current_attempt_start).count();
            } else {
                worker.attempt_time_budget_s = 0.0;
            }
            worker.attempt_node_budget = worker_uniqueness_nodes_sum;
        };
        auto maybe_publish_worker = [&](const std::chrono::steady_clock::time_point& now_tp, bool force) {
            refresh_worker_dynamic(now_tp);
            if (!monitor_enabled) {
                return;
            }
            if (force || (now_tp - last_worker_push) >= std::chrono::milliseconds(500)) {
                monitor->set_worker_row(static_cast<size_t>(tid), worker);
                last_worker_push = now_tp;
            }
        };
        maybe_publish_worker(std::chrono::steady_clock::now(), true);
        
        started_workers.fetch_add(1, std::memory_order_relaxed);
        while (started_workers.load(std::memory_order_relaxed) < thread_count) {
            if (force_abort.load(std::memory_order_relaxed) || (has_external_cancel && external_cancel->load(std::memory_order_relaxed))) {
                worker.status = "cancel_requested";
                if (monitor_enabled) {
                    monitor->set_worker_row(static_cast<size_t>(tid), worker);
                }
                return;
            }
            std::this_thread::yield();
        }

        GenericPuzzleCandidate candidate;
        candidate.puzzle.resize(static_cast<size_t>(topo.nn));
        candidate.solution.resize(static_cast<size_t>(topo.nn));
        
        TelemetryDelta local_delta{};
        uint64_t local_attempts_done = 0;
        
        while (!force_abort.load(std::memory_order_relaxed)) {
            if (has_external_cancel && external_cancel->load(std::memory_order_relaxed)) {
                force_abort.store(true, std::memory_order_relaxed);
                worker.status = "cancel_requested";
                break;
            }
            if (has_external_pause && external_paused->load(std::memory_order_relaxed)) {
                if (!paused_state) {
                    paused_state = true;
                    worker.status = "paused";
                    maybe_publish_worker(std::chrono::steady_clock::now(), true);
                }
                maybe_publish_worker(std::chrono::steady_clock::now(), false);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else if (paused_state) {
                paused_state = false;
                worker.status = "running";
                maybe_publish_worker(std::chrono::steady_clock::now(), true);
            }

            if (reseed_enabled || has_global_time_limit) {
                const auto now_tp = std::chrono::steady_clock::now();

                if (reseed_enabled && (now_tp - last_reseed_time) >= reseed_interval) {
                    const long long old_seed = local_seed;
                    worker_seed_state = splitmix64(worker_seed_state);
                    reseed_counter = worker_seed_state;
                    local_seed = bounded_positive_seed_i64(reseed_counter);
                    rng.seed(static_cast<uint64_t>(local_seed));
                    last_reseed_time = now_tp;
                    reseeds_total.fetch_add(1, std::memory_order_relaxed);
                    worker.resets += 1;
                    worker.seed = local_seed;
                    log_info(
                        "run_generic.reseed",
                        "tid=" + std::to_string(tid) +
                            " old_seed=" + std::to_string(old_seed) +
                            " new_seed=" + std::to_string(local_seed) +
                            " interval_s=" + std::to_string(cfg.reseed_interval_s) +
                            " reason=reseed_interval_s_elapsed");
                    maybe_publish_worker(now_tp, true);
                }

                if (has_global_time_limit) {
                    const auto total_elapsed = std::chrono::duration<double>(now_tp - start).count();
                    if (cfg.max_total_time_s > 0 && total_elapsed > static_cast<double>(cfg.max_total_time_s)) {
                        timeout_global_count.fetch_add(1, std::memory_order_relaxed);
                        force_abort.store(true, std::memory_order_relaxed);
                        worker.status = "global_timeout";
                        break;
                    }
                    if (cfg.max_attempts_s > 0 && total_elapsed > static_cast<double>(cfg.max_attempts_s)) {
                        timeout_per_attempt_count.fetch_add(1, std::memory_order_relaxed);
                        worker.status = "run_time_budget";
                        break;
                    }
                }
            }

            if (accepted.load(std::memory_order_relaxed) >= cfg.target_puzzles || io_failed.load(std::memory_order_relaxed)) {
                force_abort.store(true, std::memory_order_relaxed);
                worker.status = io_failed.load(std::memory_order_relaxed) ? "io_error" : "target_reached";
                break;
            }
            if (cfg.max_attempts > 0) {
                const uint64_t att = attempt_ticket.fetch_add(1, std::memory_order_relaxed) + 1ULL;
                if (att > cfg.max_attempts) {
                    force_abort.store(true, std::memory_order_relaxed);
                    worker.status = "max_attempts";
                    break;
                }
            }

            ++local_attempts_done;
            attempts.fetch_add(1, std::memory_order_relaxed);
            current_attempt_start = std::chrono::steady_clock::now();
            attempt_in_progress = true;
            maybe_publish_worker(current_attempt_start, false);
            const auto attempt_t0 = current_attempt_start;

            RejectReason reason = RejectReason::None;
            RequiredStrategyAttemptInfo strategy_info{};
            AttemptPerfStats perf_stats{};
            bool attempt_timed_out = false;
            
            bool ok = generate_one_generic(
                runtime_cfg, topo, rng, candidate, reason, strategy_info,
                solved, dig, prefilter, logic, uniq,
                &force_abort.value, &attempt_timed_out, external_cancel, external_paused,
                nullptr, nullptr, nullptr, &perf_stats);
            attempt_in_progress = false;
            const auto attempt_elapsed_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - attempt_t0).count();
            ++worker_attempts_finished;
            worker_attempt_elapsed_ms_sum += attempt_elapsed_ms;
            worker.avg_attempt_ms = worker_attempt_elapsed_ms_sum / static_cast<double>(worker_attempts_finished);

            // Aktualizacja metryk
            if (strategy_info.required_strategy_use_confirmed) {
                analyzed_required_strategy.fetch_add(1, std::memory_order_relaxed);
            }
            if (strategy_info.required_strategy_hit_confirmed) {
                required_strategy_hits.fetch_add(1, std::memory_order_relaxed);
            }
            local_delta.uniqueness_calls += perf_stats.uniqueness_calls;
            local_delta.uniqueness_nodes += perf_stats.uniqueness_nodes;
            local_delta.uniqueness_elapsed_ns += perf_stats.uniqueness_elapsed_ns;
            local_delta.logic_steps += perf_stats.logic_steps;
            local_delta.naked_use += perf_stats.strategy_naked_use;
            local_delta.naked_hit += perf_stats.strategy_naked_hit;
            local_delta.hidden_use += perf_stats.strategy_hidden_use;
            local_delta.hidden_hit += perf_stats.strategy_hidden_hit;
            worker_uniqueness_nodes_sum += perf_stats.uniqueness_nodes;
            worker_uniqueness_elapsed_ns_sum += perf_stats.uniqueness_elapsed_ns;
            worker.backtrack_count = worker_uniqueness_nodes_sum;
            if (worker_uniqueness_nodes_sum > 0) {
                worker.avg_node_ms =
                    (static_cast<double>(worker_uniqueness_elapsed_ns_sum) / 1'000'000.0) /
                    static_cast<double>(worker_uniqueness_nodes_sum);
            }
            worker_stage_solved_ms_sum += ns_to_ms(perf_stats.solved_elapsed_ns);
            worker_stage_dig_ms_sum += ns_to_ms(perf_stats.dig_elapsed_ns);
            worker_stage_prefilter_ms_sum += ns_to_ms(perf_stats.prefilter_elapsed_ns);
            worker_stage_logic_ms_sum += ns_to_ms(perf_stats.logic_elapsed_ns);
            worker_stage_uniqueness_ms_sum += ns_to_ms(perf_stats.uniqueness_elapsed_ns);
            worker.stage_solved_ms = worker_stage_solved_ms_sum / static_cast<double>(worker_attempts_finished);
            worker.stage_dig_ms = worker_stage_dig_ms_sum / static_cast<double>(worker_attempts_finished);
            worker.stage_prefilter_ms = worker_stage_prefilter_ms_sum / static_cast<double>(worker_attempts_finished);
            worker.stage_logic_ms = worker_stage_logic_ms_sum / static_cast<double>(worker_attempts_finished);
            worker.stage_uniqueness_ms = worker_stage_uniqueness_ms_sum / static_cast<double>(worker_attempts_finished);

            if (ok) {
                const uint64_t acc_before = accepted.fetch_add(1, std::memory_order_relaxed);
                if (acc_before < cfg.target_puzzles) {
                    clue_sum.fetch_add(static_cast<uint64_t>(candidate.clues), std::memory_order_relaxed);
                    const std::string line = serialize_line_generic(local_seed, cfg, candidate, topo.nn);
                    worker.clues = candidate.clues;
                    worker.applied = ++worker_applied;
                    worker.status = "running";
                    
                    if (strategy_info.matched_required_strategy) {
                        written_required_strategy.fetch_add(1, std::memory_order_relaxed);
                        clue_sum_required_strategy.fetch_add(static_cast<uint64_t>(candidate.clues), std::memory_order_relaxed);
                    }
                    
                    while (!output_ring->try_push(acc_before + 1, line)) {
                        if (io_failed.load(std::memory_order_relaxed) || force_abort.load(std::memory_order_relaxed)) break;
                        std::this_thread::yield();
                    }
                    
                    if (on_progress) on_progress(acc_before + 1, cfg.target_puzzles);
                    
                    if (acc_before + 1 >= cfg.target_puzzles) {
                        force_abort.store(true, std::memory_order_relaxed);
                    }
                }
            } else {
                rejected.fetch_add(1, std::memory_order_relaxed);
                if (reason != RejectReason::None) {
                    ++worker_dead_ends;
                }
                switch (reason) {
                case RejectReason::Prefilter:
                    reject_prefilter.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::Logic:
                    reject_logic.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::Uniqueness:
                    reject_uniqueness.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::Strategy:
                    reject_strategy.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::Replay:
                    reject_replay.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::DistributionBias:
                    reject_distribution_bias.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::UniquenessBudget:
                    reject_uniqueness_budget.fetch_add(1, std::memory_order_relaxed);
                    break;
                case RejectReason::None: break;
                }
            }
            worker.dead_ends = worker_dead_ends;
            worker.success_rate_pct = local_attempts_done > 0
                ? (100.0 * static_cast<double>(worker_applied) / static_cast<double>(local_attempts_done))
                : 0.0;

            // Popychanie metryk co 256 iteracji (szybkie flush)
            if ((local_attempts_done & 255ULL) == 0ULL) {
                if (!telemetry_ring->try_push(local_delta)) {
                    apply_telemetry_delta(local_delta);
                }
                local_delta = TelemetryDelta{};
                if (reporter_thread) {
                    TelemetryDelta d{};
                    while (telemetry_ring->try_pop(d)) apply_telemetry_delta(d);
                }
            }
            if (monitor_enabled && reporter_thread) {
                const auto now_tp = std::chrono::steady_clock::now();
                if ((now_tp - last_monitor_push) >= std::chrono::milliseconds(500)) {
                    drain_telemetry_ring();
                    push_monitor_totals();
                    push_strategy_snapshot();
                    monitor->set_background_status(
                        "Generowanie: accepted=" + std::to_string(std::min<uint64_t>(accepted.load(std::memory_order_relaxed), cfg.target_puzzles)) +
                        "/" + std::to_string(cfg.target_puzzles) +
                        " attempts=" + std::to_string(attempts.load(std::memory_order_relaxed)) +
                        " rejected=" + std::to_string(rejected.load(std::memory_order_relaxed)));
                    last_monitor_push = now_tp;
                }
            }
            maybe_publish_worker(std::chrono::steady_clock::now(), false);
        }
        
        // Zrzut reszty metryk na koniec wątku
        if (!local_delta.empty()) {
            apply_telemetry_delta(local_delta);
        }
        worker.status = force_abort.load(std::memory_order_relaxed)
            ? (has_external_cancel && external_cancel->load(std::memory_order_relaxed) ? "cancel_requested" : "stopped")
            : "finished";
        if (monitor_enabled) {
            refresh_worker_dynamic(std::chrono::steady_clock::now());
            monitor->set_worker_row(static_cast<size_t>(tid), worker);
        }
    };

PersistentThreadPool::instance().run(thread_count, worker_task);
    
    drain_telemetry_ring();
    
    force_abort.store(true, std::memory_order_relaxed);
    writer_stop.store(true, std::memory_order_relaxed);
    if (writer_thread.joinable()) {
        writer_thread.join();
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsed_s = std::chrono::duration<double>(end - start).count();

    result.accepted = std::min(accepted.load(std::memory_order_relaxed), cfg.target_puzzles);
    result.written = written.load(std::memory_order_relaxed);
    result.attempts = attempts.load(std::memory_order_relaxed);
    result.attempts_total = result.attempts;
    result.analyzed_required_strategy = analyzed_required_strategy.load(std::memory_order_relaxed);
    result.required_strategy_hits = required_strategy_hits.load(std::memory_order_relaxed);
    result.written_required_strategy = written_required_strategy.load(std::memory_order_relaxed);
    result.rejected = rejected.load(std::memory_order_relaxed);
    result.reject_prefilter = reject_prefilter.load(std::memory_order_relaxed);
    result.reject_logic = reject_logic.load(std::memory_order_relaxed);
    result.reject_uniqueness = reject_uniqueness.load(std::memory_order_relaxed);
    result.reject_strategy = reject_strategy.load(std::memory_order_relaxed);
    result.reject_replay = reject_replay.load(std::memory_order_relaxed);
    result.reject_distribution_bias = reject_distribution_bias.load(std::memory_order_relaxed);
    result.reject_uniqueness_budget = reject_uniqueness_budget.load(std::memory_order_relaxed);
    result.timeout_global = timeout_global_count.load(std::memory_order_relaxed);
    result.timeout_per_attempt = timeout_per_attempt_count.load(std::memory_order_relaxed);
    result.uniqueness_calls = uniqueness_calls_total.load(std::memory_order_relaxed);
    result.uniqueness_nodes = uniqueness_nodes_total.load(std::memory_order_relaxed);
    result.uniqueness_elapsed_ms = static_cast<double>(uniqueness_elapsed_ns_total.load(std::memory_order_relaxed)) / 1'000'000.0;
    result.uniqueness_avg_ms = result.uniqueness_calls > 0 ? (result.uniqueness_elapsed_ms / static_cast<double>(result.uniqueness_calls)) : 0.0;
    result.logic_steps_total = logic_steps_total.load(std::memory_order_relaxed);
    result.strategy_naked_use = strategy_naked_use_total.load(std::memory_order_relaxed);
    result.strategy_naked_hit = strategy_naked_hit_total.load(std::memory_order_relaxed);
    result.strategy_hidden_use = strategy_hidden_use_total.load(std::memory_order_relaxed);
    result.strategy_hidden_hit = strategy_hidden_hit_total.load(std::memory_order_relaxed);
    result.cpu_backend_selected = cpu_ctx.selected_backend;
    result.elapsed_s = elapsed_s;
    result.accepted_per_sec = elapsed_s > 0.0 ? static_cast<double>(result.accepted) / elapsed_s : 0.0;
    result.kernel_time_ms = elapsed_s * 1000.0;
    result.kernel_calls = result.attempts;
    result.backend_efficiency_score = result.accepted_per_sec * (1.0 + 0.05 * static_cast<double>(topo.n));
    result.asymmetry_efficiency_index = result.accepted_per_sec * runtime_profile.asymmetry_ratio;
    result.histogram_levels[std::clamp(runtime_cfg.difficulty_level_required, 0, 9)] = result.accepted;
    result.histogram_strategies[strategy_to_hist_idx_shared(runtime_cfg.required_strategy)] = result.accepted;
    result.avg_clues = result.written > 0 ? static_cast<double>(clue_sum.load(std::memory_order_relaxed)) / static_cast<double>(result.written) : 0.0;
    
    const bool vip_enabled = runtime_cfg.vip_mode || normalize_difficulty_engine(runtime_cfg.difficulty_engine) == "vip";
    const std::string vip_grade_target_effective = resolve_vip_grade_target_for_geometry(runtime_cfg);
    
    if (vip_enabled) {
        const VipScoreBreakdown breakdown = compute_vip_score_breakdown(result, runtime_cfg, runtime_profile);
        result.vip_score = breakdown.final_score;
        result.vip_score_breakdown_json = breakdown.to_json();
        result.vip_grade = vip_grade_from_score(result.vip_score);
    } else {
        result.vip_score = 0.0;
        result.vip_grade = "none";
        result.vip_score_breakdown_json = "{}";
    }
    
    result.vip_contract_ok = !vip_enabled || vip_contract_passed(result.vip_score, vip_grade_target_effective);
    if (!result.vip_contract_ok) {
        result.vip_contract_fail_reason = "target=" + vip_grade_target_effective + " got=" + result.vip_grade + " score=" + format_fixed(result.vip_score, 3);
    } else {
        result.vip_contract_fail_reason = "ok";
    }
    
    result.premium_signature = premium_signature_last.load(std::memory_order_relaxed);
    if (result.premium_signature == 0) {
        result.premium_signature = compute_premium_signature(runtime_cfg, result, cpu_ctx.selected_backend);
    }
    result.premium_signature_v2 = compute_premium_signature_v2(runtime_cfg, result, cpu_ctx.selected_backend, vip_grade_target_effective);
    
    write_vip_trace_and_signature_if_requested(runtime_cfg, result, cpu_ctx.selected_backend);
    const DifficultySignals diff_signals = collect_difficulty_signals(result, runtime_profile);
    append_difficulty_signals_csv(runtime_cfg, result, diff_signals);
    log_message(
        "Koniec: accepted=" + std::to_string(result.accepted) +
        "/" + std::to_string(cfg.target_puzzles) +
        " written=" + std::to_string(result.written) +
        " attempts=" + std::to_string(result.attempts) +
        " elapsed_s=" + format_fixed(result.elapsed_s, 2));

    if (monitor != nullptr) {
        MonitorTotalsSnapshot snapshot{};
        snapshot.target = cfg.target_puzzles;
        snapshot.accepted = result.accepted;
        snapshot.written = result.written;
        snapshot.attempts = result.attempts;
        snapshot.analyzed_required_strategy = result.analyzed_required_strategy;
        snapshot.required_strategy_hits = result.required_strategy_hits;
        snapshot.written_required_strategy = result.written_required_strategy;
        snapshot.rejected = result.rejected;
        snapshot.active_workers = static_cast<uint64_t>(thread_count);
        snapshot.reseeds = reseeds_total.load(std::memory_order_relaxed);
        monitor->set_totals_snapshot(snapshot);
        monitor->set_background_status("Generowanie zakonczone: accepted=" + std::to_string(result.accepted) + "/" + std::to_string(cfg.target_puzzles) + ", elapsed_s=" + format_fixed(result.elapsed_s, 2));
        monitor->add_avg_clues_per_level(cfg.difficulty_level_required, result.avg_clues);
    }
    return result;
}

RequiredStrategy parse_required_strategy(const std::string& s) {
    std::string key;
    key.reserve(s.size());
    for (unsigned char ch : s) {
        if (std::isalnum(ch) != 0) {
            key.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (key.empty()) return RequiredStrategy::None;
    if (key == "none" || key == "brak") return RequiredStrategy::None;
    if (key == "naked" || key == "nakedsingle" || key == "single") return RequiredStrategy::NakedSingle;
    if (key == "hidden" || key == "hiddensingle") return RequiredStrategy::HiddenSingle;
    if (key == "backtracking" || key == "brutalny" || key == "bruteforce") return RequiredStrategy::Backtracking;
    return RequiredStrategy::None;
}

int strategy_adjusted_level(int level, RequiredStrategy strategy) {
    const int clamped = std::clamp(level, 1, 9);
    switch (strategy) {
    case RequiredStrategy::NakedSingle: return std::min(clamped, 2);
    case RequiredStrategy::HiddenSingle: return std::clamp(clamped, 2, 4);
    case RequiredStrategy::Backtracking: return 9;
    case RequiredStrategy::None:
    default: return clamped;
    }
}

ClueRange clue_range_for_size_level(int n, int level) {
    const int lvl = std::clamp(level, 1, 9);
    if (n <= 0) return {};
    if (n == 6) {
        static constexpr int min_t[9] = {14, 10, 10, 8, 8, 7, 7, 7, 6};
        static constexpr int max_t[9] = {18, 14, 14, 10, 10, 9, 9, 9, 8};
        return {min_t[lvl - 1], max_t[lvl - 1]};
    }
    if (n == 9) {
        static constexpr int min_t[9] = {36, 30, 30, 25, 25, 22, 22, 22, 17};
        static constexpr int max_t[9] = {45, 35, 35, 29, 29, 25, 25, 25, 24};
        return {min_t[lvl - 1], max_t[lvl - 1]};
    }
    if (n == 16) {
        static constexpr int min_t[9] = {135, 110, 110, 90, 90, 75, 75, 75, 64};
        static constexpr int max_t[9] = {160, 134, 134, 109, 109, 89, 89, 89, 82};
        return {min_t[lvl - 1], max_t[lvl - 1]};
    }

    static constexpr double min_ratio[9] = {0.44, 0.38, 0.35, 0.31, 0.28, 0.24, 0.22, 0.20, 0.16};
    static constexpr double max_ratio[9] = {0.62, 0.56, 0.52, 0.47, 0.43, 0.39, 0.36, 0.33, 0.28};
    const int nn = n * n;
    int min_c = static_cast<int>(std::floor(static_cast<double>(nn) * min_ratio[lvl - 1]));
    int max_c = static_cast<int>(std::ceil(static_cast<double>(nn) * max_ratio[lvl - 1]));
    const int hard_floor = std::min(nn, n);
    min_c = std::clamp(min_c, hard_floor, nn);
    max_c = std::clamp(max_c, min_c, nn);
    return {min_c, max_c};
}

ClueRange resolve_auto_clue_range(int box_rows, int box_cols, int level, RequiredStrategy strategy) {
    const int safe_box_rows = std::max(1, box_rows);
    const int safe_box_cols = std::max(1, box_cols);
    const int n = safe_box_rows * safe_box_cols;
    const int nn = n * n;
    const int adjusted_level = strategy_adjusted_level(level, strategy);
    ClueRange out = clue_range_for_size_level(n, adjusted_level);
    const double aspect = static_cast<double>(std::max(safe_box_rows, safe_box_cols)) / static_cast<double>(std::min(safe_box_rows, safe_box_cols));
    const double clues_scale = std::clamp(1.0 + 0.05 * (aspect - 1.0), 1.0, 1.30);
    out.min_clues = std::clamp(static_cast<int>(std::llround(static_cast<double>(out.min_clues) * clues_scale)), 0, nn);
    out.max_clues = std::clamp(static_cast<int>(std::llround(static_cast<double>(out.max_clues) * clues_scale)), out.min_clues, nn);
    return out;
}

inline double suggest_time_budget_base_s(int n) {
    if (n <= 6) return 0.5;
    if (n == 9) return 2.0;
    if (n == 12) return 5.0;
    if (n == 16) return 10.0;
    if (n == 20) return 30.0;
    if (n == 25) return 60.0;
    if (n >= 30) return 120.0;
    return static_cast<double>(n * n) / 4.0;
}

double suggest_time_budget_s(int box_rows, int box_cols, int level) {
    const int safe_box_rows = std::max(1, box_rows);
    const int safe_box_cols = std::max(1, box_cols);
    const int n = safe_box_rows * safe_box_cols;
    const int lvl = std::clamp(level, 1, 9);
    const double level_scale = 1.0 + 0.08 * static_cast<double>(lvl - 1);
    const double asymmetry_scale = std::clamp(1.0 + 0.30 * (asymmetry_ratio_for_geometry(safe_box_rows, safe_box_cols) - 1.0), 1.0, 3.0);
    double high_level_scale = 1.0;
    if (lvl >= 8) high_level_scale = 1.80;
    else if (lvl == 7) high_level_scale = 1.40;
    else if (lvl == 6) high_level_scale = 1.20;

    double large_grid_scale = 1.0;
    if (n >= 36) large_grid_scale = (lvl >= 8) ? 4.00 : 2.50;
    else if (n >= 30) large_grid_scale = (lvl >= 8) ? 2.60 : 1.90;
    else if (n >= 25) large_grid_scale = 1.35;
    else if (n >= 20) large_grid_scale = 1.15;

    const double suggested = suggest_time_budget_base_s(n) * level_scale * asymmetry_scale * high_level_scale * large_grid_scale;
    return std::clamp(suggested, 0.05, 14400.0);
}

inline int suggest_attempt_time_budget_seconds(int box_rows, int box_cols, int level) {
    const double suggested = suggest_time_budget_s(box_rows, box_cols, level);
    const double rounded = std::ceil(std::max(1.0, suggested));
    return static_cast<int>(std::clamp(rounded, 1.0, 86400.0));
}

inline int suggest_reseed_interval_s(int box_rows, int box_cols, int level) {
    const int safe_box_rows = std::max(1, box_rows);
    const int safe_box_cols = std::max(1, box_cols);
    const int n = safe_box_rows * safe_box_cols;
    const int lvl = std::clamp(level, 1, 9);

    int reseed_s = 0;
    if (n >= 36) reseed_s = (lvl >= 8) ? 20 : 12;
    else if (n >= 30) reseed_s = (lvl >= 8) ? 14 : 10;
    else if (n >= 25) reseed_s = 8;
    else if (n >= 16) reseed_s = 5;
    else if (n >= 12) reseed_s = 3;

    const double asymmetry = asymmetry_ratio_for_geometry(safe_box_rows, safe_box_cols);
    if (reseed_s > 0 && asymmetry >= 2.5) {
        reseed_s += 2;
    }
    return reseed_s;
}

inline uint64_t suggest_attempt_node_budget(int box_rows, int box_cols, int level) {
    const int safe_box_rows = std::max(1, box_rows);
    const int safe_box_cols = std::max(1, box_cols);
    const int n = safe_box_rows * safe_box_cols;
    const int nn = n * n;
    const int lvl = std::clamp(level, 1, 9);
    const double asymmetry = asymmetry_ratio_for_geometry(safe_box_rows, safe_box_cols);

    const double base_nodes = std::clamp(static_cast<double>(nn) * 4000.0, 200000.0, 25000000.0);
    const double level_scale = 1.0 + 0.22 * static_cast<double>(lvl - 1);
    const double asymmetry_scale = std::clamp(1.0 + 0.25 * (asymmetry - 1.0), 1.0, 3.0);

    double large_grid_scale = 1.0;
    if (n >= 36) large_grid_scale = (lvl >= 8) ? 4.00 : 2.50;
    else if (n >= 30) large_grid_scale = (lvl >= 8) ? 2.70 : 1.90;
    else if (n >= 25) large_grid_scale = 1.35;

    double suggested = base_nodes * level_scale * asymmetry_scale * large_grid_scale;
    if (n >= 36 && lvl >= 8) suggested = std::max(suggested, 100000000.0);
    else if (n >= 36) suggested = std::max(suggested, 45000000.0);
    else if (n >= 30) suggested = std::max(suggested, 25000000.0);
    else if (n >= 25) suggested = std::max(suggested, 12000000.0);

    suggested = std::clamp(suggested, 200000.0, 500000000.0);
    return static_cast<uint64_t>(std::llround(suggested));
}

inline GenerationProfile resolve_generation_profile(
    int box_rows,
    int box_cols,
    int level,
    RequiredStrategy strategy,
    const GenerateRunConfig* cfg_override) {
    GenerationProfile profile{};
    const int safe_box_rows = std::max(1, box_rows);
    const int safe_box_cols = std::max(1, box_cols);
    const int n = safe_box_rows * safe_box_cols;
    profile.is_symmetric = geometria::is_symmetric_geometry(safe_box_rows, safe_box_cols);
    profile.asymmetry_ratio = asymmetry_ratio_for_geometry(safe_box_rows, safe_box_cols);
    profile.clue_range = resolve_auto_clue_range(safe_box_rows, safe_box_cols, level, strategy);
    profile.suggested_budget_s = suggest_time_budget_s(safe_box_rows, safe_box_cols, level);

    const std::string policy = normalize_profile_mode_policy(cfg_override != nullptr ? cfg_override->profile_mode_policy : "adaptive");
    const int full_for_n_ge = std::max(4, cfg_override != nullptr ? cfg_override->full_for_n_ge : 25);

    if (policy == "full") {
        profile.mode = GenerationMode::Full;
        profile.reason = "policy=full";
    } else if (policy == "legacy") {
        if (n <= 12) { profile.mode = GenerationMode::Full; profile.reason = "legacy:n<=12"; }
        else if (n <= 24) { profile.mode = GenerationMode::Lite; profile.reason = "legacy:13<=n<=24"; }
        else { profile.mode = GenerationMode::TopologyOnly; profile.reason = "legacy:n>=25"; }
    } else {
        if (n <= 12) { profile.mode = GenerationMode::Full; profile.reason = "adaptive:n<=12"; }
        else if (n < full_for_n_ge) { profile.mode = GenerationMode::Lite; profile.reason = "adaptive:n<full_for_n_ge"; }
        else { profile.mode = GenerationMode::Full; profile.reason = "adaptive:n>=full_for_n_ge"; }
    }
    if (strategy == RequiredStrategy::Backtracking && n <= 20) {
        profile.mode = GenerationMode::Full;
        profile.reason = "required_strategy=Backtracking";
    }
    return profile;
}

struct ParseArgsResult {
    GenerateRunConfig cfg;
    std::set<std::string> arg_used;
    bool benchmark_mode = false;
    bool list_geometries = false;
    bool validate_geometry = false;
    bool validate_geometry_catalog = false;
    bool run_geometry_gate = false;
    bool run_quality_benchmark = false;
    bool run_pre_difficulty_gate = false;
    bool run_asym_pair_benchmark = false;
    bool run_vip_benchmark = false;
    bool run_vip_gate = false;
    bool explain_profile = false;
    std::string geometry_gate_report = "plikiTMP/porownania/geometry_gate_report.txt";
    std::string quality_benchmark_report = "plikiTMP/porownania/quality_benchmark_pre_diff.txt";
    std::string pre_difficulty_gate_report = "plikiTMP/porownania/pre_difficulty_gate.txt";
    std::string asym_pair_benchmark_report = "plikiTMP/porownania/asym_pair_benchmark.txt";
    std::string vip_benchmark_report = "plikiTMP/porownania/vip_benchmark.txt";
    std::string vip_gate_report = "plikiTMP/porownania/vip_gate.txt";
    int quality_benchmark_max_cases = 0;
};

ParseArgsResult parse_args(int argc, char** argv) {
    GenerateRunConfig cfg;
    bool min_clues_set = false;
    bool max_clues_set = false;
    ParseArgsResult res;
    
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        res.arg_used.insert(arg);
        auto read_value = [&](auto& out) {
            if (i + 1 < argc) {
                std::istringstream iss(argv[++i]);
                iss >> out;
            }
        };
        if (arg == "--target" || arg == "--target_puzzles") read_value(cfg.target_puzzles);
        else if (arg == "--box-rows" || arg == "--box_rows") read_value(cfg.box_rows);
        else if (arg == "--box-cols" || arg == "--box_cols") read_value(cfg.box_cols);
        else if (arg == "--threads") read_value(cfg.threads);
        else if (arg == "--seed") read_value(cfg.seed);
        else if (arg == "--reseed-interval-s" || arg == "--reseed_interval_s") read_value(cfg.reseed_interval_s);
        else if (arg == "--attempt-time-budget-s" || arg == "--attempt_time_budget_s") read_value(cfg.attempt_time_budget_s);
        else if (arg == "--attempt-node-budget" || arg == "--attempt_node_budget_s") read_value(cfg.attempt_node_budget);
        else if (arg == "--quality-contract-off") cfg.enable_quality_contract = false;
        else if (arg == "--quality-contract-on") cfg.enable_quality_contract = true;
        else if (arg == "--replay-validate-off") cfg.enable_replay_validation = false;
        else if (arg == "--replay-validate-on") cfg.enable_replay_validation = true;
        else if (arg == "--distribution-filter-off") cfg.enable_distribution_filter = false;
        else if (arg == "--distribution-filter-on") cfg.enable_distribution_filter = true;
        else if (arg == "--uniqueness-confirm-budget-s" || arg == "--uniqueness_confirm_budget_s") read_value(cfg.uniqueness_confirm_budget_s);
        else if (arg == "--uniqueness-confirm-budget-nodes" || arg == "--uniqueness_confirm_budget_nodes") read_value(cfg.uniqueness_confirm_budget_nodes);
        else if ((arg == "--profile-mode-policy" || arg == "--profile_mode_policy") && i + 1 < argc) cfg.profile_mode_policy = normalize_profile_mode_policy(argv[++i]);
        else if (arg == "--full-for-n-ge" || arg == "--full_for_n_ge") read_value(cfg.full_for_n_ge);
        else if ((arg == "--cpu-backend" || arg == "--cpu_backend") && i + 1 < argc) cfg.cpu_backend_policy = normalize_cpu_backend_policy(argv[++i]);
        else if (arg == "--cpu-dispatch-report" || arg == "--cpu_dispatch_report") cfg.cpu_dispatch_report = true;
        else if ((arg == "--asym-heuristics" || arg == "--asym_heuristics") && i + 1 < argc) cfg.asym_heuristics_mode = normalize_asym_heuristics_mode(argv[++i]);
        else if (arg == "--adaptive-budget" || arg == "--adaptive_budget") cfg.adaptive_budget = true;
        else if (arg == "--adaptive-budget-off" || arg == "--adaptive_budget_off") cfg.adaptive_budget = false;
        else if (arg == "--vip-mode" || arg == "--vip_mode") cfg.vip_mode = true;
        else if (arg == "--vip-contract-strict" || arg == "--vip_contract_strict") cfg.vip_contract_strict = true;
        else if ((arg == "--difficulty-engine" || arg == "--difficulty_engine") && i + 1 < argc) cfg.difficulty_engine = normalize_difficulty_engine(argv[++i]);
        else if ((arg == "--vip-score-profile" || arg == "--vip_score_profile") && i + 1 < argc) cfg.vip_score_profile = normalize_vip_score_profile(argv[++i]);
        else if ((arg == "--vip-trace-level" || arg == "--vip_trace_level") && i + 1 < argc) cfg.vip_trace_level = normalize_vip_trace_level(argv[++i]);
        else if ((arg == "--vip-min-grade-by-geometry" || arg == "--vip_min_grade_by_geometry") && i + 1 < argc) cfg.vip_min_grade_by_geometry_path = argv[++i];
        else if ((arg == "--vip-grade-target" || arg == "--vip_grade_target") && i + 1 < argc) cfg.vip_grade_target = normalize_vip_grade_target(argv[++i]);
        else if ((arg == "--difficulty-trace-out" || arg == "--difficulty_trace_out") && i + 1 < argc) cfg.difficulty_trace_out = argv[++i];
        else if ((arg == "--vip-signature-out" || arg == "--vip_signature_out") && i + 1 < argc) cfg.vip_signature_out = argv[++i];
        else if (arg == "--perf-ab-suite" || arg == "--perf_ab_suite") cfg.perf_ab_suite = true;
        else if ((arg == "--perf-report-out" || arg == "--perf_report_out") && i + 1 < argc) cfg.perf_report_out = argv[++i];
        else if ((arg == "--perf-csv-out" || arg == "--perf_csv_out") && i + 1 < argc) cfg.perf_csv_out = argv[++i];
        else if ((arg == "--perf-baseline-csv" || arg == "--perf_baseline_csv") && i + 1 < argc) cfg.perf_baseline_csv = argv[++i];
        else if ((arg == "--stage-start" || arg == "--stage_start") && i + 1 < argc) { cfg.stage_start = true; cfg.stage_name = argv[++i]; }
        else if ((arg == "--stage-end" || arg == "--stage_end") && i + 1 < argc) { cfg.stage_end = true; cfg.stage_name = argv[++i]; }
        else if ((arg == "--stage-report-out" || arg == "--stage_report_out") && i + 1 < argc) cfg.stage_report_out = argv[++i];
        else if (arg == "--min-clues" || arg == "--min_clues") { read_value(cfg.min_clues); min_clues_set = cfg.min_clues > 0; }
        else if (arg == "--max-clues" || arg == "--max_clues") { read_value(cfg.max_clues); max_clues_set = cfg.max_clues > 0; }
        else if (arg == "--difficulty") read_value(cfg.difficulty_level_required);
        else if ((arg == "--required-strategy" || arg == "--required_strategy") && i + 1 < argc) cfg.required_strategy = parse_required_strategy(argv[++i]);
        else if (arg == "--no-unique") cfg.require_unique = false;
        else if (arg == "--no-strict-logical") cfg.strict_logical = false;
        else if (arg == "--symmetry-center") cfg.symmetry_center = true;
        else if ((arg == "--output-folder" || arg == "--output_folder") && i + 1 < argc) cfg.output_folder = argv[++i];
        else if ((arg == "--output-file" || arg == "--output_file") && i + 1 < argc) cfg.output_file = argv[++i];
        else if (arg == "--test-mode") { cfg.output_folder = "plikiTMP/testy"; cfg.output_file = "test_" + now_local_compact_string() + ".txt"; }
        else if (arg == "--max-attempts" || arg == "--max_attempts") read_value(cfg.max_attempts);
        else if (arg == "--time-limit-s") read_value(cfg.max_attempts_s);
        else if (arg == "--max-total-time-s" || arg == "--max_total_time_s") read_value(cfg.max_total_time_s);
        else if (arg == "--no-pause") cfg.pause_on_exit_windows = false;
        else if (arg == "--single-file-only") cfg.write_individual_files = false;
        else if (arg == "--benchmark-40s") { cfg.benchmark_profiles_40s = true; cfg.benchmark_seconds_per_profile = 40; res.arg_used.insert("--benchmark-mode"); }
        else if (arg == "--benchmark-seconds") { read_value(cfg.benchmark_seconds_per_profile); res.arg_used.insert("--benchmark-mode"); }
        else if (arg == "--benchmark-output" && i + 1 < argc) { cfg.benchmark_output_file = argv[++i]; res.arg_used.insert("--benchmark-mode"); }
        else if (arg == "--quality-gate-25s") { cfg.benchmark_profiles_40s = false; cfg.benchmark_seconds_per_profile = 25; cfg.benchmark_output_file = "plikiTMP/porownania/quality_gate_25s.txt"; res.arg_used.insert("--benchmark-mode"); }
        else if (arg == "--list-geometries") res.list_geometries = true;
        else if (arg == "--validate-geometry") res.validate_geometry = true;
        else if (arg == "--validate-geometry-catalog") res.validate_geometry_catalog = true;
        else if (arg == "--run-geometry-gate") res.run_geometry_gate = true;
        else if (arg == "--run-quality-benchmark") res.run_quality_benchmark = true;
        else if (arg == "--run-pre-difficulty-gate") res.run_pre_difficulty_gate = true;
        else if (arg == "--run-asym-pair-benchmark") res.run_asym_pair_benchmark = true;
        else if (arg == "--run-vip-benchmark") res.run_vip_benchmark = true;
        else if (arg == "--run-vip-gate") res.run_vip_gate = true;
        else if (arg == "--explain-profile") res.explain_profile = true;
        else if ((arg == "--geometry-gate-report" || arg == "--geometry_gate_report") && i + 1 < argc) res.geometry_gate_report = argv[++i];
        else if ((arg == "--quality-benchmark-report" || arg == "--quality_benchmark_report") && i + 1 < argc) res.quality_benchmark_report = argv[++i];
        else if ((arg == "--pre-difficulty-gate-report" || arg == "--pre_difficulty_gate_report") && i + 1 < argc) res.pre_difficulty_gate_report = argv[++i];
        else if ((arg == "--asym-pair-benchmark-report" || arg == "--asym_pair_benchmark_report") && i + 1 < argc) res.asym_pair_benchmark_report = argv[++i];
        else if ((arg == "--vip-benchmark-report" || arg == "--vip_benchmark_report") && i + 1 < argc) res.vip_benchmark_report = argv[++i];
        else if ((arg == "--vip-gate-report" || arg == "--vip_gate_report") && i + 1 < argc) res.vip_gate_report = argv[++i];
        else if (arg == "--quality-benchmark-max-cases" || arg == "--quality_benchmark_max_cases") read_value(res.quality_benchmark_max_cases);
        else if (arg == "--run-regression-tests") {
            std::cout << "Running regression tests...\n";
            sudoku_testy::run_all_regression_tests("plikiTMP/testy/regression_report.txt");
            std::exit(0);
        }
    }
    
    cfg.box_rows = std::max(1, cfg.box_rows);
    cfg.box_cols = std::max(1, cfg.box_cols);
    cfg.difficulty_level_required = std::clamp(cfg.difficulty_level_required, 1, 9);
    cfg.full_for_n_ge = std::clamp(cfg.full_for_n_ge, 4, 36);
    cfg.profile_mode_policy = normalize_profile_mode_policy(cfg.profile_mode_policy);
    cfg.cpu_backend_policy = normalize_cpu_backend_policy(cfg.cpu_backend_policy);
    cfg.asym_heuristics_mode = normalize_asym_heuristics_mode(cfg.asym_heuristics_mode);
    cfg.difficulty_engine = normalize_difficulty_engine(cfg.difficulty_engine);
    cfg.vip_score_profile = normalize_vip_score_profile(cfg.vip_score_profile);
    cfg.vip_trace_level = normalize_vip_trace_level(cfg.vip_trace_level);
    cfg.vip_grade_target = normalize_vip_grade_target(cfg.vip_grade_target);
    
    if (cfg.difficulty_level_required <= 1 && cfg.required_strategy == RequiredStrategy::None) {
        cfg.required_strategy = RequiredStrategy::NakedSingle;
    }
    
    const ClueRange auto_clues = resolve_auto_clue_range(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy);
    if (!min_clues_set) cfg.min_clues = auto_clues.min_clues;
    if (!max_clues_set) cfg.max_clues = auto_clues.max_clues;
    
    const int n = cfg.box_rows * cfg.box_cols;
    const int nn = n * n;
    cfg.min_clues = std::clamp(cfg.min_clues, 0, nn);
    cfg.max_clues = std::clamp(cfg.max_clues, cfg.min_clues, nn);
    
    res.benchmark_mode = (cfg.benchmark_profiles_40s || res.arg_used.count("--benchmark-mode") > 0);
    res.cfg = cfg;
    return res;
}

inline std::string supported_geometries_text() {
    std::ostringstream out;
    out << "Supported geometries (" << geometria::all_geometries().size() << "):\n";
    for (const auto& g : geometria::all_geometries()) {
        out << "  - " << g.n << "x" << g.n << " (" << g.box_rows << "x" << g.box_cols << ") [" << (g.is_symmetric ? "sym" : "asym") << "]\n";
    }
    return out.str();
}

inline bool print_geometry_validation(int box_rows, int box_cols, std::ostream& out) {
    const int n = box_rows * box_cols;
    const bool ok = geometria::is_supported_geometry(box_rows, box_cols);
    if (ok) {
        out << "Geometry OK: " << n << "x" << n << " (" << box_rows << "x" << box_cols << ") [" << geometria::geometry_class_name(geometria::classify_geometry(box_rows, box_cols)) << "]\n";
        return true;
    }
    out << "Geometry NOT supported: " << n << "x" << n << " (" << box_rows << "x" << box_cols << ")\n";
    const auto variants = geometria::geometries_for_n(n);
    if (!variants.empty()) {
        out << "Supported variants for N=" << n << ":\n";
        for (const auto& g : variants) out << "  - (" << g.box_rows << "x" << g.box_cols << ")\n";
    } else {
        out << "No supported variants for N=" << n << ".\n";
    }
    return false;
}

inline bool print_geometry_catalog_validation(std::ostream& out) {
    std::string details;
    const bool ok = geometria::validate_geometry_catalog(&details);
    if (ok) {
        out << "Geometry catalog OK: " << geometria::all_geometries().size() << " entries\n";
        return true;
    }
    out << "Geometry catalog INVALID\n" << details;
    return false;
}

inline std::string explain_generation_profile_text(const GenerateRunConfig& cfg) {
    const GenerationProfile profile = resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
    const RuntimeCpuContext cpu_ctx = resolve_runtime_cpu_context(cfg);
    const int n = std::max(1, cfg.box_rows) * std::max(1, cfg.box_cols);
    std::ostringstream out;
    out << "Generation profile for " << n << "x" << n << " (" << cfg.box_rows << "x" << cfg.box_cols << ")\n"
        << "  mode: " << generation_mode_name(profile.mode) << "\n"
        << "  class: " << (profile.is_symmetric ? "symmetric" : "asymmetric") << " (ratio=" << format_fixed(profile.asymmetry_ratio, 3) << ")\n"
        << "  suggested_clues: " << profile.clue_range.min_clues << "-" << profile.clue_range.max_clues << "\n"
        << "  suggested_attempt_budget_s: " << format_fixed(profile.suggested_budget_s, 2) << "\n"
        << "  profile_mode_policy: " << normalize_profile_mode_policy(cfg.profile_mode_policy) << "\n"
        << "  full_for_n_ge: " << cfg.full_for_n_ge << "\n"
        << "  cpu_backend_requested: " << normalize_cpu_backend_policy(cfg.cpu_backend_policy) << "\n"
        << "  cpu_backend_selected: " << cpu_ctx.selected_backend << " (avx2=" << (cpu_ctx.avx2_supported ? "1" : "0") << ", avx512=" << (cpu_ctx.avx512_supported ? "1" : "0") << ", reason=" << cpu_ctx.reason << ")\n"
        << "  difficulty_engine: " << normalize_difficulty_engine(cfg.difficulty_engine) << "\n"
        << "  vip_mode: " << (cfg.vip_mode ? "on" : "off") << "\n"
        << "  vip_score_profile: " << normalize_vip_score_profile(cfg.vip_score_profile) << "\n"
        << "  vip_trace_level: " << normalize_vip_trace_level(cfg.vip_trace_level) << "\n"
        << "  vip_grade_target: " << normalize_vip_grade_target(cfg.vip_grade_target) << "\n"
        << "  vip_grade_target_override_path: " << (cfg.vip_min_grade_by_geometry_path.empty() ? "(none)" : cfg.vip_min_grade_by_geometry_path) << "\n"
        << "  vip_contract_strict: " << (cfg.vip_contract_strict ? "on" : "off") << "\n"
        << "  asym_heuristics_mode: " << normalize_asym_heuristics_mode(cfg.asym_heuristics_mode) << "\n"
        << "  adaptive_budget: " << (cfg.adaptive_budget ? "on" : "off") << "\n"
        << "  perf_baseline_csv: " << (cfg.perf_baseline_csv.empty() ? "(none)" : cfg.perf_baseline_csv) << "\n"
        << "  quality_contract: " << (cfg.enable_quality_contract ? "on" : "off") << "\n"
        << "  replay_validation: " << (cfg.enable_replay_validation ? "on" : "off") << "\n"
        << "  distribution_filter: " << (cfg.enable_distribution_filter ? "on" : "off") << "\n"
        << "  uniqueness_confirm_budget_s: " << format_fixed(cfg.uniqueness_confirm_budget_s, 2) << "\n"
        << "  uniqueness_confirm_budget_nodes: " << cfg.uniqueness_confirm_budget_nodes << "\n"
        << "  reason: " << profile.reason << "\n";
    return out.str();
}

struct GeometryGateCaseRow { int n = 0; int box_rows = 0; int box_cols = 0; bool is_symmetric = false; bool recognized = false; bool topology_ok = false; std::string smoke_mode; std::string profile_reason; bool smoke_ok = false; bool timed_out = false; double elapsed_s = 0.0; std::string message; };
struct GeometryGateSummary { std::vector<GeometryGateCaseRow> rows; int contract_failed = 0; int smoke_failed = 0; int timeout_count = 0; };
inline std::string geometry_gate_csv_path_for(const std::string& report_txt) { std::filesystem::path p(report_txt); if (!p.has_extension()) p += ".txt"; std::filesystem::path csv = p; csv.replace_extension(".csv"); return csv.string(); }

inline GeometryGateCaseRow run_geometry_gate_case(const geometria::GeometrySpec& g, const GenerateRunConfig* base_cfg = nullptr) {
    GeometryGateCaseRow row{};
    row.n = g.n; row.box_rows = g.box_rows; row.box_cols = g.box_cols; row.is_symmetric = g.is_symmetric;
    row.recognized = geometria::czy_obslugiwana(g.n, g.box_rows, g.box_cols);
    if (!row.recognized) { row.message = "not_recognized"; return row; }

    const auto topo_opt = GenericTopology::build(g.box_rows, g.box_cols);
    row.topology_ok = topo_opt.has_value();
    if (!row.topology_ok) { row.message = "topology_build_failed"; return row; }
    
    const GenericTopology topo = *topo_opt;
    GenerateRunConfig profile_cfg{};
    profile_cfg.box_rows = g.box_rows; profile_cfg.box_cols = g.box_cols;
    if (base_cfg != nullptr) { profile_cfg.profile_mode_policy = base_cfg->profile_mode_policy; profile_cfg.full_for_n_ge = base_cfg->full_for_n_ge; }
    
    const GenerationProfile profile = resolve_generation_profile(g.box_rows, g.box_cols, 1, RequiredStrategy::NakedSingle, &profile_cfg);
    row.smoke_mode = generation_mode_name(profile.mode);
    row.profile_reason = profile.reason;

    const auto t0 = std::chrono::steady_clock::now();

    if (profile.mode == GenerationMode::Full) {
        GenerateRunConfig cfg{};
        cfg.box_rows = g.box_rows; cfg.box_cols = g.box_cols; cfg.target_puzzles = 1; cfg.threads = 1;
        cfg.seed = static_cast<long long>(g.n * 100000 + g.box_rows * 100 + g.box_cols);
        cfg.required_strategy = RequiredStrategy::None; cfg.difficulty_level_required = 1;
        cfg.pause_on_exit_windows = false; cfg.write_individual_files = false;
        cfg.output_folder = "plikiTMP/testy/geometry_gate"; cfg.output_file = "gate_" + std::to_string(g.n) + "_" + std::to_string(g.box_rows) + "x" + std::to_string(g.box_cols) + ".txt";
        cfg.max_total_time_s = 20; cfg.min_clues = profile.clue_range.min_clues; cfg.max_clues = profile.clue_range.max_clues;
        cfg.profile_mode_policy = profile_cfg.profile_mode_policy; cfg.full_for_n_ge = profile_cfg.full_for_n_ge;
        GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);
        row.elapsed_s = result.elapsed_s;
        row.timed_out = (result.elapsed_s + 0.05 >= 20.0) && result.accepted == 0;
        row.smoke_ok = result.accepted > 0 || result.written > 0;
        row.message = row.smoke_ok ? "ok accepted=" + std::to_string(result.accepted) : (row.timed_out ? "timeout_20s" : "accepted=0 attempts=" + std::to_string(result.attempts));
        return row;
    }

    if (profile.mode == GenerationMode::Lite) {
        GenerateRunConfig cfg{};
        cfg.box_rows = g.box_rows; cfg.box_cols = g.box_cols; cfg.target_puzzles = 1; cfg.threads = 1;
        cfg.seed = static_cast<long long>(g.n * 100000 + g.box_rows * 100 + g.box_cols);
        cfg.required_strategy = RequiredStrategy::None; cfg.difficulty_level_required = 1;
        cfg.require_unique = false; cfg.strict_logical = false; cfg.pause_on_exit_windows = false; cfg.write_individual_files = false;
        cfg.output_folder = "plikiTMP/testy/geometry_gate"; cfg.output_file = "gate_lite_" + std::to_string(g.n) + "_" + std::to_string(g.box_rows) + "x" + std::to_string(g.box_cols) + ".txt";
        cfg.max_total_time_s = 20; cfg.min_clues = profile.clue_range.min_clues; cfg.max_clues = profile.clue_range.max_clues;
        cfg.profile_mode_policy = profile_cfg.profile_mode_policy; cfg.full_for_n_ge = profile_cfg.full_for_n_ge;
        GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);
        row.elapsed_s = result.elapsed_s;
        row.timed_out = (result.elapsed_s + 0.05 >= 20.0) && result.accepted == 0;
        row.smoke_ok = result.accepted > 0 || (result.attempts > 0 && result.rejected > 0);
        row.message = row.smoke_ok ? "ok attempts=" + std::to_string(result.attempts) : (row.timed_out ? "timeout_20s" : "no_activity attempts=" + std::to_string(result.attempts));
        return row;
    }

    GenericSolvedKernel solved;
    std::mt19937_64 rng(static_cast<uint64_t>(g.n * 100000 + g.box_rows * 100 + g.box_cols));
    std::vector<uint16_t> solution;
    SearchAbortControl budget;
    budget.time_enabled = true;
    budget.deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(2.0));
    budget.node_enabled = true; budget.node_limit = 500000ULL;
    const bool solved_ok = solved.generate(topo, rng, solution, &budget);
    row.elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    row.timed_out = budget.aborted_by_time;
    row.smoke_ok = solved_ok || budget.aborted_by_time || budget.aborted_by_nodes;
    row.message = row.smoke_ok ? (solved_ok ? "ok solved_kernel" : "ok budget_guard") : "core_smoke_failed";
    return row;
}

inline GeometryGateSummary run_geometry_gate_20s_suite(const GenerateRunConfig* base_cfg = nullptr) {
    GeometryGateSummary summary{};
    const auto& geometries = geometria::all_geometries();
    summary.rows.reserve(geometries.size());
    for (const auto& g : geometries) {
        GeometryGateCaseRow row = run_geometry_gate_case(g, base_cfg);
        if (!row.recognized || !row.topology_ok) ++summary.contract_failed;
        if (!row.smoke_ok) ++summary.smoke_failed;
        if (row.timed_out) ++summary.timeout_count;
        summary.rows.push_back(std::move(row));
    }
    return summary;
}

inline void write_geometry_gate_reports(const GeometryGateSummary& summary, const std::string& report_txt) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_txt).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(geometry_gate_csv_path_for(report_txt)).parent_path(), ec);

    std::ofstream txt(report_txt, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== Geometry Gate (20s per case) ===\ntotal=" << summary.rows.size() << " contract_failed=" << summary.contract_failed << " smoke_failed=" << summary.smoke_failed << " timeout_count=" << summary.timeout_count << "\n\n";
        for (const auto& r : summary.rows) {
            txt << r.n << "x" << r.n << " (" << r.box_rows << "x" << r.box_cols << ") class=" << (r.is_symmetric ? "sym" : "asym") << " recognized=" << (r.recognized ? "1" : "0") << " topology=" << (r.topology_ok ? "1" : "0") << " mode=" << r.smoke_mode << " reason=" << r.profile_reason << " smoke=" << (r.smoke_ok ? "1" : "0") << " timeout=" << (r.timed_out ? "1" : "0") << " elapsed_s=" << format_fixed(r.elapsed_s, 3) << " msg=" << r.message << "\n";
        }
    }

    std::ofstream csv(geometry_gate_csv_path_for(report_txt), std::ios::out | std::ios::trunc);
    if (csv) {
        csv << "n,box_rows,box_cols,is_symmetric,recognized,topology_ok,smoke_mode,profile_reason,smoke_ok,timed_out,elapsed_s,message\n";
        for (const auto& r : summary.rows) {
            std::string msg = r.message; std::string reason = r.profile_reason;
            std::replace(msg.begin(), msg.end(), ',', ';'); std::replace(reason.begin(), reason.end(), ',', ';');
            csv << r.n << "," << r.box_rows << "," << r.box_cols << "," << (r.is_symmetric ? 1 : 0) << "," << (r.recognized ? 1 : 0) << "," << (r.topology_ok ? 1 : 0) << "," << r.smoke_mode << "," << reason << "," << (r.smoke_ok ? 1 : 0) << "," << (r.timed_out ? 1 : 0) << "," << format_fixed(r.elapsed_s, 6) << "," << msg << "\n";
        }
    }
}

inline int run_geometry_gate_cli(const std::string& report_txt, const GenerateRunConfig* base_cfg = nullptr) {
    const GeometryGateSummary summary = run_geometry_gate_20s_suite(base_cfg);
    write_geometry_gate_reports(summary, report_txt);
    std::cout << "Geometry gate: total=" << summary.rows.size() << " contract_failed=" << summary.contract_failed << " smoke_failed=" << summary.smoke_failed << " timeout_count=" << summary.timeout_count << "\nReports: " << report_txt << " | " << geometry_gate_csv_path_for(report_txt) << "\n";
    if (summary.contract_failed > 0) return 1;
    if (summary.timeout_count > 0) return 3;
    if (summary.smoke_failed > 0) return 2;
    return 0;
}

struct QualityBenchmarkCaseRow { int n = 0; int box_rows = 0; int box_cols = 0; bool is_symmetric = false; std::string mode; uint64_t attempts = 0; uint64_t accepted = 0; uint64_t rejected = 0; uint64_t reject_replay = 0; uint64_t reject_distribution_bias = 0; uint64_t reject_uniqueness_budget = 0; double elapsed_s = 0.0; double quality_pass_rate_pct = 0.0; };
struct QualityBenchmarkSummary { std::vector<QualityBenchmarkCaseRow> rows; int pair_checked = 0; int pair_violations = 0; };
inline std::string quality_benchmark_csv_path_for(const std::string& report_txt) { std::filesystem::path p(report_txt); if (!p.has_extension()) p += ".txt"; std::filesystem::path csv = p; csv.replace_extension(".csv"); return csv.string(); }

inline QualityBenchmarkSummary run_quality_benchmark_suite_20s(int max_cases = 0, const GenerateRunConfig* base_cfg = nullptr) {
    QualityBenchmarkSummary summary{};
    std::vector<geometria::GeometrySpec> selected;
    std::map<int, std::vector<geometria::GeometrySpec>> by_n;
    for (const auto& g : geometria::all_geometries()) { if (g.n >= 16) by_n[g.n].push_back(g); }
    for (auto& kv : by_n) {
        auto& geos = kv.second;
        auto sym_it = std::find_if(geos.begin(), geos.end(), [](const geometria::GeometrySpec& g) { return g.box_rows == g.box_cols; });
        if (sym_it != geos.end()) selected.push_back(*sym_it);
        for (const auto& g : geos) {
            if (g.box_rows < g.box_cols && geometria::is_supported_geometry(g.box_cols, g.box_rows)) {
                selected.push_back(g); selected.push_back({g.n, g.box_cols, g.box_rows, false, ""}); break;
            }
        }
    }
    std::sort(selected.begin(), selected.end(), [](const auto& a, const auto& b) { if (a.n != b.n) return a.n < b.n; if (a.box_rows != b.box_rows) return a.box_rows < b.box_rows; return a.box_cols < b.box_cols; });
    selected.erase(std::unique(selected.begin(), selected.end(), [](const auto& a, const auto& b) { return a.n == b.n && a.box_rows == b.box_rows && a.box_cols == b.box_cols; }), selected.end());

    int executed = 0;
    for (const auto& g : selected) {
        if (max_cases > 0 && executed >= max_cases) break;
        GenerateRunConfig cfg{};
        cfg.box_rows = g.box_rows; cfg.box_cols = g.box_cols; cfg.target_puzzles = 1; cfg.threads = 1;
        cfg.seed = static_cast<long long>(g.n * 100000 + g.box_rows * 100 + g.box_cols + 77);
        cfg.required_strategy = RequiredStrategy::None; cfg.difficulty_level_required = 1;
        cfg.require_unique = true; cfg.strict_logical = true; cfg.enable_quality_contract = true; cfg.enable_replay_validation = true; cfg.enable_distribution_filter = true;
        cfg.pause_on_exit_windows = false; cfg.write_individual_files = false; cfg.output_folder = "plikiTMP/testy/quality_benchmark"; cfg.output_file = "qb_" + std::to_string(g.n) + "_" + std::to_string(g.box_rows) + "x" + std::to_string(g.box_cols) + ".txt";
        cfg.max_total_time_s = 20;
        if (base_cfg != nullptr) { cfg.profile_mode_policy = base_cfg->profile_mode_policy; cfg.full_for_n_ge = base_cfg->full_for_n_ge; }
        const GenerationProfile profile = resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
        cfg.min_clues = profile.clue_range.min_clues; cfg.max_clues = profile.clue_range.max_clues;
        const GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);

        QualityBenchmarkCaseRow row{};
        row.n = g.n; row.box_rows = g.box_rows; row.box_cols = g.box_cols; row.is_symmetric = (g.box_rows == g.box_cols);
        row.mode = generation_mode_name(profile.mode); row.attempts = result.attempts; row.accepted = result.accepted; row.rejected = result.rejected;
        row.reject_replay = result.reject_replay; row.reject_distribution_bias = result.reject_distribution_bias; row.reject_uniqueness_budget = result.reject_uniqueness_budget;
        row.elapsed_s = result.elapsed_s; row.quality_pass_rate_pct = row.attempts > 0 ? (100.0 * static_cast<double>(row.accepted) / static_cast<double>(row.attempts)) : 0.0;
        summary.rows.push_back(std::move(row));
        ++executed;
    }

    std::map<std::tuple<int, int, int>, std::array<const QualityBenchmarkCaseRow*, 2>> pairs;
    for (const auto& row : summary.rows) {
        if (row.box_rows == row.box_cols) continue;
        const int a = std::min(row.box_rows, row.box_cols); const int b = std::max(row.box_rows, row.box_cols);
        auto& slot = pairs[std::make_tuple(row.n, a, b)];
        if (row.box_rows <= row.box_cols) slot[0] = &row; else slot[1] = &row;
    }
    for (const auto& kv : pairs) {
        const auto* p0 = kv.second[0]; const auto* p1 = kv.second[1];
        if (p0 == nullptr || p1 == nullptr) continue;
        ++summary.pair_checked;
        if (std::abs(p0->quality_pass_rate_pct - p1->quality_pass_rate_pct) > 35.0) ++summary.pair_violations;
    }
    return summary;
}

inline void write_quality_benchmark_reports(const QualityBenchmarkSummary& summary, const std::string& report_txt) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_txt).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(quality_benchmark_csv_path_for(report_txt)).parent_path(), ec);

    std::ofstream txt(report_txt, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== Quality Benchmark (pre-difficulty, 20s/case) ===\nrows=" << summary.rows.size() << " pair_checked=" << summary.pair_checked << " pair_violations=" << summary.pair_violations << "\n\n";
        for (const auto& r : summary.rows) txt << r.n << "x" << r.n << " (" << r.box_rows << "x" << r.box_cols << ") class=" << (r.is_symmetric ? "sym" : "asym") << " mode=" << r.mode << " accepted=" << r.accepted << " attempts=" << r.attempts << " rejected=" << r.rejected << " reject_replay=" << r.reject_replay << " reject_dist=" << r.reject_distribution_bias << " reject_uniq_budget=" << r.reject_uniqueness_budget << " quality_pass_rate=" << format_fixed(r.quality_pass_rate_pct, 2) << "% elapsed_s=" << format_fixed(r.elapsed_s, 3) << "\n";
    }

    std::ofstream csv(quality_benchmark_csv_path_for(report_txt), std::ios::out | std::ios::trunc);
    if (csv) {
        csv << "n,box_rows,box_cols,is_symmetric,mode,attempts,accepted,rejected,reject_replay,reject_distribution_bias,reject_uniqueness_budget,quality_pass_rate_pct,elapsed_s\n";
        for (const auto& r : summary.rows) csv << r.n << "," << r.box_rows << "," << r.box_cols << "," << (r.is_symmetric ? 1 : 0) << "," << r.mode << "," << r.attempts << "," << r.accepted << "," << r.rejected << "," << r.reject_replay << "," << r.reject_distribution_bias << "," << r.reject_uniqueness_budget << "," << format_fixed(r.quality_pass_rate_pct, 6) << "," << format_fixed(r.elapsed_s, 6) << "\n";
    }
}

inline int run_quality_benchmark_cli(const std::string& report_txt, int max_cases = 0, const GenerateRunConfig* base_cfg = nullptr) {
    const QualityBenchmarkSummary summary = run_quality_benchmark_suite_20s(max_cases, base_cfg);
    write_quality_benchmark_reports(summary, report_txt);
    std::cout << "Quality benchmark: rows=" << summary.rows.size() << " pair_checked=" << summary.pair_checked << " pair_violations=" << summary.pair_violations << "\nReports: " << report_txt << " | " << quality_benchmark_csv_path_for(report_txt) << "\n";
    return summary.pair_violations > 0 ? 4 : 0;
}

struct AsymPairBenchmarkRow { int n = 0; int a = 0; int b = 0; uint64_t attempts_ab = 0; uint64_t attempts_ba = 0; uint64_t accepted_ab = 0; uint64_t accepted_ba = 0; double elapsed_ab = 0.0; double elapsed_ba = 0.0; double throughput_ab = 0.0; double throughput_ba = 0.0; double throughput_delta_pct = 0.0; double pass_rate_ab = 0.0; double pass_rate_ba = 0.0; double pass_rate_delta = 0.0; bool violation = false; };
struct AsymPairBenchmarkSummary { std::vector<AsymPairBenchmarkRow> rows; int pairs_checked = 0; int violations = 0; double avg_throughput_delta_pct = 0.0; };
inline std::string asym_pair_benchmark_csv_path_for(const std::string& report_txt) { std::filesystem::path p(report_txt); if (!p.has_extension()) p += ".txt"; std::filesystem::path csv = p; csv.replace_extension(".csv"); return csv.string(); }

inline AsymPairBenchmarkSummary run_asym_pair_benchmark_suite(int max_cases = 0, const GenerateRunConfig* base_cfg = nullptr) {
    AsymPairBenchmarkSummary summary{};
    std::set<std::tuple<int, int, int>> pairs;
    for (const auto& g : geometria::all_geometries()) {
        if (g.box_rows == g.box_cols) continue;
        const int a = std::min(g.box_rows, g.box_cols); const int b = std::max(g.box_rows, g.box_cols);
        if (geometria::is_supported_geometry(a, b) && geometria::is_supported_geometry(b, a)) pairs.insert(std::make_tuple(g.n, a, b));
    }

    int executed = 0; double delta_sum = 0.0;
    for (const auto& pair : pairs) {
        if (max_cases > 0 && executed >= max_cases) break;
        const int n = std::get<0>(pair); const int a = std::get<1>(pair); const int b = std::get<2>(pair);

        auto run_case = [&](int br, int bc, uint64_t seed_bias) -> GenerateRunResult {
            GenerateRunConfig cfg{};
            cfg.box_rows = br; cfg.box_cols = bc; cfg.target_puzzles = 1; cfg.threads = 1;
            cfg.seed = static_cast<long long>(n * 100000 + br * 100 + bc + seed_bias);
            cfg.required_strategy = RequiredStrategy::None; cfg.difficulty_level_required = 1;
            cfg.require_unique = true; cfg.strict_logical = true; cfg.enable_quality_contract = true; cfg.enable_replay_validation = true; cfg.enable_distribution_filter = true;
            cfg.pause_on_exit_windows = false; cfg.write_individual_files = false; cfg.output_folder = "plikiTMP/testy/asym_pair_benchmark"; cfg.output_file = "asym_" + std::to_string(n) + "_" + std::to_string(br) + "x" + std::to_string(bc) + ".txt";
            cfg.max_total_time_s = 20;
            if (base_cfg != nullptr) { cfg.profile_mode_policy = base_cfg->profile_mode_policy; cfg.full_for_n_ge = base_cfg->full_for_n_ge; cfg.cpu_backend_policy = base_cfg->cpu_backend_policy; cfg.asym_heuristics_mode = base_cfg->asym_heuristics_mode; cfg.adaptive_budget = base_cfg->adaptive_budget; }
            const GenerationProfile profile = resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
            cfg.min_clues = profile.clue_range.min_clues; cfg.max_clues = profile.clue_range.max_clues;
            return run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);
        };

        const GenerateRunResult ab = run_case(a, b, 401);
        const GenerateRunResult ba = run_case(b, a, 402);

        AsymPairBenchmarkRow row{};
        row.n = n; row.a = a; row.b = b; row.attempts_ab = ab.attempts; row.attempts_ba = ba.attempts; row.accepted_ab = ab.accepted; row.accepted_ba = ba.accepted;
        row.elapsed_ab = ab.elapsed_s; row.elapsed_ba = ba.elapsed_s; row.throughput_ab = ab.accepted_per_sec; row.throughput_ba = ba.accepted_per_sec;
        const double base_tp = std::max(0.001, std::max(row.throughput_ab, row.throughput_ba));
        row.throughput_delta_pct = std::abs(row.throughput_ab - row.throughput_ba) * 100.0 / base_tp;
        row.pass_rate_ab = row.attempts_ab > 0 ? (100.0 * static_cast<double>(row.accepted_ab) / static_cast<double>(row.attempts_ab)) : 0.0;
        row.pass_rate_ba = row.attempts_ba > 0 ? (100.0 * static_cast<double>(row.accepted_ba) / static_cast<double>(row.attempts_ba)) : 0.0;
        row.pass_rate_delta = std::abs(row.pass_rate_ab - row.pass_rate_ba);
        row.violation = (row.throughput_delta_pct > 50.0) || (row.pass_rate_delta > 35.0);
        if (row.violation) ++summary.violations;
        delta_sum += row.throughput_delta_pct;
        summary.rows.push_back(std::move(row));
        ++summary.pairs_checked; ++executed;
    }
    summary.avg_throughput_delta_pct = summary.rows.empty() ? 0.0 : (delta_sum / static_cast<double>(summary.rows.size()));
    return summary;
}

inline void write_asym_pair_benchmark_reports(const AsymPairBenchmarkSummary& summary, const std::string& report_txt) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_txt).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(asym_pair_benchmark_csv_path_for(report_txt)).parent_path(), ec);

    std::ofstream txt(report_txt, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== Asym Pair Benchmark ===\npairs_checked=" << summary.pairs_checked << " violations=" << summary.violations << " avg_throughput_delta_pct=" << format_fixed(summary.avg_throughput_delta_pct, 3) << "\n\n";
        for (const auto& r : summary.rows) txt << r.n << "x" << r.n << " (" << r.a << "x" << r.b << " vs " << r.b << "x" << r.a << ") tp_ab=" << format_fixed(r.throughput_ab, 6) << " tp_ba=" << format_fixed(r.throughput_ba, 6) << " tp_delta_pct=" << format_fixed(r.throughput_delta_pct, 3) << " pass_ab=" << format_fixed(r.pass_rate_ab, 3) << " pass_ba=" << format_fixed(r.pass_rate_ba, 3) << " pass_delta=" << format_fixed(r.pass_rate_delta, 3) << " violation=" << (r.violation ? "1" : "0") << "\n";
    }

    std::ofstream csv(asym_pair_benchmark_csv_path_for(report_txt), std::ios::out | std::ios::trunc);
    if (csv) {
        csv << "n,a,b,attempts_ab,attempts_ba,accepted_ab,accepted_ba,elapsed_ab,elapsed_ba,throughput_ab,throughput_ba,throughput_delta_pct,pass_rate_ab,pass_rate_ba,pass_rate_delta,violation\n";
        for (const auto& r : summary.rows) csv << r.n << "," << r.a << "," << r.b << "," << r.attempts_ab << "," << r.attempts_ba << "," << r.accepted_ab << "," << r.accepted_ba << "," << format_fixed(r.elapsed_ab, 6) << "," << format_fixed(r.elapsed_ba, 6) << "," << format_fixed(r.throughput_ab, 6) << "," << format_fixed(r.throughput_ba, 6) << "," << format_fixed(r.throughput_delta_pct, 6) << "," << format_fixed(r.pass_rate_ab, 6) << "," << format_fixed(r.pass_rate_ba, 6) << "," << format_fixed(r.pass_rate_delta, 6) << "," << (r.violation ? 1 : 0) << "\n";
    }
}

inline int run_asym_pair_benchmark_cli(const std::string& report_txt, int max_cases = 0, const GenerateRunConfig* base_cfg = nullptr) {
    const AsymPairBenchmarkSummary summary = run_asym_pair_benchmark_suite(max_cases, base_cfg);
    write_asym_pair_benchmark_reports(summary, report_txt);
    std::cout << "Asym pair benchmark: pairs_checked=" << summary.pairs_checked << " violations=" << summary.violations << " avg_throughput_delta_pct=" << format_fixed(summary.avg_throughput_delta_pct, 3) << "\nReports: " << report_txt << " | " << asym_pair_benchmark_csv_path_for(report_txt) << "\n";
    return summary.violations > 0 ? 8 : 0;
}

struct PreDifficultyGateSummary { GeometryGateSummary geometry; QualityBenchmarkSummary quality; int throughput_pair_violations = 0; double uniqueness_budget_pct = 0.0; bool passed = false; std::string message; };
inline int count_quality_throughput_pair_violations(const QualityBenchmarkSummary& summary, double max_delta_pct = 50.0) {
    std::map<std::tuple<int, int, int>, std::array<const QualityBenchmarkCaseRow*, 2>> pairs;
    for (const auto& row : summary.rows) {
        if (row.box_rows == row.box_cols) continue;
        const int a = std::min(row.box_rows, row.box_cols); const int b = std::max(row.box_rows, row.box_cols);
        auto& slot = pairs[std::make_tuple(row.n, a, b)];
        if (row.box_rows <= row.box_cols) slot[0] = &row; else slot[1] = &row;
    }
    int violations = 0;
    for (const auto& kv : pairs) {
        const auto* p0 = kv.second[0]; const auto* p1 = kv.second[1];
        if (p0 == nullptr || p1 == nullptr) continue;
        const double t0 = p0->elapsed_s > 0.0 ? (static_cast<double>(p0->accepted) / p0->elapsed_s) : 0.0;
        const double t1 = p1->elapsed_s > 0.0 ? (static_cast<double>(p1->accepted) / p1->elapsed_s) : 0.0;
        const double base = std::max(0.001, std::max(t0, t1));
        if (std::abs(t0 - t1) * 100.0 / base > max_delta_pct) ++violations;
    }
    return violations;
}

inline PreDifficultyGateSummary run_pre_difficulty_gate_suite(const GenerateRunConfig& cfg, int quality_max_cases = 0) {
    PreDifficultyGateSummary summary{};
    summary.geometry = run_geometry_gate_20s_suite(&cfg);
    summary.quality = run_quality_benchmark_suite_20s(quality_max_cases, &cfg);
    summary.throughput_pair_violations = count_quality_throughput_pair_violations(summary.quality, 50.0);
    uint64_t attempts_total = 0; uint64_t reject_uniqueness_budget_total = 0;
    for (const auto& row : summary.quality.rows) { attempts_total += row.attempts; reject_uniqueness_budget_total += row.reject_uniqueness_budget; }
    summary.uniqueness_budget_pct = attempts_total > 0 ? (100.0 * static_cast<double>(reject_uniqueness_budget_total) / static_cast<double>(attempts_total)) : 0.0;

    const bool gate_ok_geometry = summary.geometry.contract_failed == 0 && summary.geometry.smoke_failed == 0 && summary.geometry.timeout_count == 0;
    const bool gate_ok_quality_pairs = summary.quality.pair_violations == 0;
    const bool gate_ok_throughput_pairs = summary.throughput_pair_violations == 0;
    const bool gate_ok_uniqueness_budget = summary.uniqueness_budget_pct <= 20.0;

    summary.passed = gate_ok_geometry && gate_ok_quality_pairs && gate_ok_throughput_pairs && gate_ok_uniqueness_budget;
    std::ostringstream msg;
    msg << "geometry_ok=" << (gate_ok_geometry ? "1" : "0") << " quality_pair_ok=" << (gate_ok_quality_pairs ? "1" : "0") << " throughput_pair_ok=" << (gate_ok_throughput_pairs ? "1" : "0") << " uniq_budget_ok=" << (gate_ok_uniqueness_budget ? "1" : "0") << " uniq_budget_pct=" << format_fixed(summary.uniqueness_budget_pct, 2);
    summary.message = msg.str();
    return summary;
}

inline std::string pre_difficulty_gate_csv_path_for(const std::string& report_txt) { std::filesystem::path p(report_txt); if (!p.has_extension()) p += ".txt"; std::filesystem::path csv = p; csv.replace_extension(".csv"); return csv.string(); }

inline void write_pre_difficulty_gate_report(const std::string& report_txt, const GenerateRunConfig& cfg, const PreDifficultyGateSummary& summary) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_txt).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(pre_difficulty_gate_csv_path_for(report_txt)).parent_path(), ec);

    std::ofstream txt(report_txt, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== Pre-Difficulty Gate ===\nstatus=" << (summary.passed ? "PASS" : "FAIL") << "\npolicy=" << normalize_profile_mode_policy(cfg.profile_mode_policy) << " full_for_n_ge=" << cfg.full_for_n_ge << "\nmessage=" << summary.message << "\n\ngeometry_total=" << summary.geometry.rows.size() << " contract_failed=" << summary.geometry.contract_failed << " smoke_failed=" << summary.geometry.smoke_failed << " timeout_count=" << summary.geometry.timeout_count << "\nquality_rows=" << summary.quality.rows.size() << " quality_pair_violations=" << summary.quality.pair_violations << " throughput_pair_violations=" << summary.throughput_pair_violations << " uniqueness_budget_pct=" << format_fixed(summary.uniqueness_budget_pct, 2) << "\n";
    }

    std::ofstream csv(pre_difficulty_gate_csv_path_for(report_txt), std::ios::out | std::ios::trunc);
    if (csv) {
        std::string msg = summary.message; std::replace(msg.begin(), msg.end(), ',', ';');
        csv << "status,profile_mode_policy,full_for_n_ge,geometry_contract_failed,geometry_smoke_failed,geometry_timeout_count,quality_pair_violations,throughput_pair_violations,uniqueness_budget_pct,message\n" << (summary.passed ? "PASS" : "FAIL") << "," << normalize_profile_mode_policy(cfg.profile_mode_policy) << "," << cfg.full_for_n_ge << "," << summary.geometry.contract_failed << "," << summary.geometry.smoke_failed << "," << summary.geometry.timeout_count << "," << summary.quality.pair_violations << "," << summary.throughput_pair_violations << "," << format_fixed(summary.uniqueness_budget_pct, 6) << "," << msg << "\n";
    }
}

inline int run_pre_difficulty_gate_cli(const std::string& report_txt, const GenerateRunConfig& cfg, int quality_max_cases = 0) {
    const PreDifficultyGateSummary summary = run_pre_difficulty_gate_suite(cfg, quality_max_cases);
    write_pre_difficulty_gate_report(report_txt, cfg, summary);
    std::cout << "Pre-difficulty gate: status=" << (summary.passed ? "PASS" : "FAIL") << " | " << summary.message << "\nReports: " << report_txt << " | " << pre_difficulty_gate_csv_path_for(report_txt) << "\n";
    return summary.passed ? 0 : 5;
}

struct VipBenchmarkCaseRow { int n = 0; int box_rows = 0; int box_cols = 0; std::string cpu_backend; uint64_t attempts = 0; uint64_t accepted = 0; double elapsed_s = 0.0; double vip_score = 0.0; std::string vip_grade; bool vip_contract_ok = true; uint64_t premium_signature = 0; };
struct VipBenchmarkSummary { std::vector<VipBenchmarkCaseRow> rows; int grade_target_failures = 0; double avg_vip_score = 0.0; };
inline std::string vip_benchmark_csv_path_for(const std::string& report_txt) { std::filesystem::path p(report_txt); if (!p.has_extension()) p += ".txt"; std::filesystem::path csv = p; csv.replace_extension(".csv"); return csv.string(); }

inline VipBenchmarkSummary run_vip_benchmark_suite(const GenerateRunConfig& base_cfg, int max_cases = 0) {
    VipBenchmarkSummary summary{};
    int executed = 0; double score_sum = 0.0;
    for (const auto& g : geometria::all_geometries()) {
        if (max_cases > 0 && executed >= max_cases) break;
        GenerateRunConfig cfg = base_cfg;
        cfg.box_rows = g.box_rows; cfg.box_cols = g.box_cols; cfg.target_puzzles = 1; cfg.threads = std::max(1, base_cfg.threads);
        cfg.seed = static_cast<long long>(g.n * 100000 + g.box_rows * 100 + g.box_cols + 313);
        cfg.required_strategy = RequiredStrategy::None; cfg.difficulty_level_required = 1;
        cfg.require_unique = true; cfg.strict_logical = true; cfg.enable_quality_contract = true; cfg.enable_replay_validation = true; cfg.enable_distribution_filter = true;
        cfg.pause_on_exit_windows = false; cfg.write_individual_files = false; cfg.output_folder = "plikiTMP/testy/vip_benchmark"; cfg.output_file = "vip_" + std::to_string(g.n) + "_" + std::to_string(g.box_rows) + "x" + std::to_string(g.box_cols) + ".txt";
        cfg.max_total_time_s = std::max<uint64_t>(10, base_cfg.max_total_time_s > 0 ? base_cfg.max_total_time_s : 20);
        cfg.vip_mode = true; cfg.difficulty_engine = "vip"; cfg.cpu_dispatch_report = false;
        const GenerationProfile profile = resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
        cfg.min_clues = profile.clue_range.min_clues; cfg.max_clues = profile.clue_range.max_clues;

        const GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);

        VipBenchmarkCaseRow row{};
        row.n = g.n; row.box_rows = g.box_rows; row.box_cols = g.box_cols; row.cpu_backend = result.cpu_backend_selected;
        row.attempts = result.attempts; row.accepted = result.accepted; row.elapsed_s = result.elapsed_s;
        row.vip_score = result.vip_score; row.vip_grade = result.vip_grade; row.vip_contract_ok = result.vip_contract_ok; row.premium_signature = result.premium_signature;
        if (!row.vip_contract_ok) ++summary.grade_target_failures;
        score_sum += row.vip_score;
        summary.rows.push_back(std::move(row));
        ++executed;
    }
    summary.avg_vip_score = summary.rows.empty() ? 0.0 : (score_sum / static_cast<double>(summary.rows.size()));
    return summary;
}

inline void write_vip_benchmark_reports(const VipBenchmarkSummary& summary, const std::string& report_txt) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_txt).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(vip_benchmark_csv_path_for(report_txt)).parent_path(), ec);

    std::ofstream txt(report_txt, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== VIP Benchmark ===\nrows=" << summary.rows.size() << " grade_target_failures=" << summary.grade_target_failures << " avg_vip_score=" << format_fixed(summary.avg_vip_score, 3) << "\n\n";
        for (const auto& r : summary.rows) txt << r.n << "x" << r.n << " (" << r.box_rows << "x" << r.box_cols << ") backend=" << r.cpu_backend << " accepted=" << r.accepted << " attempts=" << r.attempts << " elapsed_s=" << format_fixed(r.elapsed_s, 3) << " vip_score=" << format_fixed(r.vip_score, 3) << " vip_grade=" << r.vip_grade << " vip_contract_ok=" << (r.vip_contract_ok ? "1" : "0") << " signature=" << r.premium_signature << "\n";
    }

    std::ofstream csv(vip_benchmark_csv_path_for(report_txt), std::ios::out | std::ios::trunc);
    if (csv) {
        csv << "n,box_rows,box_cols,cpu_backend,attempts,accepted,elapsed_s,vip_score,vip_grade,vip_contract_ok,premium_signature\n";
        for (const auto& r : summary.rows) csv << r.n << "," << r.box_rows << "," << r.box_cols << "," << r.cpu_backend << "," << r.attempts << "," << r.accepted << "," << format_fixed(r.elapsed_s, 6) << "," << format_fixed(r.vip_score, 6) << "," << r.vip_grade << "," << (r.vip_contract_ok ? 1 : 0) << "," << r.premium_signature << "\n";
    }
}

inline int run_vip_benchmark_cli(const std::string& report_txt, const GenerateRunConfig& base_cfg, int max_cases = 0) {
    const VipBenchmarkSummary summary = run_vip_benchmark_suite(base_cfg, max_cases);
    write_vip_benchmark_reports(summary, report_txt);
    std::cout << "VIP benchmark: rows=" << summary.rows.size() << " grade_target_failures=" << summary.grade_target_failures << " avg_vip_score=" << format_fixed(summary.avg_vip_score, 3) << "\nReports: " << report_txt << " | " << vip_benchmark_csv_path_for(report_txt) << "\n";
    return summary.grade_target_failures > 0 ? 6 : 0;
}

struct VipGateSummary { VipBenchmarkSummary benchmark; bool passed = false; std::string message; };
inline std::string vip_gate_csv_path_for(const std::string& report_txt) { std::filesystem::path p(report_txt); if (!p.has_extension()) p += ".txt"; std::filesystem::path csv = p; csv.replace_extension(".csv"); return csv.string(); }

inline VipGateSummary run_vip_gate_suite(const GenerateRunConfig& base_cfg, int max_cases = 0) {
    VipGateSummary out{};
    out.benchmark = run_vip_benchmark_suite(base_cfg, max_cases);
    const bool avg_score_ok = out.benchmark.avg_vip_score >= 300.0;
    const bool grade_ok = out.benchmark.grade_target_failures == 0;
    out.passed = avg_score_ok && grade_ok;
    std::ostringstream oss;
    oss << "avg_score_ok=" << (avg_score_ok ? "1" : "0") << " grade_ok=" << (grade_ok ? "1" : "0") << " avg_vip_score=" << format_fixed(out.benchmark.avg_vip_score, 3) << " grade_target_failures=" << out.benchmark.grade_target_failures;
    out.message = oss.str();
    return out;
}

inline void write_vip_gate_report(const VipGateSummary& summary, const std::string& report_txt) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(report_txt).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(vip_gate_csv_path_for(report_txt)).parent_path(), ec);

    std::ofstream txt(report_txt, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== VIP Gate ===\nstatus=" << (summary.passed ? "PASS" : "FAIL") << "\nmessage=" << summary.message << "\nrows=" << summary.benchmark.rows.size() << " avg_vip_score=" << format_fixed(summary.benchmark.avg_vip_score, 3) << " grade_target_failures=" << summary.benchmark.grade_target_failures << "\n";
    }

    std::ofstream csv(vip_gate_csv_path_for(report_txt), std::ios::out | std::ios::trunc);
    if (csv) {
        std::string msg = summary.message; std::replace(msg.begin(), msg.end(), ',', ';');
        csv << "status,avg_vip_score,grade_target_failures,message\n" << (summary.passed ? "PASS" : "FAIL") << "," << format_fixed(summary.benchmark.avg_vip_score, 6) << "," << summary.benchmark.grade_target_failures << "," << msg << "\n";
    }
}

inline int run_vip_gate_cli(const std::string& report_txt, const GenerateRunConfig& base_cfg, int max_cases = 0) {
    const VipGateSummary summary = run_vip_gate_suite(base_cfg, max_cases);
    write_vip_gate_report(summary, report_txt);
    std::cout << "VIP gate: status=" << (summary.passed ? "PASS" : "FAIL") << " | " << summary.message << "\nReports: " << report_txt << " | " << vip_gate_csv_path_for(report_txt) << "\n";
    return summary.passed ? 0 : 7;
}

struct PerfAbCaseRow { int n = 0; int box_rows = 0; int box_cols = 0; std::string backend; uint64_t accepted = 0; uint64_t attempts = 0; double elapsed_s = 0.0; double throughput = 0.0; double baseline_throughput = 0.0; double delta_pct = 0.0; };
struct PerfAbSummary { std::vector<PerfAbCaseRow> rows; double avg_throughput = 0.0; double avg_delta_pct = 0.0; };

inline std::unordered_map<std::string, double> load_perf_ab_baseline_csv(const std::string& path) {
    std::unordered_map<std::string, double> out;
    if (path.empty()) return out;
    std::ifstream in(path);
    if (!in) return out;
    std::string line;
    if (!std::getline(in, line)) return out;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields;
        std::istringstream iss(line);
        std::string token;
        while (std::getline(iss, token, ',')) fields.push_back(token);
        if (fields.size() < 8) continue;
        const std::string key = fields[1] + "x" + fields[2];
        try { out[key] = std::stod(fields[7]); } catch (...) {}
    }
    return out;
}

inline PerfAbSummary run_perf_ab_suite(const GenerateRunConfig& base_cfg) {
    PerfAbSummary out{};
    std::vector<geometria::GeometrySpec> selected;
    for (const auto& g : geometria::all_geometries()) {
        if (g.n < 9 || g.n > 30) continue;
        if (g.n == 9 || g.n == 12 || g.n == 16 || g.n == 20 || g.n == 24 || g.n == 30) selected.push_back(g);
    }
    std::sort(selected.begin(), selected.end(), [](const auto& a, const auto& b) { if (a.n != b.n) return a.n < b.n; if (a.box_rows != b.box_rows) return a.box_rows < b.box_rows; return a.box_cols < b.box_cols; });
    if (selected.size() > 12) selected.resize(12);

    const auto baseline_map = load_perf_ab_baseline_csv(base_cfg.perf_baseline_csv);
    double throughput_sum = 0.0; double delta_sum = 0.0; int delta_count = 0;
    for (const auto& g : selected) {
        GenerateRunConfig cfg = base_cfg;
        cfg.box_rows = g.box_rows; cfg.box_cols = g.box_cols;
        cfg.target_puzzles = std::max<uint64_t>(3, std::min<uint64_t>(10, base_cfg.target_puzzles));
        cfg.max_total_time_s = std::max<uint64_t>(6, (base_cfg.max_total_time_s > 0 ? base_cfg.max_total_time_s : 8));
        cfg.max_attempts_s = 0; cfg.seed = static_cast<long long>(g.n * 100000 + g.box_rows * 100 + g.box_cols + 991);
        cfg.pause_on_exit_windows = false; cfg.write_individual_files = false;
        cfg.output_folder = "plikiTMP/testy/perf_ab"; cfg.output_file = "perf_" + std::to_string(g.n) + "_" + std::to_string(g.box_rows) + "x" + std::to_string(g.box_cols) + ".txt";
        const GenerationProfile profile = resolve_generation_profile(cfg.box_rows, cfg.box_cols, cfg.difficulty_level_required, cfg.required_strategy, &cfg);
        cfg.min_clues = profile.clue_range.min_clues; cfg.max_clues = profile.clue_range.max_clues;

        const GenerateRunResult result = run_generic_sudoku(cfg, nullptr, nullptr, nullptr, nullptr, nullptr);
        PerfAbCaseRow row{};
        row.n = g.n; row.box_rows = g.box_rows; row.box_cols = g.box_cols; row.backend = result.cpu_backend_selected;
        row.accepted = result.accepted; row.attempts = result.attempts; row.elapsed_s = result.elapsed_s; row.throughput = result.accepted_per_sec;
        const std::string key = std::to_string(row.box_rows) + "x" + std::to_string(row.box_cols);
        const auto it = baseline_map.find(key);
        if (it != baseline_map.end()) {
            row.baseline_throughput = it->second;
            row.delta_pct = ((row.throughput - row.baseline_throughput) / std::max(0.000001, row.baseline_throughput)) * 100.0;
            delta_sum += row.delta_pct; ++delta_count;
        }
        throughput_sum += row.throughput; out.rows.push_back(row);
    }
    out.avg_throughput = out.rows.empty() ? 0.0 : throughput_sum / static_cast<double>(out.rows.size());
    out.avg_delta_pct = delta_count > 0 ? (delta_sum / static_cast<double>(delta_count)) : 0.0;
    return out;
}

inline void write_perf_ab_reports(const PerfAbSummary& summary, const std::string& txt_path, const std::string& csv_path) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(txt_path).parent_path(), ec);
    std::filesystem::create_directories(std::filesystem::path(csv_path).parent_path(), ec);

    std::ofstream txt(txt_path, std::ios::out | std::ios::trunc);
    if (txt) {
        txt << "=== PERF A/B SUITE ===\nrows=" << summary.rows.size() << " avg_throughput=" << format_fixed(summary.avg_throughput, 4) << " avg_delta_pct=" << format_fixed(summary.avg_delta_pct, 3) << "\n\n";
        for (const auto& row : summary.rows) {
            txt << row.n << "x" << row.n << " (" << row.box_rows << "x" << row.box_cols << ") backend=" << row.backend << " accepted=" << row.accepted << " attempts=" << row.attempts << " elapsed_s=" << format_fixed(row.elapsed_s, 6) << " throughput=" << format_fixed(row.throughput, 6);
            if (row.baseline_throughput > 0.0) txt << " baseline=" << format_fixed(row.baseline_throughput, 6) << " delta_pct=" << format_fixed(row.delta_pct, 3);
            txt << "\n";
        }
    }

    std::ofstream csv(csv_path, std::ios::out | std::ios::trunc);
    if (csv) {
        csv << "n,box_rows,box_cols,backend,accepted,attempts,elapsed_s,throughput,baseline_throughput,delta_pct\n";
        for (const auto& row : summary.rows) csv << row.n << "," << row.box_rows << "," << row.box_cols << "," << row.backend << "," << row.accepted << "," << row.attempts << "," << format_fixed(row.elapsed_s, 6) << "," << format_fixed(row.throughput, 6) << "," << format_fixed(row.baseline_throughput, 6) << "," << format_fixed(row.delta_pct, 6) << "\n";
    }
}

inline int run_perf_ab_suite_cli(const GenerateRunConfig& cfg) {
    const auto baseline_debug = load_perf_ab_baseline_csv(cfg.perf_baseline_csv);
    const PerfAbSummary summary = run_perf_ab_suite(cfg);
    write_perf_ab_reports(summary, cfg.perf_report_out, cfg.perf_csv_out);
    std::cout << "Perf A/B suite: rows=" << summary.rows.size() << " avg_throughput=" << format_fixed(summary.avg_throughput, 4) << " avg_delta_pct=" << format_fixed(summary.avg_delta_pct, 3) << "\n";
    if (!cfg.perf_baseline_csv.empty()) std::cout << "Perf baseline entries: " << baseline_debug.size() << "\n";
    std::cout << "Reports: " << cfg.perf_report_out << " | " << cfg.perf_csv_out << "\n";
    return 0;
}

inline std::vector<std::string> stage_trackable_roots() { return {"main.cpp", "Sources"}; }

inline bool stage_is_source_file(const std::filesystem::path& p) {
    const std::string ext = p.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c";
}

inline std::vector<std::filesystem::path> collect_stage_files_current() {
    std::vector<std::filesystem::path> files;
    const std::filesystem::path cwd = std::filesystem::weakly_canonical(std::filesystem::current_path());
    auto to_rel = [&](const std::filesystem::path& p) -> std::filesystem::path {
        std::error_code ec; const auto abs = std::filesystem::weakly_canonical(p, ec);
        if (!ec) { const auto rel = abs.lexically_relative(cwd); return !rel.empty() ? rel : abs.lexically_normal(); }
        return p.lexically_normal();
    };
    for (const auto& root : stage_trackable_roots()) {
        const std::filesystem::path rp(root);
        if (!std::filesystem::exists(rp)) continue;
        if (std::filesystem::is_regular_file(rp)) {
            const auto rel = to_rel(rp);
            if (stage_is_source_file(rel)) files.push_back(rel);
            continue;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rp)) {
            if (!entry.is_regular_file()) continue;
            const auto rel = to_rel(entry.path());
            if (stage_is_source_file(rel)) files.push_back(rel);
        }
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

inline std::vector<std::string> read_text_lines(const std::filesystem::path& p) {
    std::vector<std::string> out; std::ifstream in(p); std::string line;
    while (std::getline(in, line)) out.push_back(line);
    return out;
}

inline uint64_t lcs_length(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.empty() || b.empty()) return 0;
    std::vector<uint32_t> prev(b.size() + 1, 0); std::vector<uint32_t> curr(b.size() + 1, 0);
    for (size_t i = 1; i <= a.size(); ++i) {
        for (size_t j = 1; j <= b.size(); ++j) {
            if (a[i - 1] == b[j - 1]) curr[j] = prev[j - 1] + 1U;
            else curr[j] = std::max(prev[j], curr[j - 1]);
        }
        std::swap(prev, curr); std::fill(curr.begin(), curr.end(), 0U);
    }
    return static_cast<uint64_t>(prev[b.size()]);
}

inline std::pair<uint64_t, uint64_t> diff_added_removed_lines(const std::filesystem::path& before_file, const std::filesystem::path& after_file) {
    const bool has_before = std::filesystem::exists(before_file); const bool has_after = std::filesystem::exists(after_file);
    if (!has_before && !has_after) return {0ULL, 0ULL};
    if (!has_before && has_after) return {static_cast<uint64_t>(read_text_lines(after_file).size()), 0ULL};
    if (has_before && !has_after) return {0ULL, static_cast<uint64_t>(read_text_lines(before_file).size())};
    const auto a = read_text_lines(before_file); const auto b = read_text_lines(after_file);
    if (a == b) return {0ULL, 0ULL};
    return {static_cast<uint64_t>(b.size()), static_cast<uint64_t>(a.size())};
}

inline std::filesystem::path stage_snapshot_root(const std::string& stage_name) {
    return std::filesystem::path("plikiTMP") / "stage_snapshots" / stage_name;
}

inline int run_stage_start_cli(const GenerateRunConfig& base_cfg) {
    if (base_cfg.stage_name.empty()) { std::cerr << "stage-start requires --stage-start <name>\n"; return 2; }
    std::error_code ec; const auto root = stage_snapshot_root(base_cfg.stage_name); const auto before_dir = root / "before";
    std::filesystem::remove_all(root, ec); std::filesystem::create_directories(before_dir, ec);
    if (ec) { std::cerr << "stage-start: cannot create snapshot dir: " << root.string() << " err=" << ec.message() << "\n"; return 2; }
    const auto files = collect_stage_files_current();
    for (const auto& rel : files) {
        const auto dst = before_dir / rel; std::filesystem::create_directories(dst.parent_path(), ec);
        std::filesystem::copy_file(rel, dst, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { std::cerr << "stage-start: copy failed: " << rel.string() << " err=" << ec.message() << "\n"; return 2; }
    }

    GenerateRunConfig perf_cfg = base_cfg; perf_cfg.perf_baseline_csv.clear();
    perf_cfg.perf_report_out = (root / "perf_before.txt").string(); perf_cfg.perf_csv_out = (root / "perf_before.csv").string();
    const PerfAbSummary perf_before = run_perf_ab_suite(perf_cfg);
    write_perf_ab_reports(perf_before, perf_cfg.perf_report_out, perf_cfg.perf_csv_out);

    std::ofstream out(base_cfg.stage_report_out, std::ios::out | std::ios::trunc);
    if (out) { out << "stage=" << base_cfg.stage_name << "\nstatus=STARTED\nsnapshot_root=" << root.string() << "\ntracked_files=" << files.size() << "\nperf_before_csv=" << perf_cfg.perf_csv_out << "\nperf_before_txt=" << perf_cfg.perf_report_out << "\n"; }
    std::cout << "Stage start: " << base_cfg.stage_name << "\nTracked files: " << files.size() << "\nPerf BEFORE: " << perf_cfg.perf_report_out << " | " << perf_cfg.perf_csv_out << "\n";
    return 0;
}

inline int run_stage_end_cli(const GenerateRunConfig& base_cfg) {
    if (base_cfg.stage_name.empty()) { std::cerr << "stage-end requires --stage-end <name>\n"; return 3; }
    const auto root = stage_snapshot_root(base_cfg.stage_name); const auto before_dir = root / "before";
    if (!std::filesystem::exists(before_dir)) { std::cerr << "stage-end: snapshot not found: " << before_dir.string() << "\n"; return 3; }
    std::error_code ec; const auto current_dir = root / "current"; std::filesystem::remove_all(current_dir, ec); std::filesystem::create_directories(current_dir, ec);
    if (ec) { std::cerr << "stage-end: cannot create current snapshot: " << current_dir.string() << " err=" << ec.message() << "\n"; return 3; }
    const auto current_files = collect_stage_files_current();
    for (const auto& rel : current_files) {
        const auto dst = current_dir / rel; std::filesystem::create_directories(dst.parent_path(), ec);
        std::filesystem::copy_file(rel, dst, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { std::cerr << "stage-end: copy current failed: " << rel.string() << " err=" << ec.message() << "\n"; return 3; }
    }
    const auto numstat_path = root / "numstat.txt";
    const std::string cmd = "git diff --no-index --numstat \"" + before_dir.string() + "\" \"" + current_dir.string() + "\" > \"" + numstat_path.string() + "\" 2> NUL";
    (void)std::system(cmd.c_str());
    uint64_t lines_added = 0; uint64_t lines_removed = 0;
    std::ifstream numstat(numstat_path); std::string line;
    while (std::getline(numstat, line)) {
        if (line.empty()) continue; std::istringstream iss(line); std::string add_s, rem_s, path_s;
        if (!std::getline(iss, add_s, '\t') || !std::getline(iss, rem_s, '\t')) continue; std::getline(iss, path_s);
        if (add_s == "-" || rem_s == "-") continue;
        try { lines_added += static_cast<uint64_t>(std::stoull(add_s)); lines_removed += static_cast<uint64_t>(std::stoull(rem_s)); } catch (...) {}
    }

    GenerateRunConfig perf_cfg = base_cfg; perf_cfg.perf_baseline_csv = (root / "perf_before.csv").string();
    perf_cfg.perf_report_out = (root / "perf_after.txt").string(); perf_cfg.perf_csv_out = (root / "perf_after.csv").string();
    const PerfAbSummary perf_after = run_perf_ab_suite(perf_cfg);
    write_perf_ab_reports(perf_after, perf_cfg.perf_report_out, perf_cfg.perf_csv_out);

    std::ofstream out(base_cfg.stage_report_out, std::ios::out | std::ios::trunc);
    if (out) { out << "stage=" << base_cfg.stage_name << "\nstatus=COMPLETED\nlines_added=" << lines_added << "\nlines_removed=" << lines_removed << "\nperf_after_avg_throughput=" << format_fixed(perf_after.avg_throughput, 6) << "\nperf_after_avg_delta_pct=" << format_fixed(perf_after.avg_delta_pct, 6) << "\nperf_after_csv=" << perf_cfg.perf_csv_out << "\nperf_after_txt=" << perf_cfg.perf_report_out << "\n"; }
    std::cout << "Stage end: " << base_cfg.stage_name << " lines_added=" << lines_added << " lines_removed=" << lines_removed << " avg_delta_pct=" << format_fixed(perf_after.avg_delta_pct, 3) << "\nPerf AFTER: " << perf_cfg.perf_report_out << " | " << perf_cfg.perf_csv_out << "\nStage report: " << base_cfg.stage_report_out << "\n";
    return 0;
}

BenchmarkReportData build_sample_benchmark_data(const GenerateRunConfig& cfg, const GenerateRunResult& result) {
    BenchmarkReportData data;
    data.probe_per_level = std::to_string(cfg.target_puzzles);
    data.benchmark_mode = "initial_generic_stage";
    data.runtime_info = detect_runtime_info();
    data.threads_info = std::to_string(cfg.threads <= 0 ? std::max(1u, std::thread::hardware_concurrency()) : static_cast<unsigned>(cfg.threads));
#ifdef _WIN32
    data.os_info = detect_os_info();
#else
    data.os_info = "UnknownOS";
#endif
    data.cpu_model = detect_cpu_model(); data.ram_info = detect_ram_info();

    BenchmarkTableARow a;
    a.lvl = cfg.difficulty_level_required; a.solved_ok = static_cast<int>(result.accepted); a.analyzed = static_cast<int>(result.attempts);
    a.required_use = result.analyzed_required_strategy; a.required_hit = result.required_strategy_hits; a.reject_strategy = result.reject_strategy;
    a.avg_solved_gen_ms = result.accepted > 0 ? (result.elapsed_s * 1000.0) / static_cast<double>(result.accepted) : 0.0;
    a.avg_dig_ms = 0.0; a.avg_analyze_ms = 0.0; a.backtracks = 0; a.timeouts = 0; a.success_rate = result.attempts > 0 ? (100.0 * static_cast<double>(result.accepted) / static_cast<double>(result.attempts)) : 0.0;
    data.table_a.push_back(a);

    BenchmarkTableA2Row a2; a2.lvl = cfg.difficulty_level_required; a2.analyzed = static_cast<int>(result.attempts); data.table_a2.push_back(a2);

    BenchmarkTableA3Row a3; a3.strategy = to_string(cfg.required_strategy); a3.lvl = cfg.difficulty_level_required; a3.max_attempts = cfg.max_attempts; a3.analyzed = result.analyzed_required_strategy; a3.required_strategy_hits = result.required_strategy_hits;
    a3.analyzed_per_s = result.elapsed_s > 0.0 ? static_cast<double>(a3.analyzed) / result.elapsed_s : 0.0; a3.est_5min = static_cast<uint64_t>(a3.analyzed_per_s * 300.0); a3.written = result.written_required_strategy;
    data.table_a3.push_back(a3);

    BenchmarkTableBRow bg; const int n = std::max(1, cfg.box_rows) * std::max(1, cfg.box_cols); bg.size = std::to_string(n) + "x" + std::to_string(n);
    for (int lvl = 1; lvl <= 8; ++lvl) { const ClueRange cr = clue_range_for_size_level(n, lvl); bg.levels[static_cast<size_t>(lvl - 1)] = std::to_string(cr.min_clues) + "-" + std::to_string(cr.max_clues); }
    data.table_b.push_back(bg);

    BenchmarkTableCRow c; c.size = std::to_string(n) + "x" + std::to_string(n); c.lvl = cfg.difficulty_level_required; c.est_analyze_s = 0.0; c.budget_s = cfg.attempt_time_budget_s > 0.0 ? cfg.attempt_time_budget_s : 0.0; c.peak_ram_mb = 0.0; c.decision = "RUN";
    data.table_c.push_back(c);

    data.rules = { "Pomijanie testow gdy est_analyze_s > budget_s dla danego rozmiaru.", "Dla N>12 mozna pomijac poziomy 7-9 przy przekroczeniu budzetu.", "Wymuszona unikalnosc tylko przez array DLX limit=2.", "Kontrakt required_strategy: use=analyzed_required_strategy, hit=required_strategy_hits, reject_strategy gdy !(use&&hit)." };
    data.total_execution_s = static_cast<uint64_t>(std::llround(result.elapsed_s));
    data.table_microprofiling.clear();
    return data;
}

RequiredStrategy default_required_strategy_for_level(int lvl) {
    if (lvl == 1) return RequiredStrategy::NakedSingle;
    if (lvl == 2) return RequiredStrategy::HiddenSingle;
    return RequiredStrategy::None;
}

BenchmarkReportData run_benchmark_profiles_40s(const GenerateRunConfig& base_cfg);

} // namespace sudoku_hpc



