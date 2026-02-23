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
    long long seed = 0;
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
    // Budget settings for display
    int reseed_interval_s = 0;
    double attempt_time_budget_s = 0.0;
    uint64_t attempt_node_budget = 0;
    // Mikroprofiling metrics
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
        enable_ansi();
        log_info("ConsoleStatsMonitor", "constructed - will refresh every 8s in CLI mode");
    }

    ~ConsoleStatsMonitor() {
        stop_ui_thread();
        log_info("ConsoleStatsMonitor", "destroyed");
    }

    // ========================================================================
    // TELEMETRIA - główne metryki generatora
    // ========================================================================
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
            const size_t old_size = workers_.size();
            workers_.resize(static_cast<size_t>(n));
            for (size_t i = old_size; i < workers_.size(); ++i) {
                std::ostringstream name;
                name << "worker_" << std::setw(2) << std::setfill('0') << i;
                workers_[i].worker = name.str();
            }
        }
    }

    void set_attempts(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.attempts = v;
    }

    // ========================================================================
    // TELEMETRIA STRATEGII - Required Strategy Contract
    // ========================================================================
    void set_attempts_total(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.attempts = v;
    }

    void set_analyzed_required_strategy(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.analyzed_required_strategy = v;
    }

    void set_required_strategy_hits(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.required_strategy_hits = v;
    }

    void set_written_required_strategy(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.written_required_strategy = v;
    }

    void set_accepted(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.accepted = v;
    }

    void set_written(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.written = v;
    }

    void set_rejected(uint64_t v) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.rejected = v;
    }

    void set_totals_snapshot(const MonitorTotalsSnapshot& snapshot) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_ = snapshot;
    }

    void add_reseed(uint64_t inc = 1) {
        std::unique_lock<std::shared_mutex> lock(totals_mu_);
        totals_.reseeds += inc;
    }

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

    // Microprofiling update
    void set_profiler_summary(const std::string& summary) {
        std::lock_guard<std::mutex> lock(profiler_mu_);
        profiler_summary_ = summary;
    }

    // Grid info setter
    void set_grid_info(int box_rows, int box_cols, int difficulty_level) {
        box_rows_ = box_rows;
        box_cols_ = box_cols;
        difficulty_level_ = difficulty_level;
    }

    void start_ui_thread(int refresh_rate_ms) {
        stop_ui_thread();
        log_info("ConsoleStatsMonitor", "start_ui_thread refresh_ms=" + std::to_string(refresh_rate_ms));
        ui_thread_ = std::jthread([this, refresh_rate_ms](std::stop_token st) {
            log_info("ConsoleStatsMonitor", "ui_thread started with interval=" + std::to_string(refresh_rate_ms) + "ms");
            using namespace std::chrono_literals;
            const auto interval = std::chrono::milliseconds(refresh_rate_ms);
            uint64_t render_count = 0;
            while (!st.stop_requested()) {
                render_once();
                ++render_count;
                if ((render_count % 10) == 0) {
                    log_info("ConsoleStatsMonitor", "ui_thread rendered " + std::to_string(render_count) + " frames");
                }
                std::this_thread::sleep_for(interval);
            }
            log_info("ConsoleStatsMonitor", "ui_thread stopping after " + std::to_string(render_count) + " renders");
            render_once();
            log_info("ConsoleStatsMonitor", "ui_thread finished");
        });
    }

    void stop_ui_thread() {
        if (ui_thread_.joinable()) {
            log_info("ConsoleStatsMonitor", "stop_ui_thread requested");
            ui_thread_.request_stop();
            ui_thread_.join();
            log_info("ConsoleStatsMonitor", "stop_ui_thread joined");
        }
    }

    std::string snapshot_text() const {
        return compose_snapshot_text();
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
    std::string background_status_ = "Generowanie: init";

    mutable std::mutex profiler_mu_;
    std::string profiler_summary_ = "";

    // Grid info
    int box_rows_ = 0;
    int box_cols_ = 0;
    int difficulty_level_ = 0;

    std::jthread ui_thread_;
    bool ansi_enabled_ = false;
    mutable std::mutex render_mu_;

    void enable_ansi() {
#ifdef _WIN32
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) {
            log_warn("ConsoleStatsMonitor", "GetStdHandle(STD_OUTPUT_HANDLE) failed");
            return;
        }
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) {
            log_warn("ConsoleStatsMonitor", "GetConsoleMode failed - console VT disabled");
            return;
        }
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (SetConsoleMode(h, mode)) {
            ansi_enabled_ = true;
            log_info("ConsoleStatsMonitor", "ANSI VT enabled");
        } else {
            log_warn("ConsoleStatsMonitor", "SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING) failed");
        }
#else
        ansi_enabled_ = true;
#endif
    }

#ifdef _WIN32
    bool render_winapi(const std::string& text) const {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == nullptr || h == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) {
            return false;
        }

        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!GetConsoleScreenBufferInfo(h, &csbi)) {
            return false;
        }

        const COORD home{0, 0};
        const DWORD cells = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
        DWORD n = 0;
        if (!FillConsoleOutputCharacterA(h, ' ', cells, home, &n)) {
            return false;
        }
        if (!FillConsoleOutputAttribute(h, csbi.wAttributes, cells, home, &n)) {
            return false;
        }
        if (!SetConsoleCursorPosition(h, home)) {
            return false;
        }
        if (text.empty()) {
            return true;
        }

        DWORD written = 0;
        if (!WriteConsoleA(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)) {
            return false;
        }
        return true;
    }
#endif

    std::string render_global_line() const {
        using namespace std::chrono;
        MonitorTotalsSnapshot totals_snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(totals_mu_);
            totals_snapshot = totals_;
        }
        const double elapsed_h = duration<double>(steady_clock::now() - start_tp_).count() / 3600.0;
        const double hourly =
            elapsed_h > 0.0 ? static_cast<double>(totals_snapshot.accepted) / elapsed_h : 0.0;
        std::ostringstream oss;
        // Grid size and difficulty info
        const int n = box_rows_ * box_cols_;
        if (n > 0) {
            oss << "[" << n << "x" << n << " L" << difficulty_level_ << "] ";
        }
        oss << "accepted=" << totals_snapshot.accepted << "/" << totals_snapshot.target
            << " written=" << totals_snapshot.written
            << " attempts_total=" << totals_snapshot.attempts
            << " analyzed_required_strategy=" << totals_snapshot.analyzed_required_strategy
            << " required_strategy_hit/use=" << totals_snapshot.required_strategy_hits << "/"
            << totals_snapshot.analyzed_required_strategy
            << " written_required_strategy=" << totals_snapshot.written_required_strategy
            << " rejected=" << totals_snapshot.rejected
            << " " << std::fixed << std::setprecision(2) << hourly << "/h"
            << " active_workers=" << totals_snapshot.active_workers
            << " reseeds=" << totals_snapshot.reseeds;
        return oss.str();
    }

    std::string render_worker_table() const {
        std::vector<std::vector<std::string>> rows;
        const auto now_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
        {
            std::lock_guard<std::mutex> lock(workers_mu_);
            rows.reserve(workers_.size());
            for (const auto& w : workers_) {
                double reset_lag = w.reset_lag;
                double lag_max = w.lag_max;
                double reset_in_s = w.reset_in_s;
                if (w.reseed_interval_s > 0 && w.last_reseed_steady_ns > 0 && now_ns >= w.last_reseed_steady_ns) {
                    const double elapsed_s =
                        static_cast<double>(now_ns - w.last_reseed_steady_ns) / 1'000'000'000.0;
                    const double interval_s = static_cast<double>(w.reseed_interval_s);
                    const double lag = std::max(0.0, elapsed_s - interval_s);
                    reset_lag = lag;
                    lag_max = std::max(lag_max, lag);
                    reset_in_s = std::max(0.0, interval_s - elapsed_s);
                }
                rows.push_back(
                    {
                        w.worker,
                        std::to_string(w.clues),
                        std::to_string(w.seed),
                        std::to_string(w.resets),
                        std::to_string(w.applied),
                        format_fixed(reset_lag, 2),
                        format_fixed(lag_max, 2),
                        format_fixed(reset_in_s, 2),
                        w.status,
                        std::to_string(w.dead_ends),
                        std::to_string(w.max_depth),
                        format_fixed(w.avg_node_ms, 4),
                        format_fixed(w.cpu_load_pct, 1),
                        format_fixed(w.ram_current_mb, 1),
                        format_fixed(w.ram_peak_mb, 1),
                        std::to_string(w.reseed_interval_s),
                        format_fixed(w.attempt_time_budget_s, 1),
                        std::to_string(w.attempt_node_budget),
                        format_fixed(w.stage_solved_ms, 4),
                        format_fixed(w.stage_dig_ms, 4),
                        format_fixed(w.stage_logic_ms, 4),
                        format_fixed(w.stage_uniqueness_ms, 4),
                        format_fixed(w.avg_attempt_ms, 4),
                        format_fixed(w.success_rate_pct, 2),
                        std::to_string(w.backtrack_count),
                    });
            }
        }
        return render_table(
            {
                {"worker", Align::Left},
                {"clues", Align::Right},
                {"seed", Align::Right},
                {"resets", Align::Right},
                {"applied", Align::Right},
                {"reset_lag", Align::Right},
                {"lag_max", Align::Right},
                {"reset_in_s", Align::Right},
                {"status", Align::Left},
                {"dead_ends", Align::Right},
                {"max_depth", Align::Right},
                {"avg_node_ms", Align::Right},
                {"cpu_%", Align::Right},
                {"ram_mb", Align::Right},
                {"peak_mb", Align::Right},
                {"reseed_s", Align::Right},
                {"time_s", Align::Right},
                {"nodes", Align::Right},
                {"solved_ms", Align::Right},
                {"dig_ms", Align::Right},
                {"logic_ms", Align::Right},
                {"uniq_ms", Align::Right},
                {"avg_att_ms", Align::Right},
                {"success_%", Align::Right},
                {"backtracks", Align::Right},
            },
            rows);
    }

    std::string render_strategy_table() const {
        std::vector<std::vector<std::string>> rows;
        {
            std::lock_guard<std::mutex> lock(strategies_mu_);
            rows.reserve(strategies_.size());
            for (const auto& s : strategies_) {
                rows.push_back(
                    {
                        s.strategy,
                        std::to_string(s.lvl),
                        std::to_string(s.max_attempts),
                        std::to_string(s.analyzed),
                        std::to_string(s.required_strategy_hits) + "/" + std::to_string(s.analyzed),
                        format_fixed(s.analyzed_per_min, 2),
                        std::to_string(s.est_5min),
                        std::to_string(s.written),
                        format_fixed(s.avg_clues, 2),
                    });
            }
        }
        return render_table(
            {
                {"strategy", Align::Left},
                {"lvl", Align::Right},
                {"max_attempts", Align::Right},
                {"analyzed", Align::Right},
                {"hit/use", Align::Right},
                {"analyzed/min", Align::Right},
                {"est_5min", Align::Right},
                {"written", Align::Right},
                {"avg_clues", Align::Right},
            },
            rows);
    }

    std::string render_avg_clues() const {
        std::ostringstream oss;
        oss << "avg_clues_per_level: ";
        std::lock_guard<std::mutex> lock(clues_mu_);
        bool any = false;
        for (size_t lvl = 1; lvl < clues_sum_.size(); ++lvl) {
            if (clues_count_[lvl] == 0) {
                continue;
            }
            if (any) {
                oss << " | ";
            }
            any = true;
            oss << "L" << lvl << "=" << format_fixed(clues_sum_[lvl] / static_cast<double>(clues_count_[lvl]), 2);
        }
        if (!any) {
            oss << "(brak)";
        }
        return oss.str();
    }

    std::string compose_snapshot_text() const {
        std::ostringstream out;
        out << "=== Statystyki Generowania (console) ===\n";
        out << render_global_line() << "\n\n";
        out << render_worker_table() << "\n\n";
        out << "=== Required Strategy Monitor (10s) ===\n";
        out << render_strategy_table() << "\n";
        out << render_avg_clues() << "\n";
        {
            std::lock_guard<std::mutex> lock(status_mu_);
            out << background_status_ << "\n";
        }
        {
            std::lock_guard<std::mutex> lock(profiler_mu_);
            if (!profiler_summary_.empty()) {
                out << "\n=== Mikroprofiling ===\n";
                out << profiler_summary_ << "\n";
            }
        }
        return out.str();
    }

    void render_once() const {
        try {
            const std::string text = compose_snapshot_text();
            std::lock_guard<std::mutex> lock(render_mu_);
            // CLI mode: just append text with newline, don't clear screen
            log_info("ConsoleStatsMonitor", "render_once - outputting " + std::to_string(text.size()) + " bytes");
            std::cout << "\n" << text << std::flush;
        } catch (const std::exception& ex) {
            log_error("ConsoleStatsMonitor", std::string("render_once exception: ") + ex.what());
        } catch (...) {
            log_error("ConsoleStatsMonitor", "render_once unknown exception");
        }
    }
};

} // namespace sudoku_hpc
