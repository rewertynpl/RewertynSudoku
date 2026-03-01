// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: death_blossom.h (Level 7 - Nightmare)
// Description: Direct stem+petals Death Blossom detector (zero-allocation).
// ============================================================================

#pragma once

#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline int other_digit_in_bivalue(uint64_t bivalue_mask, int known_digit) {
    if (std::popcount(bivalue_mask) != 2) {
        return 0;
    }
    const uint64_t known_bit = 1ULL << (known_digit - 1);
    if ((bivalue_mask & known_bit) == 0ULL) {
        return 0;
    }
    const uint64_t other = bivalue_mask & ~known_bit;
    if (std::popcount(other) != 1) {
        return 0;
    }
    return static_cast<int>(std::countr_zero(other)) + 1;
}

inline ApplyResult apply_death_blossom(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    bool progress = false;

    // petals[digit-1][] keeps peer bivalue cells containing this pivot digit.
    int petals[64][192]{};
    int petal_cnt[64]{};
    int pivot_digits[64]{};

    for (int pivot = 0; pivot < nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;

        const uint64_t pivot_mask = st.cands[pivot];
        const int pivot_pc = std::popcount(pivot_mask);
        if (pivot_pc < 3 || pivot_pc > 6) continue;

        for (int i = 0; i < n; ++i) {
            petal_cnt[i] = 0;
        }

        int pivot_digit_cnt = 0;
        uint64_t w = pivot_mask;
        while (w != 0ULL && pivot_digit_cnt < n) {
            const uint64_t bit = w & (~w + 1ULL);
            w &= (w - 1ULL);
            const int d = static_cast<int>(std::countr_zero(bit)) + 1;
            pivot_digits[pivot_digit_cnt++] = d;
        }

        const int p0 = st.topo->peer_offsets[pivot];
        const int p1 = st.topo->peer_offsets[pivot + 1];
        for (int p = p0; p < p1; ++p) {
            const int peer = st.topo->peers_flat[p];
            if (st.board->values[peer] != 0) continue;
            const uint64_t pm = st.cands[peer];
            if (std::popcount(pm) != 2) continue;

            for (int di = 0; di < pivot_digit_cnt; ++di) {
                const int d = pivot_digits[di];
                const uint64_t bit = 1ULL << (d - 1);
                if ((pm & bit) == 0ULL) continue;
                int& cnt = petal_cnt[d - 1];
                if (cnt < 192) {
                    petals[d - 1][cnt++] = peer;
                }
            }
        }

        for (int ia = 0; ia < pivot_digit_cnt; ++ia) {
            const int da = pivot_digits[ia];
            const int ca = petal_cnt[da - 1];
            if (ca == 0) continue;

            for (int ib = ia + 1; ib < pivot_digit_cnt; ++ib) {
                const int db = pivot_digits[ib];
                const int cb = petal_cnt[db - 1];
                if (cb == 0) continue;

                for (int pa = 0; pa < ca; ++pa) {
                    const int petal_a = petals[da - 1][pa];
                    const int z_a = other_digit_in_bivalue(st.cands[petal_a], da);
                    if (z_a == 0) continue;

                    for (int pb = 0; pb < cb; ++pb) {
                        const int petal_b = petals[db - 1][pb];
                        if (petal_b == petal_a) continue;
                        const int z_b = other_digit_in_bivalue(st.cands[petal_b], db);
                        if (z_b == 0 || z_b != z_a) continue;

                        const uint64_t elim_bit = 1ULL << (z_a - 1);
                        const int q0 = st.topo->peer_offsets[petal_a];
                        const int q1 = st.topo->peer_offsets[petal_a + 1];
                        for (int q = q0; q < q1; ++q) {
                            const int target = st.topo->peers_flat[q];
                            if (target == pivot || target == petal_a || target == petal_b) continue;
                            if (!st.is_peer(target, petal_b)) continue;
                            if (st.board->values[target] != 0) continue;

                            const ApplyResult er = st.eliminate(target, elim_bit);
                            if (er == ApplyResult::Contradiction) {
                                s.elapsed_ns += st.now_ns() - t0;
                                return er;
                            }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }
    }

    if (progress) {
        ++s.hit_count;
        r.used_death_blossom = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare

