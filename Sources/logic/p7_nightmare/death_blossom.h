// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: death_blossom.h (Level 7 - Nightmare)
// Description: Direct stem+petals Death Blossom detector (zero-allocation).
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/als_builder.h"

namespace sudoku_hpc::logic::p7_nightmare {

struct DeathBlossomPetalRef {
    uint16_t kind;
    uint16_t idx;
};

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

inline int death_blossom_collect_als_holders(
    const CandidateState& st,
    const shared::ALS& als,
    uint64_t bit,
    int* out) {
    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    int cnt = 0;
    for (int w = 0; w < words; ++w) {
        uint64_t m = als.cell_mask[w];
        while (m != 0ULL) {
            const uint64_t lsb = config::bit_lsb(m);
            const int b = config::bit_ctz_u64(lsb);
            const int idx = (w << 6) + b;
            if (idx < nn && (st.cands[idx] & bit) != 0ULL) out[cnt++] = idx;
            m = config::bit_clear_lsb_u64(m);
        }
    }
    return cnt;
}

inline bool death_blossom_all_holders_see_pivot(
    const CandidateState& st,
    int pivot,
    const int* holders,
    int count) {
    for (int i = 0; i < count; ++i) {
        if (!st.is_peer(pivot, holders[i])) return false;
    }
    return count > 0;
}

inline bool death_blossom_cell_sees_all(
    const CandidateState& st,
    int cell,
    const int* holders,
    int count) {
    for (int i = 0; i < count; ++i) {
        if (!st.is_peer(cell, holders[i])) return false;
    }
    return count > 0;
}

inline uint64_t death_blossom_petal_digit_mask(
    const CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    const DeathBlossomPetalRef& ref) {
    if (ref.kind == 0) return st.cands[ref.idx];
    return sp.als_list[ref.idx].digit_mask;
}

inline int death_blossom_collect_petal_holders(
    const CandidateState& st,
    const shared::ExactPatternScratchpad& sp,
    const DeathBlossomPetalRef& ref,
    uint64_t bit,
    int* out) {
    if (ref.kind == 0) {
        if ((st.cands[ref.idx] & bit) == 0ULL) return 0;
        out[0] = ref.idx;
        return 1;
    }
    return death_blossom_collect_als_holders(st, sp.als_list[ref.idx], bit, out);
}

inline bool death_blossom_petal_contains_cell(
    const shared::ExactPatternScratchpad& sp,
    const DeathBlossomPetalRef& ref,
    int idx) {
    if (ref.kind == 0) return ref.idx == idx;
    return shared::als_cell_in(sp.als_list[ref.idx], idx);
}

inline bool death_blossom_petals_overlap(
    const shared::ExactPatternScratchpad& sp,
    const DeathBlossomPetalRef& a,
    const DeathBlossomPetalRef& b,
    int words) {
    if (a.kind == 0 && b.kind == 0) return a.idx == b.idx;
    if (a.kind == 0 && b.kind != 0) return shared::als_cell_in(sp.als_list[b.idx], a.idx);
    if (a.kind != 0 && b.kind == 0) return shared::als_cell_in(sp.als_list[a.idx], b.idx);
    return shared::als_overlap(sp.als_list[a.idx], sp.als_list[b.idx], words);
}

inline ApplyResult death_blossom_mixed_petal_pass(
    CandidateState& st,
    int pivot,
    const int* pivot_digits,
    int pivot_digit_cnt,
    int (*bivalue_petals)[192],
    const int* bivalue_counts,
    int als_limit,
    bool& progress) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    DeathBlossomPetalRef petal_refs[6][320]{};
    int petal_ref_cnt[6]{};
    int pa[8]{}, pb[8]{}, pc[8]{};
    int za[8]{}, zb[8]{}, zc[8]{};

    for (int di = 0; di < pivot_digit_cnt; ++di) {
        const int digit = pivot_digits[di];
        const uint64_t bit = 1ULL << (digit - 1);
        int& ref_cnt = petal_ref_cnt[di];

        const int bc = bivalue_counts[digit - 1];
        for (int i = 0; i < bc && ref_cnt < 320; ++i) {
            petal_refs[di][ref_cnt++] = DeathBlossomPetalRef{0, static_cast<uint16_t>(bivalue_petals[digit - 1][i])};
        }

        for (int a = 0; a < als_limit && ref_cnt < 320; ++a) {
            const shared::ALS& als = sp.als_list[a];
            if ((als.digit_mask & bit) == 0ULL) continue;
            const int cnt = death_blossom_collect_als_holders(st, als, bit, pa);
            if (!death_blossom_all_holders_see_pivot(st, pivot, pa, cnt)) continue;
            petal_refs[di][ref_cnt++] = DeathBlossomPetalRef{1, static_cast<uint16_t>(a)};
        }
    }

    for (int ia = 0; ia < pivot_digit_cnt; ++ia) {
        const uint64_t da = 1ULL << (pivot_digits[ia] - 1);
        const int ca = petal_ref_cnt[ia];
        if (ca == 0) continue;

        for (int ib = ia + 1; ib < pivot_digit_cnt; ++ib) {
            const uint64_t db = 1ULL << (pivot_digits[ib] - 1);
            const int cb = petal_ref_cnt[ib];
            if (cb == 0) continue;

            for (int a = 0; a < ca; ++a) {
                const DeathBlossomPetalRef petal_a = petal_refs[ia][a];
                const uint64_t mask_a = death_blossom_petal_digit_mask(st, sp, petal_a);
                for (int b = 0; b < cb; ++b) {
                    const DeathBlossomPetalRef petal_b = petal_refs[ib][b];
                    if (death_blossom_petals_overlap(sp, petal_a, petal_b, words)) continue;
                    const uint64_t mask_b = death_blossom_petal_digit_mask(st, sp, petal_b);

                    uint64_t zmask = (mask_a & mask_b) & ~(da | db);
                    while (zmask != 0ULL) {
                        const uint64_t z = config::bit_lsb(zmask);
                        zmask = config::bit_clear_lsb_u64(zmask);
                        const int za_cnt = death_blossom_collect_petal_holders(st, sp, petal_a, z, za);
                        const int zb_cnt = death_blossom_collect_petal_holders(st, sp, petal_b, z, zb);
                        if (za_cnt <= 0 || zb_cnt <= 0) continue;

                        for (int t = 0; t < nn; ++t) {
                            if (t == pivot || st.board->values[t] != 0) continue;
                            if ((st.cands[t] & z) == 0ULL) continue;
                            if (death_blossom_petal_contains_cell(sp, petal_a, t) ||
                                death_blossom_petal_contains_cell(sp, petal_b, t)) {
                                continue;
                            }
                            if (!death_blossom_cell_sees_all(st, t, za, za_cnt)) continue;
                            if (!death_blossom_cell_sees_all(st, t, zb, zb_cnt)) continue;

                            const ApplyResult er = st.eliminate(t, z);
                            if (er == ApplyResult::Contradiction) return er;
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }

            for (int ic = ib + 1; ic < pivot_digit_cnt; ++ic) {
                const uint64_t dc = 1ULL << (pivot_digits[ic] - 1);
                const int cc = petal_ref_cnt[ic];
                if (cc == 0) continue;

                for (int a = 0; a < ca; ++a) {
                    const DeathBlossomPetalRef petal_a = petal_refs[ia][a];
                    const uint64_t mask_a = death_blossom_petal_digit_mask(st, sp, petal_a);
                    for (int b = 0; b < cb; ++b) {
                        const DeathBlossomPetalRef petal_b = petal_refs[ib][b];
                        if (death_blossom_petals_overlap(sp, petal_a, petal_b, words)) continue;
                        const uint64_t mask_b = death_blossom_petal_digit_mask(st, sp, petal_b);

                        for (int c = 0; c < cc; ++c) {
                            const DeathBlossomPetalRef petal_c = petal_refs[ic][c];
                            if (death_blossom_petals_overlap(sp, petal_a, petal_c, words) ||
                                death_blossom_petals_overlap(sp, petal_b, petal_c, words)) {
                                continue;
                            }
                            const uint64_t mask_c = death_blossom_petal_digit_mask(st, sp, petal_c);

                            uint64_t zmask = (mask_a & mask_b & mask_c) & ~(da | db | dc);
                            while (zmask != 0ULL) {
                                const uint64_t z = config::bit_lsb(zmask);
                                zmask = config::bit_clear_lsb_u64(zmask);
                                const int za_cnt = death_blossom_collect_petal_holders(st, sp, petal_a, z, za);
                                const int zb_cnt = death_blossom_collect_petal_holders(st, sp, petal_b, z, zb);
                                const int zc_cnt = death_blossom_collect_petal_holders(st, sp, petal_c, z, zc);
                                if (za_cnt <= 0 || zb_cnt <= 0 || zc_cnt <= 0) continue;

                                for (int t = 0; t < nn; ++t) {
                                    if (t == pivot || st.board->values[t] != 0) continue;
                                    if ((st.cands[t] & z) == 0ULL) continue;
                                    if (death_blossom_petal_contains_cell(sp, petal_a, t) ||
                                        death_blossom_petal_contains_cell(sp, petal_b, t) ||
                                        death_blossom_petal_contains_cell(sp, petal_c, t)) {
                                        continue;
                                    }
                                    if (!death_blossom_cell_sees_all(st, t, za, za_cnt)) continue;
                                    if (!death_blossom_cell_sees_all(st, t, zb, zb_cnt)) continue;
                                    if (!death_blossom_cell_sees_all(st, t, zc, zc_cnt)) continue;

                                    const ApplyResult er = st.eliminate(t, z);
                                    if (er == ApplyResult::Contradiction) return er;
                                    progress = progress || (er == ApplyResult::Progress);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult apply_death_blossom(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int n = st.topo->n;
    const int nn = st.topo->nn;
    if (n > 49 || st.board->empty_cells > (nn - std::max((5 * n) / 4, n))) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    bool progress = false;

    // petals[digit-1][] keeps peer bivalue cells containing this pivot digit.
    int petals[64][192]{};
    int petal_cnt[64]{};
    int pivot_digits[64]{};

    for (int pivot = 0; pivot < nn; ++pivot) {
        if (st.board->values[pivot] != 0) continue;

        const uint64_t pivot_mask = st.cands[pivot];
        const int pivot_pc = std::popcount(pivot_mask);
        if (pivot_pc < 3 || pivot_pc > 7) continue;

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

        for (int ia = 0; ia < pivot_digit_cnt; ++ia) {
            const int da = pivot_digits[ia];
            const int ca = petal_cnt[da - 1];
            if (ca == 0) continue;

            for (int ib = ia + 1; ib < pivot_digit_cnt; ++ib) {
                const int db = pivot_digits[ib];
                const int cb = petal_cnt[db - 1];
                if (cb == 0) continue;

                for (int ic = ib + 1; ic < pivot_digit_cnt; ++ic) {
                    const int dc = pivot_digits[ic];
                    const int cc = petal_cnt[dc - 1];
                    if (cc == 0) continue;

                    for (int pa = 0; pa < ca; ++pa) {
                        const int petal_a = petals[da - 1][pa];
                        const int z_a = other_digit_in_bivalue(st.cands[petal_a], da);
                        if (z_a == 0) continue;

                        for (int pb = 0; pb < cb; ++pb) {
                            const int petal_b = petals[db - 1][pb];
                            if (petal_b == petal_a) continue;
                            const int z_b = other_digit_in_bivalue(st.cands[petal_b], db);
                            if (z_b == 0 || z_b != z_a) continue;

                            for (int pc = 0; pc < cc; ++pc) {
                                const int petal_c = petals[dc - 1][pc];
                                if (petal_c == petal_a || petal_c == petal_b) continue;
                                const int z_c = other_digit_in_bivalue(st.cands[petal_c], dc);
                                if (z_c == 0 || z_c != z_a) continue;

                                const uint64_t elim_bit = 1ULL << (z_a - 1);
                                for (int target = 0; target < nn; ++target) {
                                    if (target == pivot || target == petal_a || target == petal_b || target == petal_c) continue;
                                    if (st.board->values[target] != 0) continue;
                                    if ((st.cands[target] & elim_bit) == 0ULL) continue;
                                    if (!st.is_peer(target, petal_a) ||
                                        !st.is_peer(target, petal_b) ||
                                        !st.is_peer(target, petal_c)) {
                                        continue;
                                    }

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
        }
    }

    if (!progress) {
        auto& sp = shared::exact_pattern_scratchpad();
        const int als_max_size = (n <= 16) ? 5 : ((n <= 25) ? 6 : 5);
        const int als_min_size = (n <= 25) ? 1 : 2;
        const int als_cnt = shared::build_als_list(st, als_min_size, als_max_size);
        const int limit = std::min(als_cnt, std::clamp(128 + 3 * n, 128, 224));
        int pivot_digits[6]{};
        int pa[8]{}, pb[8]{}, za[8]{}, zb[8]{};

        for (int pivot = 0; pivot < nn; ++pivot) {
            if (st.board->values[pivot] != 0) continue;
            const uint64_t pivot_mask = st.cands[pivot];
            const int pivot_pc = std::popcount(pivot_mask);
            if (pivot_pc < 3 || pivot_pc > 7) continue;

            int pivot_digit_cnt = 0;
            uint64_t w = pivot_mask;
            while (w != 0ULL && pivot_digit_cnt < 6) {
                const uint64_t bit = config::bit_lsb(w);
                w = config::bit_clear_lsb_u64(w);
                pivot_digits[pivot_digit_cnt++] = config::bit_ctz_u64(bit) + 1;
            }

            int mixed_bivalue_petals[64][192]{};
            int mixed_bivalue_cnt[64]{};
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
                    int& cnt = mixed_bivalue_cnt[d - 1];
                    if (cnt < 192) mixed_bivalue_petals[d - 1][cnt++] = peer;
                }
            }

            const ApplyResult mixed_er = death_blossom_mixed_petal_pass(
                st, pivot, pivot_digits, pivot_digit_cnt, mixed_bivalue_petals, mixed_bivalue_cnt, limit, progress);
            if (mixed_er == ApplyResult::Contradiction) {
                s.elapsed_ns += st.now_ns() - t0;
                return mixed_er;
            }
            if (progress) break;

            for (int ia = 0; ia < pivot_digit_cnt; ++ia) {
                const uint64_t da = 1ULL << (pivot_digits[ia] - 1);
                for (int ib = ia + 1; ib < pivot_digit_cnt; ++ib) {
                    const uint64_t db = 1ULL << (pivot_digits[ib] - 1);
                    for (int a = 0; a < limit; ++a) {
                        const shared::ALS& als_a = sp.als_list[a];
                        if ((als_a.digit_mask & da) == 0ULL) continue;
                        const int pa_cnt = death_blossom_collect_als_holders(st, als_a, da, pa);
                        if (!death_blossom_all_holders_see_pivot(st, pivot, pa, pa_cnt)) continue;

                        for (int b = a + 1; b < limit; ++b) {
                            const shared::ALS& als_b = sp.als_list[b];
                            if ((als_b.digit_mask & db) == 0ULL) continue;
                            const int pb_cnt = death_blossom_collect_als_holders(st, als_b, db, pb);
                            if (!death_blossom_all_holders_see_pivot(st, pivot, pb, pb_cnt)) continue;

                            uint64_t zmask = (als_a.digit_mask & als_b.digit_mask) & ~(da | db);
                            while (zmask != 0ULL) {
                                const uint64_t z = config::bit_lsb(zmask);
                                zmask = config::bit_clear_lsb_u64(zmask);
                                const int za_cnt = death_blossom_collect_als_holders(st, als_a, z, za);
                                const int zb_cnt = death_blossom_collect_als_holders(st, als_b, z, zb);
                                if (za_cnt <= 0 || zb_cnt <= 0) continue;

                                for (int t = 0; t < nn; ++t) {
                                    if (t == pivot || st.board->values[t] != 0) continue;
                                    if (shared::als_cell_in(als_a, t) || shared::als_cell_in(als_b, t)) continue;
                                    if ((st.cands[t] & z) == 0ULL) continue;

                                    bool sees_all = true;
                                    for (int i = 0; i < za_cnt; ++i) {
                                        if (!st.is_peer(t, za[i])) {
                                            sees_all = false;
                                            break;
                                        }
                                    }
                                    if (!sees_all) continue;
                                    for (int i = 0; i < zb_cnt; ++i) {
                                        if (!st.is_peer(t, zb[i])) {
                                            sees_all = false;
                                            break;
                                        }
                                    }
                                    if (!sees_all) continue;

                                    const ApplyResult er = st.eliminate(t, z);
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

            for (int ia = 0; ia < pivot_digit_cnt; ++ia) {
                const uint64_t da = 1ULL << (pivot_digits[ia] - 1);
                for (int ib = ia + 1; ib < pivot_digit_cnt; ++ib) {
                    const uint64_t db = 1ULL << (pivot_digits[ib] - 1);
                    for (int ic = ib + 1; ic < pivot_digit_cnt; ++ic) {
                        const uint64_t dc = 1ULL << (pivot_digits[ic] - 1);
                        for (int a = 0; a < limit; ++a) {
                            const shared::ALS& als_a = sp.als_list[a];
                            if ((als_a.digit_mask & da) == 0ULL) continue;
                            const int pa_cnt = death_blossom_collect_als_holders(st, als_a, da, pa);
                            if (!death_blossom_all_holders_see_pivot(st, pivot, pa, pa_cnt)) continue;

                            for (int b = a + 1; b < limit; ++b) {
                                const shared::ALS& als_b = sp.als_list[b];
                                if ((als_b.digit_mask & db) == 0ULL) continue;
                                const int pb_cnt = death_blossom_collect_als_holders(st, als_b, db, pb);
                                if (!death_blossom_all_holders_see_pivot(st, pivot, pb, pb_cnt)) continue;

                                for (int c = b + 1; c < limit; ++c) {
                                    const shared::ALS& als_c = sp.als_list[c];
                                    if ((als_c.digit_mask & dc) == 0ULL) continue;
                                    const int pc_cnt = death_blossom_collect_als_holders(st, als_c, dc, zb);
                                    if (!death_blossom_all_holders_see_pivot(st, pivot, zb, pc_cnt)) continue;

                                    uint64_t zmask = (als_a.digit_mask & als_b.digit_mask & als_c.digit_mask) & ~(da | db | dc);
                                    while (zmask != 0ULL) {
                                        const uint64_t z = config::bit_lsb(zmask);
                                        zmask = config::bit_clear_lsb_u64(zmask);
                                        const int za_cnt = death_blossom_collect_als_holders(st, als_a, z, za);
                                        const int zb_cnt = death_blossom_collect_als_holders(st, als_b, z, pb);
                                        const int zc_cnt = death_blossom_collect_als_holders(st, als_c, z, pa);
                                        if (za_cnt <= 0 || zb_cnt <= 0 || zc_cnt <= 0) continue;

                                        for (int t = 0; t < nn; ++t) {
                                            if (t == pivot || st.board->values[t] != 0) continue;
                                            if (shared::als_cell_in(als_a, t) || shared::als_cell_in(als_b, t) || shared::als_cell_in(als_c, t)) continue;
                                            if ((st.cands[t] & z) == 0ULL) continue;
                                            if (!death_blossom_cell_sees_all(st, t, za, za_cnt)) continue;
                                            if (!death_blossom_cell_sees_all(st, t, pb, zb_cnt)) continue;
                                            if (!death_blossom_cell_sees_all(st, t, pa, zc_cnt)) continue;

                                            const ApplyResult er = st.eliminate(t, z);
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
