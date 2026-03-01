#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../config/run_config.h"
#include "../core/geometry.h"
#include "../monitor.h"
#include "../utils/logging.h"
#include "../generator/generator_facade.h"
#include "../generator/post_processing/vip_scoring.h"

namespace sudoku_hpc {

inline void accumulate_reject_reason(GenerateRunResult& r, RejectReason reason, bool timed_out) {
    if (timed_out) {
        ++r.reject_uniqueness_budget;
    }
    switch (reason) {
        case RejectReason::Prefilter: ++r.reject_prefilter; break;
        case RejectReason::Logic: ++r.reject_logic; break;
        case RejectReason::Uniqueness: ++r.reject_uniqueness; break;
        case RejectReason::Strategy: ++r.reject_strategy; break;
        case RejectReason::Replay: ++r.reject_replay; break;
        case RejectReason::DistributionBias: ++r.reject_distribution_bias; break;
        case RejectReason::UniquenessBudget: ++r.reject_uniqueness_budget; break;
        case RejectReason::None: break;
    }
}

inline GenerateRunResult run_generic_sudoku(
    const GenerateRunConfig& cfg,
    ConsoleStatsMonitor* monitor = nullptr,
    std::atomic<bool>* cancel_flag = nullptr,
    std::atomic<bool>* pause_flag = nullptr,
    std::function<void(uint64_t, uint64_t)> on_progress = nullptr,
    std::function<void(const std::string&)> on_log = nullptr) {

    using namespace std::chrono;

    GenerateRunResult result{};
    result.cpu_backend_selected = cfg.cpu_backend;

    GenericTopology topo;
    std::string topo_err;
    if (!build_generic_topology(cfg.box_rows, cfg.box_cols, topo, &topo_err)) {
        log_error("runner", "invalid geometry: " + topo_err);
        if (on_log) on_log("invalid geometry: " + topo_err);
        result.reject_logic = 1;
        result.rejected = 1;
        return result;
    }

    const int n = topo.n;
    const int nn = topo.nn;
    GenerateRunConfig run_cfg = cfg;

    if (run_cfg.min_clues <= 0 || run_cfg.max_clues <= 0 || run_cfg.max_clues < run_cfg.min_clues) {
        const ClueRange auto_range = resolve_auto_clue_range(run_cfg.box_rows, run_cfg.box_cols, run_cfg.difficulty_level_required, run_cfg.required_strategy);
        if (run_cfg.min_clues <= 0) run_cfg.min_clues = auto_range.min_clues;
        if (run_cfg.max_clues <= 0) run_cfg.max_clues = auto_range.max_clues;
        if (run_cfg.max_clues < run_cfg.min_clues) run_cfg.max_clues = run_cfg.min_clues;
    }
    run_cfg.min_clues = std::clamp(run_cfg.min_clues, 0, nn);
    run_cfg.max_clues = std::clamp(run_cfg.max_clues, run_cfg.min_clues, nn);

    if (run_cfg.fast_test_mode) {
        // Fast smoke profile: bounded runtime and relaxed heavy verification stages.
        run_cfg.enable_quality_contract = false;
        run_cfg.enable_distribution_filter = false;
        run_cfg.enable_replay_validation = false;
        run_cfg.require_unique = false;
        run_cfg.strict_logical = false;
        run_cfg.strict_canonical_strategies = false;
        run_cfg.allow_proxy_advanced = true;

        if (run_cfg.max_attempts == 0) {
            run_cfg.max_attempts = std::max<uint64_t>(32ULL, run_cfg.target_puzzles * 32ULL);
        }
        if (run_cfg.max_total_time_s == 0) {
            run_cfg.max_total_time_s = 20ULL;
        }
        if (run_cfg.attempt_time_budget_s <= 0.0) {
            run_cfg.attempt_time_budget_s = (run_cfg.difficulty_level_required >= 7) ? 1.2 : 0.7;
        }
        if (run_cfg.attempt_node_budget == 0) {
            const uint64_t suggested = suggest_attempt_node_budget(
                run_cfg.box_rows,
                run_cfg.box_cols,
                std::max(1, run_cfg.difficulty_level_required));
            run_cfg.attempt_node_budget = std::max<uint64_t>(20'000ULL, suggested / 8ULL);
        }
    }

    if (run_cfg.attempt_time_budget_s <= 0.0) {
        run_cfg.attempt_time_budget_s = 0.0;
    }

    const int hw = std::max(1u, std::thread::hardware_concurrency());
    const int worker_count = std::max(1, run_cfg.threads <= 0 ? hw : run_cfg.threads);

    std::filesystem::create_directories(run_cfg.output_folder);
    const std::filesystem::path output_path = std::filesystem::path(run_cfg.output_folder) / run_cfg.output_file;
    std::ofstream batch_out(output_path, std::ios::out | std::ios::app);
    if (!batch_out) {
        log_error("runner", "cannot open output file: " + output_path.string());
        if (on_log) on_log("cannot open output file: " + output_path.string());
        result.reject_logic = 1;
        result.rejected = 1;
        return result;
    }

    if (monitor != nullptr) {
        monitor->set_target(run_cfg.target_puzzles);
        monitor->set_active_workers(worker_count);
        monitor->set_grid_info(run_cfg.box_rows, run_cfg.box_cols, run_cfg.difficulty_level_required);
        monitor->set_background_status("runtime initialized");
    }

    std::mutex write_mu;
    std::mutex result_mu;

    std::atomic<uint64_t> accepted{0};
    std::atomic<uint64_t> written{0};
    std::atomic<uint64_t> attempts{0};

    std::atomic<uint64_t> uniqueness_calls{0};
    std::atomic<uint64_t> uniqueness_nodes{0};
    std::atomic<uint64_t> uniqueness_elapsed_ns{0};
    std::atomic<uint64_t> logic_steps_total{0};
    std::atomic<uint64_t> strategy_naked_use{0};
    std::atomic<uint64_t> strategy_naked_hit{0};
    std::atomic<uint64_t> strategy_hidden_use{0};
    std::atomic<uint64_t> strategy_hidden_hit{0};
    std::atomic<uint64_t> kernel_elapsed_ns{0};
    std::atomic<uint64_t> kernel_calls{0};

    const auto t0 = steady_clock::now();

    auto is_cancelled = [&]() -> bool {
        return (cancel_flag != nullptr) && cancel_flag->load(std::memory_order_relaxed);
    };

    auto is_paused = [&]() -> bool {
        return (pause_flag != nullptr) && pause_flag->load(std::memory_order_relaxed);
    };

    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(worker_count));

    for (int worker_idx = 0; worker_idx < worker_count; ++worker_idx) {
        workers.emplace_back([&, worker_idx]() {
            const uint64_t base_seed = (run_cfg.seed == 0)
                ? static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
                : run_cfg.seed;
            std::mt19937_64 rng(base_seed ^ (0x9E3779B97F4A7C15ULL + static_cast<uint64_t>(worker_idx) * 0x100000001B3ULL));

            core_engines::GenericSolvedKernel solved(
                core_engines::GenericSolvedKernel::backend_from_string(run_cfg.cpu_backend));
            core_engines::GenericQuickPrefilter prefilter;
            logic::GenericLogicCertify logic;
            core_engines::GenericUniquenessCounter uniq;

            uint64_t local_attempts = 0;
            uint64_t local_written = 0;

            while (true) {
                if (is_cancelled()) {
                    break;
                }

                if (run_cfg.max_total_time_s > 0) {
                    const auto elapsed = duration_cast<seconds>(steady_clock::now() - t0).count();
                    if (elapsed >= static_cast<long long>(run_cfg.max_total_time_s)) {
                        break;
                    }
                }

                while (is_paused() && !is_cancelled()) {
                    std::this_thread::sleep_for(milliseconds(20));
                }

                const uint64_t current_accepted = accepted.load(std::memory_order_relaxed);
                if (current_accepted >= run_cfg.target_puzzles) {
                    break;
                }

                if (run_cfg.max_attempts > 0) {
                    const uint64_t current_attempts = attempts.load(std::memory_order_relaxed);
                    if (current_attempts >= run_cfg.max_attempts) {
                        break;
                    }
                }

                ++local_attempts;
                attempts.fetch_add(1, std::memory_order_relaxed);

                generator::GenericPuzzleCandidate candidate;
                RejectReason reason = RejectReason::None;
                RequiredStrategyAttemptInfo strategy_info{};
                generator::AttemptPerfStats perf{};
                bool timed_out = false;

                const bool ok = generator::generate_one_generic(
                    run_cfg,
                    topo,
                    rng,
                    candidate,
                    reason,
                    strategy_info,
                    solved,
                    prefilter,
                    logic,
                    uniq,
                    nullptr,
                    &timed_out,
                    cancel_flag,
                    pause_flag,
                    nullptr,
                    nullptr,
                    nullptr,
                    &perf);

                kernel_elapsed_ns.fetch_add(perf.solved_elapsed_ns + perf.dig_elapsed_ns, std::memory_order_relaxed);
                kernel_calls.fetch_add(1, std::memory_order_relaxed);

                uniqueness_calls.fetch_add(perf.uniqueness_calls, std::memory_order_relaxed);
                uniqueness_nodes.fetch_add(perf.uniqueness_nodes, std::memory_order_relaxed);
                uniqueness_elapsed_ns.fetch_add(perf.uniqueness_elapsed_ns, std::memory_order_relaxed);
                logic_steps_total.fetch_add(perf.logic_steps, std::memory_order_relaxed);
                strategy_naked_use.fetch_add(perf.strategy_naked_use, std::memory_order_relaxed);
                strategy_naked_hit.fetch_add(perf.strategy_naked_hit, std::memory_order_relaxed);
                strategy_hidden_use.fetch_add(perf.strategy_hidden_use, std::memory_order_relaxed);
                strategy_hidden_hit.fetch_add(perf.strategy_hidden_hit, std::memory_order_relaxed);

                if (ok) {
                    uint64_t accepted_idx = 0;
                    bool slot_acquired = false;
                    while (true) {
                        uint64_t cur = accepted.load(std::memory_order_relaxed);
                        if (cur >= run_cfg.target_puzzles) {
                            slot_acquired = false;
                            break;
                        }
                        if (accepted.compare_exchange_weak(cur, cur + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                            accepted_idx = cur + 1;
                            slot_acquired = true;
                            break;
                        }
                    }
                    if (!slot_acquired) {
                        continue;
                    }

                    const std::string line = generator::serialize_line_generic(
                        (base_seed + local_attempts),
                        run_cfg,
                        candidate,
                        topo.nn);

                    {
                        std::lock_guard<std::mutex> lock(write_mu);
                        batch_out << line << '\n';
                        if (run_cfg.write_individual_files) {
                            const std::filesystem::path file_path =
                                std::filesystem::path(run_cfg.output_folder) /
                                ("sudoku_" + std::to_string(accepted_idx) + ".txt");
                            std::ofstream one(file_path, std::ios::out | std::ios::trunc);
                            if (one) {
                                one << line << '\n';
                            }
                        }
                    }

                    ++local_written;
                    written.fetch_add(1, std::memory_order_relaxed);

                    if (on_progress) {
                        on_progress(accepted_idx, run_cfg.target_puzzles);
                    }

                    if (on_log && (accepted_idx % 10ULL == 0ULL || accepted_idx == run_cfg.target_puzzles)) {
                        on_log("accepted=" + std::to_string(accepted_idx) + "/" + std::to_string(run_cfg.target_puzzles));
                    }
                } else {
                    std::lock_guard<std::mutex> lock(result_mu);
                    ++result.rejected;
                    accumulate_reject_reason(result, reason, timed_out);
                }

                if (monitor != nullptr && ((local_attempts % 16ULL) == 0ULL || local_written > 0)) {
                    monitor->set_attempts(attempts.load(std::memory_order_relaxed));
                    monitor->set_accepted(accepted.load(std::memory_order_relaxed));
                    monitor->set_written(written.load(std::memory_order_relaxed));
                    monitor->set_rejected(result.rejected);

                    WorkerRow row{};
                    row.worker = "worker_" + std::to_string(worker_idx);
                    row.clues = candidate.clues;
                    row.seed = base_seed;
                    row.applied = local_attempts;
                    row.status = is_paused() ? "paused" : "running";
                    row.reseed_interval_s = run_cfg.reseed_interval_s;
                    row.attempt_time_budget_s = run_cfg.attempt_time_budget_s;
                    row.attempt_node_budget = run_cfg.attempt_node_budget;
                    row.stage_solved_ms = static_cast<double>(perf.solved_elapsed_ns) / 1e6;
                    row.stage_dig_ms = static_cast<double>(perf.dig_elapsed_ns) / 1e6;
                    row.stage_prefilter_ms = static_cast<double>(perf.prefilter_elapsed_ns) / 1e6;
                    row.stage_logic_ms = static_cast<double>(perf.logic_elapsed_ns) / 1e6;
                    row.stage_uniqueness_ms = static_cast<double>(perf.uniqueness_elapsed_ns) / 1e6;
                    monitor->set_worker_row(static_cast<size_t>(worker_idx), row);
                }
            }

            if (monitor != nullptr) {
                WorkerRow row{};
                row.worker = "worker_" + std::to_string(worker_idx);
                row.applied = local_attempts;
                row.status = "done";
                monitor->set_worker_row(static_cast<size_t>(worker_idx), row);
            }
        });
    }

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    result.accepted = accepted.load(std::memory_order_relaxed);
    result.written = written.load(std::memory_order_relaxed);
    result.attempts = attempts.load(std::memory_order_relaxed);

    result.uniqueness_calls = uniqueness_calls.load(std::memory_order_relaxed);
    result.uniqueness_nodes = uniqueness_nodes.load(std::memory_order_relaxed);
    result.uniqueness_elapsed_ms = static_cast<double>(uniqueness_elapsed_ns.load(std::memory_order_relaxed)) / 1e6;
    result.uniqueness_avg_ms = (result.uniqueness_calls > 0)
        ? (result.uniqueness_elapsed_ms / static_cast<double>(result.uniqueness_calls))
        : 0.0;

    result.kernel_calls = kernel_calls.load(std::memory_order_relaxed);
    result.kernel_time_ms = static_cast<double>(kernel_elapsed_ns.load(std::memory_order_relaxed)) / 1e6;
    result.logic_steps_total = logic_steps_total.load(std::memory_order_relaxed);
    result.strategy_naked_use = strategy_naked_use.load(std::memory_order_relaxed);
    result.strategy_naked_hit = strategy_naked_hit.load(std::memory_order_relaxed);
    result.strategy_hidden_use = strategy_hidden_use.load(std::memory_order_relaxed);
    result.strategy_hidden_hit = strategy_hidden_hit.load(std::memory_order_relaxed);

    const double asymmetry_ratio = static_cast<double>(std::max(run_cfg.box_rows, run_cfg.box_cols)) /
                                   static_cast<double>(std::max(1, std::min(run_cfg.box_rows, run_cfg.box_cols)));
    result.asymmetry_efficiency_index = asymmetry_ratio;
    result.backend_efficiency_score = (result.kernel_time_ms > 0.0)
        ? static_cast<double>(result.accepted) / (result.kernel_time_ms / 1000.0)
        : 0.0;

    const auto elapsed = duration_cast<duration<double>>(steady_clock::now() - t0).count();
    result.elapsed_s = elapsed;
    result.accepted_per_sec = (elapsed > 0.0) ? static_cast<double>(result.accepted) / elapsed : 0.0;

    const auto vip_target = post_processing::resolve_vip_grade_target_for_geometry(run_cfg);
    result.vip_score = post_processing::compute_vip_score(result, run_cfg, asymmetry_ratio);
    result.vip_grade = post_processing::vip_grade_from_score(result.vip_score);
    result.vip_contract_ok = post_processing::vip_contract_passed(result.vip_score, vip_target);
    result.vip_contract_fail_reason = result.vip_contract_ok ? "" : ("required=" + vip_target + ", actual=" + result.vip_grade);

    const std::string sig_raw =
        std::to_string(result.accepted) + ":" +
        std::to_string(result.written) + ":" +
        std::to_string(result.attempts) + ":" +
        std::to_string(result.uniqueness_nodes) + ":" +
        std::to_string(run_cfg.box_rows) + "x" + std::to_string(run_cfg.box_cols);
    const size_t h1 = std::hash<std::string>{}(sig_raw);
    const size_t h2 = std::hash<std::string>{}(sig_raw + ":v2");
    result.premium_signature = std::to_string(static_cast<unsigned long long>(h1));
    result.premium_signature_v2 = std::to_string(static_cast<unsigned long long>(h2));

    if (monitor != nullptr) {
        monitor->set_attempts(result.attempts);
        monitor->set_accepted(result.accepted);
        monitor->set_written(result.written);
        monitor->set_rejected(result.rejected);
        monitor->set_background_status("done accepted=" + std::to_string(result.accepted) + " written=" + std::to_string(result.written));
    }

    log_info(
        "runner",
        "done accepted=" + std::to_string(result.accepted) +
        " written=" + std::to_string(result.written) +
        " attempts=" + std::to_string(result.attempts));

    return result;
}

} // namespace sudoku_hpc
