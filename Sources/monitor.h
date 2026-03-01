//Author copyright Marcin Matysek (Rewertyn)
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "utils/logging.h"

namespace sudoku_hpc {

struct MonitorTotalsSnapshot {
    uint64_t target = 0;
    uint64_t accepted = 0;
    uint64_t written = 0;
    uint64_t attempts = 0;
    uint64_t analyzed_required_strategy = 0;
    uint64_t required_strategy_hits = 0;
    uint64_t written_required_strategy = 0;
    uint64_t rejected = 0;
    uint64_t active_workers = 0;
    uint64_t reseeds = 0;
};

struct WorkerRow {
    std::string worker = "worker_00";
    int clues = 0;
    uint64_t seed = 0;
    uint64_t last_reseed_steady_ns = 0;
    uint64_t resets = 0;
    uint64_t applied = 0;
    double reset_lag = 0.0;
    double lag_max = 0.0;
    double reset_in_s = 0.0;
    std::string status = "idle";
    uint64_t dead_ends = 0;
    uint64_t max_depth = 0;
    double avg_node_ms = 0.0;
    double cpu_load_pct = 0.0;
    double ram_current_mb = 0.0;
    double ram_peak_mb = 0.0;
    int reseed_interval_s = 0;
    double attempt_time_budget_s = 0.0;
    uint64_t attempt_node_budget = 0;
    double stage_solved_ms = 0.0;
    double stage_dig_ms = 0.0;
    double stage_prefilter_ms = 0.0;
    double stage_logic_ms = 0.0;
    double stage_uniqueness_ms = 0.0;
    double avg_attempt_ms = 0.0;
    double success_rate_pct = 0.0;
    uint64_t backtrack_count = 0;
};

struct StrategyRow {
    std::string strategy = "None";
    int lvl = 0;
    uint64_t max_attempts = 0;
    uint64_t analyzed = 0;
    uint64_t required_strategy_hits = 0;
    double analyzed_per_min = 0.0;
    uint64_t est_5min = 0;
    uint64_t written = 0;
    double avg_clues = 0.0;
};

class ConsoleStatsMonitor {
public:
    ConsoleStatsMonitor() {
        start_tp_ = std::chrono::steady_clock::now();
    }

    ~ConsoleStatsMonitor() {
        stop_ui_thread();
    }

    void set_target(uint64_t target) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.target = target;
    }

    void set_active_workers(int n) {
        {
            std::unique_lock<std::shared_mutex> lock(totals_mu_);
            totals_.active_workers = static_cast<uint64_t>(std::max(0, n));
        }
        std::lock_guard<std::mutex> lock(workers_mu_);
        if (static_cast<int>(workers_.size()) < n) {
            workers_.resize(static_cast<size_t>(n));
        }
    }

    void set_attempts(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.attempts = v; }
    void set_attempts_total(uint64_t v) { set_attempts(v); }
    void set_analyzed_required_strategy(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.analyzed_required_strategy = v; }
    void set_required_strategy_hits(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.required_strategy_hits = v; }
    void set_written_required_strategy(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.written_required_strategy = v; }
    void set_accepted(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.accepted = v; }
    void set_written(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.written = v; }
    void set_rejected(uint64_t v) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.rejected = v; }
    void set_totals_snapshot(const MonitorTotalsSnapshot& snapshot) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_ = snapshot; }
    void add_reseed(uint64_t inc = 1) { std::unique_lock<std::shared_mutex> lock(totals_mu_); totals_.reseeds += inc; }

    void set_worker_row(size_t worker_idx, const WorkerRow& row) {
        std::lock_guard<std::mutex> lock(workers_mu_);
        if (worker_idx >= workers_.size()) {
            workers_.resize(worker_idx + 1);
        }
        workers_[worker_idx] = row;
    }

    void update_strategy_row(const StrategyRow& row) {
        std::lock_guard<std::mutex> lock(strategies_mu_);
        for (auto& item : strategies_) {
            if (item.strategy == row.strategy && item.lvl == row.lvl) {
                item = row;
                return;
            }
        }
        strategies_.push_back(row);
    }

    void add_avg_clues_per_level(int lvl, double clues) {
        if (lvl < 0 || lvl >= static_cast<int>(clues_sum_.size())) {
            return;
        }
        std::lock_guard<std::mutex> lock(clues_mu_);
        clues_sum_[lvl] += clues;
        clues_count_[lvl] += 1;
    }

    void set_background_status(const std::string& status) {
        std::lock_guard<std::mutex> lock(status_mu_);
        background_status_ = status;
    }

    void set_profiler_summary(const std::string& summary) {
        std::lock_guard<std::mutex> lock(profiler_mu_);
        profiler_summary_ = summary;
    }

    void set_grid_info(int box_rows, int box_cols, int difficulty_level) {
        box_rows_ = box_rows;
        box_cols_ = box_cols;
        difficulty_level_ = difficulty_level;
    }

    void start_ui_thread(int refresh_rate_ms) {
        stop_ui_thread();
        ui_thread_ = std::jthread([this, refresh_rate_ms](std::stop_token st) {
            const auto interval = std::chrono::milliseconds(std::max(100, refresh_rate_ms));
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(interval);
            }
        });
    }

    void stop_ui_thread() {
        if (ui_thread_.joinable()) {
            ui_thread_.request_stop();
            ui_thread_.join();
        }
    }

    std::string snapshot_text() const {
        std::ostringstream out;
        MonitorTotalsSnapshot t{};
        {
            std::shared_lock<std::shared_mutex> lock(totals_mu_);
            t = totals_;
        }

        const auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_tp_).count();

        out << "=== Stats (console) ===\n";
        out << "target=" << t.target
            << " accepted=" << t.accepted
            << " written=" << t.written
            << " attempts=" << t.attempts
            << " rejected=" << t.rejected
            << " workers=" << t.active_workers
            << " elapsed_s=" << elapsed_s
            << "\n";

        {
            std::lock_guard<std::mutex> lock(workers_mu_);
            for (const auto& w : workers_) {
                if (w.worker.empty()) continue;
                out << "[" << w.worker << "] status=" << w.status
                    << " clues=" << w.clues
                    << " applied=" << w.applied
                    << " solved_ms=" << std::fixed << std::setprecision(3) << w.stage_solved_ms
                    << " dig_ms=" << std::fixed << std::setprecision(3) << w.stage_dig_ms
                    << " logic_ms=" << std::fixed << std::setprecision(3) << w.stage_logic_ms
                    << " uniq_ms=" << std::fixed << std::setprecision(3) << w.stage_uniqueness_ms
                    << "\n";
            }
        }

        {
            std::lock_guard<std::mutex> lock(status_mu_);
            if (!background_status_.empty()) {
                out << background_status_ << "\n";
            }
        }

        return out.str();
    }

private:
    alignas(64) MonitorTotalsSnapshot totals_{};
    mutable std::shared_mutex totals_mu_;

    std::chrono::steady_clock::time_point start_tp_;

    mutable std::mutex workers_mu_;
    std::vector<WorkerRow> workers_;

    mutable std::mutex strategies_mu_;
    std::vector<StrategyRow> strategies_;

    mutable std::mutex clues_mu_;
    std::array<double, 10> clues_sum_{};
    std::array<uint64_t, 10> clues_count_{};

    mutable std::mutex status_mu_;
    std::string background_status_;

    mutable std::mutex profiler_mu_;
    std::string profiler_summary_;

    int box_rows_ = 0;
    int box_cols_ = 0;
    int difficulty_level_ = 0;

    std::jthread ui_thread_;
};

} // namespace sudoku_hpc
