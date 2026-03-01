// ============================================================================
// SUDOKU HPC - CONCURRENCY
// Moduł: telemetry_ring.h
// Opis: Wysoce wydajne, bezblokadowe (Lock-Free) kolejki pierścieniowe MPSC 
//       do przesyłania statystyk telemetrii oraz danych wynikowych z workerów.
//       Rozmiary chronione cache-line alignmentem (false sharing avoidance).
// ============================================================================

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace sudoku_hpc::concurrency {

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

// ============================================================================
// KOLEJKA TELEMETRII (MPSC RING BUFFER)
// ============================================================================
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
            // Zbyt wolny konsument - wyrzucamy próbkę
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
    
    // Zapobiega falszywemu współdzieleniu między wątkami zrzucając head_ i tail_ do oddzielnych cache-lines
    alignas(64) std::atomic<uint64_t> head_{0};
    alignas(64) std::atomic<uint64_t> tail_{0};
    alignas(64) std::atomic<uint64_t> dropped_{0};
};


// ============================================================================
// KOLEJKA WYNIKOWA (MPSC RING BUFFER)
// ============================================================================
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

} // namespace sudoku_hpc::concurrency