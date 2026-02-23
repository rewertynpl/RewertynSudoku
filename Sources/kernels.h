//Author copyright Marcin Matysek (Rewertyn)

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
#include <bit>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace sudoku_hpc {

#if defined(__GNUC__) || defined(__clang__)
#define SUDOKU_HOT_INLINE inline __attribute__((always_inline))
#define SUDOKU_LIKELY(x) (__builtin_expect(!!(x), 1))
#define SUDOKU_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define SUDOKU_HOT_INLINE inline
#define SUDOKU_LIKELY(x) (x)
#define SUDOKU_UNLIKELY(x) (x)
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#define SUDOKU_TARGET_AVX2 __attribute__((target("avx2")))
#define SUDOKU_TARGET_AVX512BW __attribute__((target("avx512f,avx512bw")))
#define SUDOKU_TARGET_AVX512VPOPCNT __attribute__((target("avx512f,avx512bw,avx512vpopcntdq")))
#define SUDOKU_TARGET_BMI2 __attribute__((target("bmi,bmi2")))
#else
#define SUDOKU_TARGET_AVX2
#define SUDOKU_TARGET_AVX512BW
#define SUDOKU_TARGET_AVX512VPOPCNT
#define SUDOKU_TARGET_BMI2
#endif

enum class RejectReason {
    None,
    Prefilter,
    Logic,
    Uniqueness,
    Strategy,
    Replay,
    DistributionBias,
    UniquenessBudget
};

struct RequiredStrategyAttemptInfo {
    bool analyzed_required_strategy = false;
    bool required_strategy_use_confirmed = false;
    bool required_strategy_hit_confirmed = false;
    bool matched_required_strategy = false;
};

struct SearchAbortControl {
    bool time_enabled = false;
    bool node_enabled = false;
    std::chrono::steady_clock::time_point deadline{};
    uint64_t node_limit = 0;
    uint64_t nodes = 0;
    bool aborted_by_time = false;
    bool aborted_by_nodes = false;
    bool aborted_by_force = false;
    bool aborted_by_pause = false;
    const std::atomic<bool>* force_abort_ptr = nullptr;
    const std::atomic<bool>* cancel_ptr = nullptr;
    const std::atomic<bool>* pause_ptr = nullptr;

    SUDOKU_HOT_INLINE bool step() {
        const uint64_t n = ++nodes;
        if (SUDOKU_UNLIKELY(node_enabled && n > node_limit)) {
            aborted_by_nodes = true;
            return false;
        }
        if ((n & 511ULL) != 0ULL) {
            return true;
        }
        if (SUDOKU_LIKELY(cancel_ptr == nullptr && pause_ptr == nullptr && force_abort_ptr == nullptr && !time_enabled)) {
            return true;
        }
        if (cancel_ptr != nullptr && cancel_ptr->load(std::memory_order_relaxed)) {
            aborted_by_force = true;
            return false;
        }
        if (pause_ptr != nullptr && pause_ptr->load(std::memory_order_relaxed)) {
            aborted_by_pause = true;
            return false;
        }
        if (force_abort_ptr != nullptr && force_abort_ptr->load(std::memory_order_relaxed)) {
            aborted_by_force = true;
            return false;
        }
        if (time_enabled && std::chrono::steady_clock::now() >= deadline) {
            aborted_by_time = true;
            return false;
        }
        return true;
    }

    bool aborted() const {
        return aborted_by_time || aborted_by_nodes || aborted_by_force || aborted_by_pause;
    }
};

#if defined(__x86_64__) || defined(__i386__)
SUDOKU_TARGET_BMI2 static int shuffled_digits_from_mask_bmi2(uint64_t mask, std::mt19937_64& rng, int* out_digits) {
    const int count = static_cast<int>(std::popcount(mask));
    if (count == 0) {
        return 0;
    }
    if (count <= 2) {
        const uint64_t dense = _pext_u64(mask, mask);
        const uint64_t a = _pdep_u64(1ULL & dense, mask);
        out_digits[0] = static_cast<int>(_tzcnt_u64(a)) + 1;
        if (count == 2) {
            const uint64_t b = _pdep_u64((2ULL & dense), mask);
            out_digits[1] = static_cast<int>(_tzcnt_u64(b)) + 1;
            if ((rng() & 1ULL) != 0ULL) {
                std::swap(out_digits[0], out_digits[1]);
            }
        }
        return count;
    }
    if (count > 8) {
        int out_count = 0;
        while (mask != 0ULL) {
            const uint64_t bit = _blsi_u64(mask);
            out_digits[out_count++] = static_cast<int>(_tzcnt_u64(bit)) + 1;
            mask = _blsr_u64(mask);
        }
        for (int i = out_count - 1; i > 0; --i) {
            const int j = static_cast<int>(rng() % static_cast<uint64_t>(i + 1));
            std::swap(out_digits[i], out_digits[j]);
        }
        return out_count;
    }
    int ranks[16];
    for (int i = 0; i < count; ++i) {
        ranks[i] = i;
    }
    for (int i = count - 1; i > 0; --i) {
        const int j = static_cast<int>(rng() % static_cast<uint64_t>(i + 1));
        std::swap(ranks[i], ranks[j]);
    }
    const uint64_t dense = _pext_u64(mask, mask); // contiguous candidate domain bits [0..count)
    for (int i = 0; i < count; ++i) {
        const uint64_t compact_pick = (1ULL << ranks[i]) & dense;
        const uint64_t lifted = _pdep_u64(compact_pick, mask);
        out_digits[i] = static_cast<int>(_tzcnt_u64(lifted)) + 1;
    }
    return count;
}
#endif

static int shuffled_digits_from_mask(uint64_t mask, std::mt19937_64& rng, int* out_digits) {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    if (__builtin_cpu_supports("bmi2")) {
        return shuffled_digits_from_mask_bmi2(mask, rng, out_digits);
    }
#endif
    int count = 0;
    while (mask != 0ULL) {
        const int bit = std::countr_zero(mask);
        out_digits[count++] = bit + 1;
        mask &= (mask - 1ULL);
    }
    if (count <= 2) {
        if (count == 2 && (rng() & 1ULL) != 0ULL) {
            std::swap(out_digits[0], out_digits[1]);
        }
        return count;
    }
    for (int i = count - 1; i > 0; --i) {
        const int j = static_cast<int>(rng() % static_cast<uint64_t>(i + 1));
        std::swap(out_digits[i], out_digits[j]);
    }
    return count;
}

struct GenericSolvedKernel {
    enum class Backend : uint8_t {
        Scalar = 0,
        Avx2 = 1,
        Avx512 = 2
    };

    explicit GenericSolvedKernel(Backend backend = Backend::Scalar) : backend_(backend) {}

    static Backend backend_from_string(const std::string& backend) {
        if (backend == "avx2") {
            return Backend::Avx2;
        }
        if (backend == "avx512") {
            return Backend::Avx512;
        }
        return Backend::Scalar;
    }

    struct CandidateCache {
        int prepared_nn = 0;
        std::vector<uint64_t> candidates;
        std::vector<uint8_t> candidate_popcnt;
        std::vector<uint64_t> singleton_words;
        std::vector<int> undo_idx;
        std::vector<uint64_t> undo_old;
        std::vector<uint8_t> undo_old_count;

        void ensure(const GenericTopology& topo) {
            if (prepared_nn != topo.nn) {
                candidates.resize(static_cast<size_t>(topo.nn));
                candidate_popcnt.resize(static_cast<size_t>(topo.nn));
                singleton_words.resize((static_cast<size_t>(topo.nn) + 63ULL) >> 6U);
                const size_t per_depth_hint = static_cast<size_t>(std::max(8, std::min(3 * topo.n, 64)));
                const size_t reserve_hint = static_cast<size_t>(topo.nn) * per_depth_hint;
                if (undo_idx.capacity() < reserve_hint) {
                    undo_idx.reserve(reserve_hint);
                    undo_old.reserve(reserve_hint);
                    undo_old_count.reserve(reserve_hint);
                }
                prepared_nn = topo.nn;
            }
            undo_idx.clear();
            undo_old.clear();
            undo_old_count.clear();
        }
    };

    static CandidateCache& candidate_cache_for(const GenericTopology& topo) {
        static thread_local CandidateCache cache;
        cache.ensure(topo);
        return cache;
    }

    static SUDOKU_HOT_INLINE void cache_set_candidate(
        CandidateCache& cache,
        int idx,
        uint64_t mask,
        uint8_t count) {
        const size_t uidx = static_cast<size_t>(idx);
        cache.candidates[uidx] = mask;
        cache.candidate_popcnt[uidx] = count;
        const size_t w = uidx >> 6U;
        const uint64_t bit = 1ULL << (uidx & 63ULL);
        uint64_t& word = cache.singleton_words[w];
        if (count == 1U) {
            word |= bit;
        } else {
            word &= ~bit;
        }
    }

    static int cell_pressure(const GenericBoard& board, int idx) {
        const uint32_t rcb = board.topo->cell_rcb_packed[static_cast<size_t>(idx)];
        const int r = GenericBoard::packed_row(rcb);
        const int c = GenericBoard::packed_col(rcb);
        const int b = GenericBoard::packed_box(rcb);
        const uint64_t used =
            board.row_used[static_cast<size_t>(r)] |
            board.col_used[static_cast<size_t>(c)] |
            board.box_used[static_cast<size_t>(b)];
        return static_cast<int>(std::popcount(used));
    }

    struct MvrState {
        int best_bucket = 65;
        int best_idx = -1;
        uint64_t best_mask = 0ULL;
        int best_pressure = -1;
        bool found_empty = false;
    };

    static bool consider_empty_cell_mrv_precount(
        int idx,
        uint64_t candidate_mask,
        int candidate_cnt,
        int used_pressure,
        MvrState& state) {
        if (candidate_cnt == 0) {
            return false;
        }
        state.found_empty = true;
        if (candidate_cnt < state.best_bucket) {
            state.best_bucket = candidate_cnt;
            state.best_idx = idx;
            state.best_mask = candidate_mask;
            state.best_pressure = (candidate_cnt == 1) ? 64 : used_pressure;
        } else if (candidate_cnt == state.best_bucket && candidate_cnt > 1) {
            if (used_pressure > state.best_pressure) {
                state.best_idx = idx;
                state.best_mask = candidate_mask;
                state.best_pressure = used_pressure;
            }
        }
        return true;
    }

    static bool consider_empty_cell_mrv_fast(
        int idx,
        uint64_t candidate_mask,
        uint64_t used_mask,
        MvrState& state) {
        const int cnt = static_cast<int>(std::popcount(candidate_mask));
        const int pressure = static_cast<int>(std::popcount(used_mask));
        return consider_empty_cell_mrv_precount(idx, candidate_mask, cnt, pressure, state);
    }

    static bool finalize_mrv_pick(
        int& best_idx,
        uint64_t& best_mask,
        const MvrState& state) {
        if (!state.found_empty) {
            best_idx = -1;
            best_mask = 0ULL;
            return true;
        }
        best_idx = state.best_idx;
        best_mask = state.best_mask;
        return true;
    }

    static void popcount4x64_scalar(const uint64_t* in_masks, int* out_counts) {
        out_counts[0] = static_cast<int>(std::popcount(in_masks[0]));
        out_counts[1] = static_cast<int>(std::popcount(in_masks[1]));
        out_counts[2] = static_cast<int>(std::popcount(in_masks[2]));
        out_counts[3] = static_cast<int>(std::popcount(in_masks[3]));
    }

#if defined(__x86_64__) || defined(__i386__)
    SUDOKU_TARGET_AVX2 static void popcount4x64_avx2(const uint64_t* in_masks, int* out_counts) {
        const __m256i x = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in_masks));
        const __m256i low_nibble_mask = _mm256_set1_epi8(0x0F);
        const __m256i lut = _mm256_setr_epi8(
            0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
            0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);

        const __m256i lo = _mm256_and_si256(x, low_nibble_mask);
        const __m256i hi = _mm256_and_si256(_mm256_srli_epi16(x, 4), low_nibble_mask);
        const __m256i pc = _mm256_add_epi8(_mm256_shuffle_epi8(lut, lo), _mm256_shuffle_epi8(lut, hi));
        const __m256i sad = _mm256_sad_epu8(pc, _mm256_setzero_si256());
        alignas(32) uint64_t sums[4];
        _mm256_store_si256(reinterpret_cast<__m256i*>(sums), sad);
        out_counts[0] = static_cast<int>(sums[0]);
        out_counts[1] = static_cast<int>(sums[1]);
        out_counts[2] = static_cast<int>(sums[2]);
        out_counts[3] = static_cast<int>(sums[3]);
    }

    SUDOKU_TARGET_AVX512VPOPCNT static void popcount8x64_avx512vpopcnt(const uint64_t* in_masks, int* out_counts) {
        const __m512i x = _mm512_loadu_si512(reinterpret_cast<const void*>(in_masks));
        const __m512i c = _mm512_popcnt_epi64(x);
        alignas(64) uint64_t lanes[8];
        _mm512_store_si512(reinterpret_cast<__m512i*>(lanes), c);
        for (int i = 0; i < 8; ++i) {
            out_counts[i] = static_cast<int>(lanes[i]);
        }
    }
#endif

    static bool select_best_cell_bucketed_scalar(
        const GenericBoard& board,
        int& best_idx,
        uint64_t& best_mask) {
        MvrState state{};
        const auto* topo = board.topo;
        const auto* packed_ptr = topo->cell_rcb_packed.data();
        const auto* row_used = board.row_used.data();
        const auto* col_used = board.col_used.data();
        const auto* box_used = board.box_used.data();
        const uint64_t full_mask = board.full_mask;

        for (int idx = 0; idx < topo->nn; ++idx) {
            if (board.values[static_cast<size_t>(idx)] != 0) {
                continue;
            }
            const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
            const int r = GenericBoard::packed_row(rcb);
            const int c = GenericBoard::packed_col(rcb);
            const int b = GenericBoard::packed_box(rcb);
            const uint64_t used = row_used[static_cast<size_t>(r)] |
                                  col_used[static_cast<size_t>(c)] |
                                  box_used[static_cast<size_t>(b)];
            const uint64_t candidate_mask = (~used) & full_mask;
            const int cnt = static_cast<int>(std::popcount(candidate_mask));
            const int pressure = static_cast<int>(std::popcount(used));
            if (!consider_empty_cell_mrv_precount(idx, candidate_mask, cnt, pressure, state)) {
                return false;
            }
        }
        return finalize_mrv_pick(best_idx, best_mask, state);
    }

#if defined(__x86_64__) || defined(__i386__)
    SUDOKU_TARGET_AVX2 static bool select_best_cell_bucketed_avx2(
        const GenericBoard& board,
        int& best_idx,
        uint64_t& best_mask) {
        MvrState state{};
        const auto* topo = board.topo;
        const auto* packed_ptr = topo->cell_rcb_packed.data();
        const auto* row_used = board.row_used.data();
        const auto* col_used = board.col_used.data();
        const auto* box_used = board.box_used.data();
        const uint64_t full_mask = board.full_mask;

        constexpr int kLanes = 16;  // 16x uint16_t in 256-bit register.
        const int nn = topo->nn;
        const auto* values_ptr = board.values.data();
        const __m256i zero = _mm256_setzero_si256();
        int batch_idx[4];
        uint64_t batch_cand[4];
        uint64_t batch_used[4];
        int batch_size = 0;
        auto flush_batch = [&](int count) -> bool {
            if (count <= 0) {
                return true;
            }
            int cand_cnt[4]{};
            int used_cnt[4]{};
            if (count == 4) {
                popcount4x64_avx2(batch_cand, cand_cnt);
                popcount4x64_avx2(batch_used, used_cnt);
            } else {
                for (int i = 0; i < count; ++i) {
                    cand_cnt[i] = static_cast<int>(std::popcount(batch_cand[i]));
                    used_cnt[i] = static_cast<int>(std::popcount(batch_used[i]));
                }
            }
            for (int i = 0; i < count; ++i) {
                if (!consider_empty_cell_mrv_precount(batch_idx[i], batch_cand[i], cand_cnt[i], used_cnt[i], state)) {
                    return false;
                }
            }
            return true;
        };

        int idx = 0;
        for (; idx + kLanes <= nn; idx += kLanes) {
            const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(values_ptr + idx));
            const __m256i eq_zero = _mm256_cmpeq_epi16(v, zero);
            const uint32_t bytes_mask = static_cast<uint32_t>(_mm256_movemask_epi8(eq_zero));
            uint32_t pair_mask = bytes_mask & (bytes_mask >> 1U) & 0x55555555U;
            while (pair_mask != 0U) {
                const uint32_t bit = static_cast<uint32_t>(std::countr_zero(pair_mask));
                const int lane = static_cast<int>(bit >> 1U);
                const int cell_idx = idx + lane;
                const uint32_t rcb = packed_ptr[static_cast<size_t>(cell_idx)];
                const int r = GenericBoard::packed_row(rcb);
                const int c = GenericBoard::packed_col(rcb);
                const int b = GenericBoard::packed_box(rcb);
                const uint64_t used = row_used[static_cast<size_t>(r)] |
                                      col_used[static_cast<size_t>(c)] |
                                      box_used[static_cast<size_t>(b)];
                batch_idx[batch_size] = cell_idx;
                batch_used[batch_size] = used;
                batch_cand[batch_size] = (~used) & full_mask;
                ++batch_size;
                if (batch_size == 4) {
                    if (!flush_batch(batch_size)) {
                        return false;
                    }
                    batch_size = 0;
                }
                pair_mask &= (pair_mask - 1U);
            }
        }
        for (; idx < nn; ++idx) {
            if (values_ptr[idx] != 0) {
                continue;
            }
            const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
            const int r = GenericBoard::packed_row(rcb);
            const int c = GenericBoard::packed_col(rcb);
            const int b = GenericBoard::packed_box(rcb);
            const uint64_t used = row_used[static_cast<size_t>(r)] |
                                  col_used[static_cast<size_t>(c)] |
                                  box_used[static_cast<size_t>(b)];
            batch_idx[batch_size] = idx;
            batch_used[batch_size] = used;
            batch_cand[batch_size] = (~used) & full_mask;
            ++batch_size;
            if (batch_size == 4) {
                if (!flush_batch(batch_size)) {
                    return false;
                }
                batch_size = 0;
            }
        }
        if (!flush_batch(batch_size)) {
            return false;
        }
        return finalize_mrv_pick(best_idx, best_mask, state);
    }

    SUDOKU_TARGET_AVX512BW static bool select_best_cell_bucketed_avx512(
        const GenericBoard& board,
        int& best_idx,
        uint64_t& best_mask) {
        MvrState state{};
        const auto* topo = board.topo;
        const auto* packed_ptr = topo->cell_rcb_packed.data();
        const auto* row_used = board.row_used.data();
        const auto* col_used = board.col_used.data();
        const auto* box_used = board.box_used.data();
        const uint64_t full_mask = board.full_mask;

        constexpr int kLanes = 32;  // 32x uint16_t in 512-bit register.
        const int nn = topo->nn;
        const auto* values_ptr = board.values.data();
        const __m512i zero = _mm512_setzero_si512();
        static const bool kHasVpopcntDq =
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
            __builtin_cpu_supports("avx512vpopcntdq");
#else
            false;
#endif

        int batch_idx[8];
        uint64_t batch_cand[8];
        uint64_t batch_used[8];
        int batch_size = 0;
        auto flush_batch = [&](int count) -> bool {
            if (count <= 0) {
                return true;
            }
            int cand_cnt[8]{};
            int used_cnt[8]{};
            if (count == 8 && kHasVpopcntDq) {
                popcount8x64_avx512vpopcnt(batch_cand, cand_cnt);
                popcount8x64_avx512vpopcnt(batch_used, used_cnt);
            } else {
                for (int i = 0; i < count; ++i) {
                    cand_cnt[i] = static_cast<int>(std::popcount(batch_cand[i]));
                    used_cnt[i] = static_cast<int>(std::popcount(batch_used[i]));
                }
            }
            for (int i = 0; i < count; ++i) {
                if (!consider_empty_cell_mrv_precount(batch_idx[i], batch_cand[i], cand_cnt[i], used_cnt[i], state)) {
                    return false;
                }
            }
            return true;
        };

        int idx = 0;
        for (; idx + kLanes <= nn; idx += kLanes) {
            const __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(values_ptr + idx));
            uint32_t empty_mask = static_cast<uint32_t>(_mm512_cmpeq_epi16_mask(v, zero));
            while (empty_mask != 0U) {
                const uint32_t lane = static_cast<uint32_t>(std::countr_zero(empty_mask));
                const int cell_idx = idx + static_cast<int>(lane);
                const uint32_t rcb = packed_ptr[static_cast<size_t>(cell_idx)];
                const int r = GenericBoard::packed_row(rcb);
                const int c = GenericBoard::packed_col(rcb);
                const int b = GenericBoard::packed_box(rcb);
                const uint64_t used = row_used[static_cast<size_t>(r)] |
                                      col_used[static_cast<size_t>(c)] |
                                      box_used[static_cast<size_t>(b)];
                batch_idx[batch_size] = cell_idx;
                batch_used[batch_size] = used;
                batch_cand[batch_size] = (~used) & full_mask;
                ++batch_size;
                if (batch_size == 8) {
                    if (!flush_batch(batch_size)) {
                        return false;
                    }
                    batch_size = 0;
                }
                empty_mask &= (empty_mask - 1U);
            }
        }
        for (; idx < nn; ++idx) {
            if (values_ptr[idx] != 0) {
                continue;
            }
            const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
            const int r = GenericBoard::packed_row(rcb);
            const int c = GenericBoard::packed_col(rcb);
            const int b = GenericBoard::packed_box(rcb);
            const uint64_t used = row_used[static_cast<size_t>(r)] |
                                  col_used[static_cast<size_t>(c)] |
                                  box_used[static_cast<size_t>(b)];
            batch_idx[batch_size] = idx;
            batch_used[batch_size] = used;
            batch_cand[batch_size] = (~used) & full_mask;
            ++batch_size;
            if (batch_size == 8) {
                if (!flush_batch(batch_size)) {
                    return false;
                }
                batch_size = 0;
            }
        }
        if (!flush_batch(batch_size)) {
            return false;
        }
        return finalize_mrv_pick(best_idx, best_mask, state);
    }
#endif

    // MRV with bucketed candidate counts and pressure tie-break for asymmetric topologies.
    static bool select_best_cell_bucketed(
        const GenericBoard& board,
        int& best_idx,
        uint64_t& best_mask,
        Backend backend = Backend::Scalar) {
        switch (backend) {
        case Backend::Avx512:
#if defined(__x86_64__) || defined(__i386__)
            return select_best_cell_bucketed_avx512(board, best_idx, best_mask);
#else
            return select_best_cell_bucketed_scalar(board, best_idx, best_mask);
#endif
        case Backend::Avx2:
#if defined(__x86_64__) || defined(__i386__)
            return select_best_cell_bucketed_avx2(board, best_idx, best_mask);
#else
            return select_best_cell_bucketed_scalar(board, best_idx, best_mask);
#endif
        case Backend::Scalar:
        default:
            return select_best_cell_bucketed_scalar(board, best_idx, best_mask);
        }
    }

    static bool init_candidate_cache(const GenericBoard& board, CandidateCache& cache) {
        const auto* topo = board.topo;
        const int nn = topo->nn;
        const auto* packed_ptr = topo->cell_rcb_packed.data();
        const auto* row_used = board.row_used.data();
        const auto* col_used = board.col_used.data();
        const auto* box_used = board.box_used.data();
        const auto* values_ptr = board.values.data();
        const uint64_t full_mask = board.full_mask;
        std::fill(cache.singleton_words.begin(), cache.singleton_words.end(), 0ULL);
        for (int idx = 0; idx < nn; ++idx) {
            if (values_ptr[static_cast<size_t>(idx)] != 0) {
                cache_set_candidate(cache, idx, 0ULL, 0U);
                continue;
            }
            const uint32_t rcb = packed_ptr[static_cast<size_t>(idx)];
            const int r = GenericBoard::packed_row(rcb);
            const int c = GenericBoard::packed_col(rcb);
            const int b = GenericBoard::packed_box(rcb);
            const uint64_t used = row_used[static_cast<size_t>(r)] |
                                  col_used[static_cast<size_t>(c)] |
                                  box_used[static_cast<size_t>(b)];
            const uint64_t mask = (~used) & full_mask;
            if (mask == 0ULL) {
                return false;
            }
            const uint8_t cnt = static_cast<uint8_t>(std::popcount(mask));
            cache_set_candidate(cache, idx, mask, cnt);
        }
        cache.undo_idx.clear();
        cache.undo_old.clear();
        cache.undo_old_count.clear();
        return true;
    }

    static bool select_best_cell_cached(
        const GenericBoard& board,
        const CandidateCache& cache,
        int& best_idx,
        uint64_t& best_mask) {
        MvrState state{};
        const int nn = board.topo->nn;
        const int n = board.topo->n;
        const uint64_t* const cand_ptr = cache.candidates.data();
        const uint8_t* const cnt_ptr = cache.candidate_popcnt.data();
        const uint64_t* const singles_ptr = cache.singleton_words.data();
        const int single_words = static_cast<int>(cache.singleton_words.size());
        for (int wi = 0; wi < single_words; ++wi) {
            uint64_t sw = singles_ptr[static_cast<size_t>(wi)];
            while (sw != 0ULL) {
                const int bit = static_cast<int>(std::countr_zero(sw));
                const int idx = (wi << 6) + bit;
                if (idx >= nn) {
                    break;
                }
                const uint64_t candidate_mask = cand_ptr[static_cast<size_t>(idx)];
                if (candidate_mask != 0ULL) {
                    best_idx = idx;
                    best_mask = candidate_mask;
                    return true;
                }
                sw &= (sw - 1ULL);
            }
        }
        for (int idx = 0; idx < nn; ++idx) {
            const int cnt = static_cast<int>(cnt_ptr[static_cast<size_t>(idx)]);
            if (cnt == 0) {
                continue;
            }
            if (cnt == 1) {
                const uint64_t candidate_mask = cand_ptr[static_cast<size_t>(idx)];
                best_idx = idx;
                best_mask = candidate_mask;
                return true;
            }
            const uint64_t candidate_mask = cand_ptr[static_cast<size_t>(idx)];
            const int pressure = n - cnt;
            if (!consider_empty_cell_mrv_precount(idx, candidate_mask, cnt, pressure, state)) {
                return false;
            }
        }
        return finalize_mrv_pick(best_idx, best_mask, state);
    }

    static bool try_place_with_cache(
        GenericBoard& board,
        CandidateCache& cache,
        int idx,
        int digit,
        size_t& out_marker) {
        const auto* topo = board.topo;
        const uint64_t placed_bit = 1ULL << (digit - 1);

        out_marker = cache.undo_idx.size();
        const size_t idx_u = static_cast<size_t>(idx);
        cache.undo_idx.push_back(idx);
        cache.undo_old.push_back(cache.candidates[idx_u]);
        cache.undo_old_count.push_back(cache.candidate_popcnt[idx_u]);
        cache_set_candidate(cache, idx, 0ULL, 0U);

        board.place(idx, digit);
        const int off0 = topo->peer_offsets[static_cast<size_t>(idx)];
        const int off1 = topo->peer_offsets[static_cast<size_t>(idx + 1)];
        for (int p = off0; p < off1; ++p) {
            const int peer_idx = topo->peers_flat[static_cast<size_t>(p)];
            const size_t peer_u = static_cast<size_t>(peer_idx);
            const uint64_t old_mask = cache.candidates[peer_u];
            if (old_mask == 0ULL) {
                continue;
            }
            const uint64_t new_mask = old_mask & ~placed_bit;
            if (new_mask == old_mask) {
                continue;
            }
            cache.undo_idx.push_back(peer_idx);
            cache.undo_old.push_back(old_mask);
            cache.undo_old_count.push_back(cache.candidate_popcnt[peer_u]);
            if (new_mask == 0ULL) {
                cache_set_candidate(cache, peer_idx, 0ULL, 0U);
                return false;
            }
            const uint8_t new_cnt = static_cast<uint8_t>(std::popcount(new_mask));
            cache_set_candidate(cache, peer_idx, new_mask, new_cnt);
        }
        return true;
    }

    static void rollback_place_with_cache(
        GenericBoard& board,
        CandidateCache& cache,
        int idx,
        int digit,
        size_t marker) {
        while (cache.undo_idx.size() > marker) {
            const int changed_idx = cache.undo_idx.back();
            const uint64_t old_mask = cache.undo_old.back();
            const uint8_t old_count = cache.undo_old_count.back();
            cache.undo_idx.pop_back();
            cache.undo_old.pop_back();
            cache.undo_old_count.pop_back();
            cache_set_candidate(cache, changed_idx, old_mask, old_count);
        }
        board.unplace(idx, digit);
    }

    bool fill_cached(GenericBoard& board, std::mt19937_64& rng, SearchAbortControl* budget, CandidateCache& cache) const {
        const bool has_budget = (budget != nullptr);
        if (has_budget && !budget->step()) {
            return false;
        }
        int best_idx = -1;
        uint64_t best_mask = 0ULL;
        if (!select_best_cell_cached(board, cache, best_idx, best_mask)) {
            return false;
        }
        if (best_idx == -1) {
            return true;
        }

        int digits[64];
        const int digit_count = shuffled_digits_from_mask(best_mask, rng, digits);
        if (digit_count == 1) {
            size_t marker = 0;
            const int d = digits[0];
            const bool ok = try_place_with_cache(board, cache, best_idx, d, marker);
            if (ok && fill_cached(board, rng, budget, cache)) {
                return true;
            }
            rollback_place_with_cache(board, cache, best_idx, d, marker);
            return false;
        }
        for (int i = 0; i < digit_count; ++i) {
            const int d = digits[i];
            size_t marker = 0;
            const bool ok = try_place_with_cache(board, cache, best_idx, d, marker);
            if (ok && fill_cached(board, rng, budget, cache)) {
                return true;
            }
            rollback_place_with_cache(board, cache, best_idx, d, marker);
            if (has_budget && budget->aborted()) {
                return false;
            }
        }
        return false;
    }

    bool fill_recompute(GenericBoard& board, std::mt19937_64& rng, SearchAbortControl* budget) const {
        const bool has_budget = (budget != nullptr);
        if (has_budget && !budget->step()) {
            return false;
        }
        int best_idx = -1;
        uint64_t best_mask = 0ULL;
        if (!select_best_cell_bucketed(board, best_idx, best_mask, backend_)) {
            return false;
        }
        if (best_idx == -1) {
            return true;
        }

        int digits[64];
        const int digit_count = shuffled_digits_from_mask(best_mask, rng, digits);
        if (digit_count == 1) {
            const int d = digits[0];
            board.place(best_idx, d);
            if (fill_recompute(board, rng, budget)) {
                return true;
            }
            board.unplace(best_idx, d);
            return false;
        }
        for (int i = 0; i < digit_count; ++i) {
            const int d = digits[i];
            board.place(best_idx, d);
            if (fill_recompute(board, rng, budget)) {
                return true;
            }
            board.unplace(best_idx, d);
            if (has_budget && budget->aborted()) {
                return false;
            }
        }
        return false;
    }

    bool fill(GenericBoard& board, std::mt19937_64& rng, SearchAbortControl* budget) const {
        // Candidate-cache MRV is most beneficial for larger boards where repeated
        // candidate recomputation dominates. For smaller geometries, recompute path
        // with AVX dispatch is typically faster.
        static constexpr int kCacheMrvMinN = 25;
        if (board.topo->n < kCacheMrvMinN) {
            return fill_recompute(board, rng, budget);
        }
        CandidateCache& cache = candidate_cache_for(*board.topo);
        if (!init_candidate_cache(board, cache)) {
            return false;
        }
        return fill_cached(board, rng, budget, cache);
    }

    bool generate(
        const GenericTopology& topo,
        std::mt19937_64& rng,
        std::vector<uint16_t>& out_solution,
        SearchAbortControl* budget = nullptr) const {
        static thread_local GenericBoard board;
        board.reset(topo);
        if (!fill(board, rng, budget)) {
            return false;
        }
        if (out_solution.size() != static_cast<size_t>(topo.nn)) {
            out_solution.resize(static_cast<size_t>(topo.nn));
        }
        std::copy_n(board.values.data(), static_cast<size_t>(topo.nn), out_solution.data());
        return true;
    }

private:
    Backend backend_ = Backend::Scalar;
};

struct GenericDigResult {
    std::vector<uint16_t> puzzle;
    int clues = 0;
};

struct GenericDigKernel {
    void dig_into(
        const std::vector<uint16_t>& solved,
        const GenericTopology& topo,
        const GenerateRunConfig& cfg,
        std::mt19937_64& rng,
        std::vector<uint16_t>& out_puzzle,
        int& out_clues) const {
        out_puzzle.resize(solved.size());
        std::copy(solved.begin(), solved.end(), out_puzzle.begin());
        int min_clues = std::clamp(cfg.min_clues, 0, topo.nn);
        int max_clues = std::clamp(cfg.max_clues, min_clues, topo.nn);
        std::uniform_int_distribution<int> pick_target(min_clues, max_clues);
        const int target_clues = pick_target(rng);

        GenericThreadScratch& scratch = generic_tls_for(topo);
        std::vector<int>& order = scratch.order;
        for (int i = 0; i < topo.nn; ++i) {
            order[static_cast<size_t>(i)] = i;
        }
        for (int i = topo.nn - 1; i > 0; --i) {
            const int j = static_cast<int>(rng() % static_cast<uint64_t>(i + 1));
            std::swap(order[static_cast<size_t>(i)], order[static_cast<size_t>(j)]);
        }

        int clues = topo.nn;
        for (int i = 0; i < topo.nn; ++i) {
            const int idx = order[static_cast<size_t>(i)];
            if (clues <= target_clues) {
                break;
            }
            if (out_puzzle[static_cast<size_t>(idx)] == 0) {
                continue;
            }
            int sym_idx = -1;
            if (cfg.symmetry_center) {
                sym_idx = topo.cell_center_sym[static_cast<size_t>(idx)];
            }
            if (sym_idx >= 0 && sym_idx != idx && out_puzzle[static_cast<size_t>(sym_idx)] != 0) {
                if (clues - 2 < target_clues) {
                    continue;
                }
                out_puzzle[static_cast<size_t>(idx)] = 0;
                out_puzzle[static_cast<size_t>(sym_idx)] = 0;
                clues -= 2;
            } else {
                if (clues - 1 < target_clues) {
                    continue;
                }
                out_puzzle[static_cast<size_t>(idx)] = 0;
                --clues;
            }
        }
        out_clues = clues;
    }

    GenericDigResult dig(const std::vector<uint16_t>& solved, const GenericTopology& topo, const GenerateRunConfig& cfg, std::mt19937_64& rng) const {
        GenericDigResult result;
        dig_into(solved, topo, cfg, rng, result.puzzle, result.clues);
        return result;
    }
};

struct GenericQuickPrefilter {
    bool check(const std::vector<uint16_t>& puzzle, const GenericTopology& topo, int min_clues, int max_clues) const {
        if (static_cast<int>(puzzle.size()) != topo.nn) {
            return false;
        }
        int clues = 0;
        const auto& rcb = topo.cell_rcb_packed;
        GenericThreadScratch& scratch = generic_tls_for(topo);
        std::fill(scratch.row_tmp.begin(), scratch.row_tmp.end(), 0ULL);
        std::fill(scratch.col_tmp.begin(), scratch.col_tmp.end(), 0ULL);
        std::fill(scratch.box_tmp.begin(), scratch.box_tmp.end(), 0ULL);
        uint64_t* const row_used = scratch.row_tmp.data();
        uint64_t* const col_used = scratch.col_tmp.data();
        uint64_t* const box_used = scratch.box_tmp.data();
        const uint32_t* const packed = rcb.data();
        for (int idx = 0; idx < topo.nn; ++idx) {
            const int d = static_cast<int>(puzzle[static_cast<size_t>(idx)]);
            if (d == 0) {
                const int remaining = topo.nn - idx - 1;
                if (clues + remaining < min_clues) {
                    return false;
                }
                continue;
            }
            if (d < 1 || d > topo.n) {
                return false;
            }
            ++clues;
            if (clues > max_clues) {
                return false;
            }
            const uint32_t p = packed[static_cast<size_t>(idx)];
            const int r = GenericBoard::packed_row(p);
            const int c = GenericBoard::packed_col(p);
            const int b = GenericBoard::packed_box(p);
            const uint64_t bit = (1ULL << (d - 1));
            if ((row_used[static_cast<size_t>(r)] & bit) ||
                (col_used[static_cast<size_t>(c)] & bit) ||
                (box_used[static_cast<size_t>(b)] & bit)) {
                return false;
            }
            row_used[static_cast<size_t>(r)] |= bit;
            col_used[static_cast<size_t>(c)] |= bit;
            box_used[static_cast<size_t>(b)] |= bit;
        }
        return clues >= min_clues;
    }
};

} // namespace sudoku_hpc
