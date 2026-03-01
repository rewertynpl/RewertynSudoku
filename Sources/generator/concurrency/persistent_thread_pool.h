// ============================================================================
// SUDOKU HPC - CONCURRENCY
// Moduł: persistent_thread_pool.h
// Opis: Ekstremalnie niskolatencyjna pula wątków typu "Start & Wait".
//       Zoptymalizowana w C++20 poprzez użycie std::atomic::wait/notify.
//       Rozwiązany problem "missed wake-up".
// ============================================================================

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace sudoku_hpc::concurrency {

class PersistentThreadPool {
public:
    static PersistentThreadPool& instance() {
        static PersistentThreadPool pool;
        return pool;
    }

    // Wykonuje zadanie fn() w thread_count wątkach. Blokuje wątek wywołujący 
    // dopóki ostatni worker nie zgłosi zakończenia zadania.
    void run(int task_count, const std::function<void(int)>& fn) {
        if (task_count <= 0) {
            return;
        }

        std::lock_guard<std::mutex> run_guard(run_mu_);
        ensure_workers(task_count);

        job_fn_ = &fn;
        task_count_.store(task_count, std::memory_order_relaxed);
        next_task_.store(0, std::memory_order_relaxed);
        
        // Zapisz ile zadań pozostało przed budzeniem, aby zapobiec wybudzeniu 
        // zarządcy zanim workerzy zaczną przetwarzać
        remaining_.store(task_count, std::memory_order_release);
        
        // Zbudź workery korzystając z szybkiego mechanizmu w C++20
        epoch_.fetch_add(1, std::memory_order_acq_rel);
        epoch_.notify_all();

        // Czekaj bez aktywnego spinowania aż ostatni worker zakończy zadanie
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
        // Zabezpiecza przed "missed wake-up". Nawet jeśli ten worker 
        // wystartował z opóźnieniem (np. po tym jak główny wątek wywołał już run() 
        // i podbił epoch_), to `seen_epoch` równe 0 zmusza go do dogonienia `epoch_`.
        uint64_t seen_epoch = 0;

        while (true) {
            if (stop_.load(std::memory_order_acquire)) return;
            
            // Czekamy na zmianę epoki zadaniowej. Zabezpiecza przed spurious wakeups.
            epoch_.wait(seen_epoch, std::memory_order_acquire);
            seen_epoch = epoch_.load(std::memory_order_acquire);
            
            if (stop_.load(std::memory_order_acquire)) return;

            // Worker konsumuje paczki zadań, aż licznik next_task dojdzie do limitu task_count.
            while (true) {
                const int idx = next_task_.fetch_add(1, std::memory_order_relaxed);
                if (idx >= task_count_.load(std::memory_order_relaxed)) {
                    break;
                }
                
                // Uruchomienie wyznaczonego zadania.
                (*job_fn_)(idx);
                
                // Atomowe odjęcie zadania ze wskaźnikiem na zakończenie całego bloku (1 => to był ostatni)
                if (remaining_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    // Budzimy główny wątek z funkcji run()
                    remaining_.notify_one();
                }
            }
        }
    }

    std::mutex run_mu_;
    std::vector<std::thread> workers_;
    
    // Pola współdzielone do delegacji zlecenia (tylko wskaźnik, unika kopiowania function)
    const std::function<void(int)>* job_fn_{nullptr};
    
    // Liczniki blokowane do oddzielnych warstw Cache'a
    alignas(64) std::atomic<int> task_count_{0};
    alignas(64) std::atomic<int> next_task_{0};
    alignas(64) std::atomic<int> remaining_{0};
    
    // Zarządzanie stanem i wybudzaniem
    alignas(64) std::atomic<uint64_t> epoch_{0};
    alignas(64) std::atomic<bool> stop_{false};
};

} // namespace sudoku_hpc::concurrency