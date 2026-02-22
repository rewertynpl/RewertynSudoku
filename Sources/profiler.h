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

namespace sudoku_hpc {
enum class ProfilerStage : uint8_t {
    SolvedKernel = 0,
    DigKernel = 1,
    QuickPrefilter = 2,
    LogicCertify = 3,
    UniquenessCertify = 4,
    TotalCount = 5
};

inline const char* profiler_stage_name(ProfilerStage stage) {
    switch (stage) {
    case ProfilerStage::SolvedKernel: return "SolvedKernel";
    case ProfilerStage::DigKernel: return "DigKernel";
    case ProfilerStage::QuickPrefilter: return "QuickPrefilter";
    case ProfilerStage::LogicCertify: return "LogicCertify";
    case ProfilerStage::UniquenessCertify: return "UniquenessCertify";
    default: return "Unknown";
    }
}

struct StageMetrics {
    std::atomic<uint64_t> call_count{0};
    std::atomic<uint64_t> total_elapsed_ns{0};
    std::atomic<uint64_t> min_elapsed_ns{std::numeric_limits<uint64_t>::max()};
    std::atomic<uint64_t> max_elapsed_ns{0};
    
    void record(uint64_t elapsed_ns) {
        call_count.fetch_add(1, std::memory_order_relaxed);
        total_elapsed_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
        
        // Atomowe min/max - pętla CAS
        uint64_t current_min = min_elapsed_ns.load(std::memory_order_relaxed);
        while (elapsed_ns < current_min) {
            if (min_elapsed_ns.compare_exchange_weak(current_min, elapsed_ns, 
                std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
        
        uint64_t current_max = max_elapsed_ns.load(std::memory_order_relaxed);
        while (elapsed_ns > current_max) {
            if (max_elapsed_ns.compare_exchange_weak(current_max, elapsed_ns,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
    
    double avg_elapsed_ms() const {
        const uint64_t count = call_count.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return static_cast<double>(total_elapsed_ns.load(std::memory_order_relaxed)) / 
               static_cast<double>(count) / 1'000'000.0;
    }
    
    double total_elapsed_ms() const {
        return static_cast<double>(total_elapsed_ns.load(std::memory_order_relaxed)) / 1'000'000.0;
    }
    
    uint64_t min_elapsed_ns_val() const {
        const uint64_t val = min_elapsed_ns.load(std::memory_order_relaxed);
        return (val == std::numeric_limits<uint64_t>::max()) ? 0 : val;
    }
    
    uint64_t max_elapsed_ns_val() const {
        return max_elapsed_ns.load(std::memory_order_relaxed);
    }
    
    uint64_t call_count_val() const {
        return call_count.load(std::memory_order_relaxed);
    }
};

struct StrategyMetrics {
    std::string strategy_name = "None";
    int level = 0;
    std::atomic<uint64_t> use_count{0};
    std::atomic<uint64_t> hit_count{0};
    std::atomic<uint64_t> placements{0};
    std::atomic<uint64_t> total_elapsed_ns{0};
    
    void record_use(uint64_t elapsed_ns) {
        use_count.fetch_add(1, std::memory_order_relaxed);
        total_elapsed_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
    }
    
    void record_hit(uint64_t placements_count) {
        hit_count.fetch_add(1, std::memory_order_relaxed);
        placements.fetch_add(placements_count, std::memory_order_relaxed);
    }
    
    double avg_elapsed_ms() const {
        const uint64_t count = use_count.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return static_cast<double>(total_elapsed_ns.load(std::memory_order_relaxed)) / 
               static_cast<double>(count) / 1'000'000.0;
    }
};

struct AttemptMetrics {
    std::atomic<uint64_t> total_attempts{0};
    std::atomic<uint64_t> successful_attempts{0};
    std::atomic<uint64_t> total_time_ns{0};
    std::atomic<uint64_t> dead_ends{0};
    std::atomic<uint64_t> max_search_depth{0};
    std::atomic<uint64_t> backtrack_count{0};
    
    void record_attempt(uint64_t elapsed_ns, bool success) {
        total_attempts.fetch_add(1, std::memory_order_relaxed);
        total_time_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
        if (success) {
            successful_attempts.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    void record_dead_end() {
        dead_ends.fetch_add(1, std::memory_order_relaxed);
    }
    
    void record_search_depth(uint64_t depth) {
        uint64_t current_max = max_search_depth.load(std::memory_order_relaxed);
        while (depth > current_max) {
            if (max_search_depth.compare_exchange_weak(current_max, depth,
                std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
    
    void record_backtrack() {
        backtrack_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    double avg_attempt_time_ms() const {
        const uint64_t count = total_attempts.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return static_cast<double>(total_time_ns.load(std::memory_order_relaxed)) / 
               static_cast<double>(count) / 1'000'000.0;
    }
    
    double success_rate_pct() const {
        const uint64_t total = total_attempts.load(std::memory_order_relaxed);
        if (total == 0) return 0.0;
        return 100.0 * static_cast<double>(successful_attempts.load(std::memory_order_relaxed)) / 
               static_cast<double>(total);
    }
    
    uint64_t total_attempts_val() const {
        return total_attempts.load(std::memory_order_relaxed);
    }
    
    uint64_t dead_ends_val() const {
        return dead_ends.load(std::memory_order_relaxed);
    }
    
    uint64_t max_depth_val() const {
        return max_search_depth.load(std::memory_order_relaxed);
    }
    
    uint64_t backtrack_count_val() const {
        return backtrack_count.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// MIKROPROFILING - QPC (QueryPerformanceCounter) via std::chrono::steady_clock
// ============================================================================
// Profiling per etap:
//   - SolvedKernel: generowanie pełnej planszy
//   - DigKernel: usuwanie wskazówek
//   - QuickPrefilter: szybka walidacja wstępna
//   - LogicCertify: certyfikacja logiczna
//   - UniquenessCertify: unikalność (DLX)
//
// Profiling per strategia:
//   - NakedSingle, HiddenSingle (poziom 1)
//   - PointingPairs, BoxLineReduction (poziom 2)
//   - ... (poziomy 3-8)
// ============================================================================

class MicroProfiler {
public:
    MicroProfiler() {
        // Atomic variables are already zero-initialized
        // No need to assign StageMetrics{} which would fail due to atomics
    }

    void record_stage(ProfilerStage stage, uint64_t elapsed_ns) {
        if (static_cast<int>(stage) < static_cast<int>(ProfilerStage::TotalCount)) {
            stage_metrics_[static_cast<size_t>(stage)].record(elapsed_ns);
        }
    }

    StageMetrics& get_stage_metrics(ProfilerStage stage) {
        return stage_metrics_[static_cast<size_t>(stage)];
    }

    const StageMetrics& get_stage_metrics(ProfilerStage stage) const {
        return stage_metrics_[static_cast<size_t>(stage)];
    }

    AttemptMetrics& get_attempt_metrics() { return attempt_metrics_; }
    const AttemptMetrics& get_attempt_metrics() const { return attempt_metrics_; }

    void record_strategy_use(const std::string& name, int level, uint64_t elapsed_ns) {
        std::lock_guard<std::mutex> lock(strategy_mu_);
        for (const auto& sm : strategy_metrics_) {
            if (sm->strategy_name == name && sm->level == level) {
                sm->record_use(elapsed_ns);
                return;
            }
        }
        // Nowa strategia - używamy unique_ptr
        auto sm = std::make_unique<StrategyMetrics>();
        sm->strategy_name = name;
        sm->level = level;
        sm->record_use(elapsed_ns);
        strategy_metrics_.push_back(std::move(sm));
    }

    void record_strategy_hit(const std::string& name, int level, uint64_t placements) {
        std::lock_guard<std::mutex> lock(strategy_mu_);
        for (const auto& sm : strategy_metrics_) {
            if (sm->strategy_name == name && sm->level == level) {
                sm->record_hit(placements);
                return;
            }
        }
    }

    std::vector<std::unique_ptr<StrategyMetrics>> get_strategy_metrics() const {
        std::lock_guard<std::mutex> lock(strategy_mu_);
        // Return copies of the metrics (without atomics issue)
        std::vector<std::unique_ptr<StrategyMetrics>> result;
        for (const auto& sm : strategy_metrics_) {
            auto copy = std::make_unique<StrategyMetrics>();
            copy->strategy_name = sm->strategy_name;
            copy->level = sm->level;
            copy->use_count.store(sm->use_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            copy->hit_count.store(sm->hit_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            copy->placements.store(sm->placements.load(std::memory_order_relaxed), std::memory_order_relaxed);
            copy->total_elapsed_ns.store(sm->total_elapsed_ns.load(std::memory_order_relaxed), std::memory_order_relaxed);
            result.push_back(std::move(copy));
        }
        return result;
    }

    void reset() {
        // Reset stage metrics using atomic store
        for (int i = 0; i < static_cast<int>(ProfilerStage::TotalCount); ++i) {
            stage_metrics_[i].call_count.store(0, std::memory_order_relaxed);
            stage_metrics_[i].total_elapsed_ns.store(0, std::memory_order_relaxed);
            stage_metrics_[i].min_elapsed_ns.store(std::numeric_limits<uint64_t>::max(), std::memory_order_relaxed);
            stage_metrics_[i].max_elapsed_ns.store(0, std::memory_order_relaxed);
        }
        // Reset attempt metrics
        attempt_metrics_.total_attempts.store(0, std::memory_order_relaxed);
        attempt_metrics_.successful_attempts.store(0, std::memory_order_relaxed);
        attempt_metrics_.total_time_ns.store(0, std::memory_order_relaxed);
        attempt_metrics_.dead_ends.store(0, std::memory_order_relaxed);
        attempt_metrics_.max_search_depth.store(0, std::memory_order_relaxed);
        attempt_metrics_.backtrack_count.store(0, std::memory_order_relaxed);
        // Clear strategy metrics
        std::lock_guard<std::mutex> lock(strategy_mu_);
        strategy_metrics_.clear();  // unique_ptr automatically deletes
    }

    std::string to_summary_string() const {
        std::ostringstream oss;
        oss << "=== Mikroprofiling Summary ===\n";
        oss << "Etapy:\n";
        for (int i = 0; i < static_cast<int>(ProfilerStage::TotalCount); ++i) {
            const auto stage = static_cast<ProfilerStage>(i);
            const auto& m = stage_metrics_[i];
            oss << "  " << profiler_stage_name(stage) << ": "
                << "calls=" << m.call_count_val()
                << ", avg_ms=" << std::fixed << std::setprecision(4) << m.avg_elapsed_ms()
                << ", total_ms=" << std::fixed << std::setprecision(2) << m.total_elapsed_ms()
                << ", min_ns=" << m.min_elapsed_ns_val()
                << ", max_ns=" << m.max_elapsed_ns_val() << "\n";
        }
        oss << "Proby:\n";
        oss << "  total_attempts=" << attempt_metrics_.total_attempts_val()
            << ", success_rate=" << std::fixed << std::setprecision(2) << attempt_metrics_.success_rate_pct() << "%"
            << ", avg_attempt_ms=" << std::fixed << std::setprecision(4) << attempt_metrics_.avg_attempt_time_ms()
            << ", dead_ends=" << attempt_metrics_.dead_ends_val()
            << ", max_depth=" << attempt_metrics_.max_depth_val()
            << ", backtracks=" << attempt_metrics_.backtrack_count_val() << "\n";
        return oss.str();
    }

private:
    std::array<StageMetrics, static_cast<size_t>(ProfilerStage::TotalCount)> stage_metrics_{};
    AttemptMetrics attempt_metrics_{};
    mutable std::mutex strategy_mu_;
    std::vector<std::unique_ptr<StrategyMetrics>> strategy_metrics_;
};

// Thread-local timer helper for RAII-based profiling
class ScopedStageTimer {
public:
    ScopedStageTimer(MicroProfiler& profiler, ProfilerStage stage)
        : profiler_(profiler), stage_(stage) {
        start_ = std::chrono::steady_clock::now();
    }
    
    ~ScopedStageTimer() {
        const auto end = std::chrono::steady_clock::now();
        const uint64_t elapsed_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count());
        profiler_.record_stage(stage_, elapsed_ns);
    }
    
private:
    MicroProfiler& profiler_;
    ProfilerStage stage_;
    std::chrono::steady_clock::time_point start_;
};

// ============================================================================
// KONIEC MIKROPROFILING
// ============================================================================

} // namespace sudoku_hpc
