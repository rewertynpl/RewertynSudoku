// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Module: als_xy_wing_chain.h (Level 7 - Nightmare)
// Description: Direct zero-allocation passes for ALS-XY-Wing, ALS-Chain and
// ALS-AIC style eliminations, with conservative fallback composition.
// ============================================================================

#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/als_builder.h"
#include "../shared/exact_pattern_scratchpad.h"

#include "../p6_diabolical/als_xz.h"
#include "../p5_expert/xyz_w_wing.h"
#include "../p6_diabolical/chains_basic.h"

namespace sudoku_hpc::logic::p7_nightmare {

inline bool als_deep_pass_allowed(const CandidateState& st) {
    if (st.topo->n > 32) return false;
    return st.board->empty_cells <= (st.topo->nn - st.topo->n);
}

inline bool als_overlap(const shared::ALS& a, const shared::ALS& b, int words) {
    for (int w = 0; w < words; ++w) {
        if ((a.cell_mask[w] & b.cell_mask[w]) != 0ULL) return true;
    }
    return false;
}

inline int als_collect_holders_for_digit(
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
            if (idx < nn && (st.cands[idx] & bit) != 0ULL) {
                out[cnt++] = idx;
            }
            m = config::bit_clear_lsb_u64(m);
        }
    }
    return cnt;
}

inline bool holders_fully_cross_peer(
    const CandidateState& st,
    const int* left,
    int left_cnt,
    const int* right,
    int right_cnt) {
    if (left_cnt <= 0 || right_cnt <= 0) return false;
    for (int i = 0; i < left_cnt; ++i) {
        for (int j = 0; j < right_cnt; ++j) {
            if (!st.is_peer(left[i], right[j])) return false;
        }
    }
    return true;
}

inline bool als_restricted_common(
    const CandidateState& st,
    const shared::ALS& a,
    const shared::ALS& b,
    uint64_t bit,
    int* a_holders,
    int& a_cnt,
    int* b_holders,
    int& b_cnt) {
    a_cnt = als_collect_holders_for_digit(st, a, bit, a_holders);
    b_cnt = als_collect_holders_for_digit(st, b, bit, b_holders);
    if (a_cnt <= 0 || b_cnt <= 0) return false;
    return holders_fully_cross_peer(st, a_holders, a_cnt, b_holders, b_cnt);
}

inline ApplyResult eliminate_from_seen_intersection(
    CandidateState& st,
    uint64_t bit,
    const int* h1,
    int h1_cnt,
    const int* h2,
    int h2_cnt,
    const shared::ALS* s1,
    const shared::ALS* s2,
    const shared::ALS* s3 = nullptr,
    const shared::ALS* s4 = nullptr) {
    const int nn = st.topo->nn;
    for (int t = 0; t < nn; ++t) {
        if (st.board->values[t] != 0) continue;
        if ((st.cands[t] & bit) == 0ULL) continue;
        if (shared::als_cell_in(*s1, t) || shared::als_cell_in(*s2, t) ||
            (s3 != nullptr && shared::als_cell_in(*s3, t)) ||
            (s4 != nullptr && shared::als_cell_in(*s4, t))) {
            continue;
        }

        bool sees_all = true;
        for (int i = 0; i < h1_cnt; ++i) {
            if (!st.is_peer(t, h1[i])) {
                sees_all = false;
                break;
            }
        }
        if (!sees_all) continue;
        for (int i = 0; i < h2_cnt; ++i) {
            if (!st.is_peer(t, h2[i])) {
                sees_all = false;
                break;
            }
        }
        if (!sees_all) continue;

        const ApplyResult er = st.eliminate(t, bit);
        if (er != ApplyResult::NoProgress) return er;
    }
    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_xy_wing_pass(CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, 4);
    if (als_cnt < 3) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, 128);

    int p_x[8]{}, w1_x[8]{}, p_y[8]{}, w2_y[8]{}, w1_z[8]{}, w2_z[8]{};
    int p_xc = 0, w1_xc = 0, p_yc = 0, w2_yc = 0, w1_zc = 0, w2_zc = 0;

    for (int ip = 0; ip < limit; ++ip) {
        const shared::ALS& pivot = sp.als_list[ip];
        for (int i1 = 0; i1 < limit; ++i1) {
            if (i1 == ip) continue;
            const shared::ALS& wing1 = sp.als_list[i1];
            if (als_overlap(pivot, wing1, words)) continue;

            const uint64_t common_p1 = pivot.digit_mask & wing1.digit_mask;
            if (common_p1 == 0ULL) continue;

            for (int i2 = i1 + 1; i2 < limit; ++i2) {
                if (i2 == ip) continue;
                const shared::ALS& wing2 = sp.als_list[i2];
                if (als_overlap(pivot, wing2, words) || als_overlap(wing1, wing2, words)) continue;

                const uint64_t common_p2 = pivot.digit_mask & wing2.digit_mask;
                if (common_p2 == 0ULL) continue;

                uint64_t wx = common_p1;
                while (wx != 0ULL) {
                    const uint64_t x = config::bit_lsb(wx);
                    wx = config::bit_clear_lsb_u64(wx);
                    if (!als_restricted_common(st, pivot, wing1, x, p_x, p_xc, w1_x, w1_xc)) continue;

                    uint64_t wy = common_p2 & ~x;
                    while (wy != 0ULL) {
                        const uint64_t y = config::bit_lsb(wy);
                        wy = config::bit_clear_lsb_u64(wy);
                        if (!als_restricted_common(st, pivot, wing2, y, p_y, p_yc, w2_y, w2_yc)) continue;

                        uint64_t zmask = (wing1.digit_mask & wing2.digit_mask) & ~(x | y);
                        while (zmask != 0ULL) {
                            const uint64_t z = config::bit_lsb(zmask);
                            zmask = config::bit_clear_lsb_u64(zmask);
                            w1_zc = als_collect_holders_for_digit(st, wing1, z, w1_z);
                            w2_zc = als_collect_holders_for_digit(st, wing2, z, w2_z);
                            if (w1_zc <= 0 || w2_zc <= 0) continue;
                            const ApplyResult er = eliminate_from_seen_intersection(
                                st, z, w1_z, w1_zc, w2_z, w2_zc, &pivot, &wing1, &wing2);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_chain_pass(CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, 4);
    if (als_cnt < 3) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, 96);

    int a_x[8]{}, b_x[8]{}, b_y[8]{}, c_y[8]{}, a_z[8]{}, c_z[8]{};
    int a_xc = 0, b_xc = 0, b_yc = 0, c_yc = 0, a_zc = 0, c_zc = 0;

    for (int ia = 0; ia < limit; ++ia) {
        const shared::ALS& a = sp.als_list[ia];
        for (int ib = 0; ib < limit; ++ib) {
            if (ib == ia) continue;
            const shared::ALS& b = sp.als_list[ib];
            if (als_overlap(a, b, words)) continue;

            uint64_t xmask = a.digit_mask & b.digit_mask;
            while (xmask != 0ULL) {
                const uint64_t x = config::bit_lsb(xmask);
                xmask = config::bit_clear_lsb_u64(xmask);
                if (!als_restricted_common(st, a, b, x, a_x, a_xc, b_x, b_xc)) continue;

                for (int ic = 0; ic < limit; ++ic) {
                    if (ic == ia || ic == ib) continue;
                    const shared::ALS& c = sp.als_list[ic];
                    if (als_overlap(a, c, words) || als_overlap(b, c, words)) continue;

                    uint64_t ymask = (b.digit_mask & c.digit_mask) & ~x;
                    while (ymask != 0ULL) {
                        const uint64_t y = config::bit_lsb(ymask);
                        ymask = config::bit_clear_lsb_u64(ymask);
                        if (!als_restricted_common(st, b, c, y, b_y, b_yc, c_y, c_yc)) continue;

                        uint64_t zmask = (a.digit_mask & c.digit_mask) & ~(x | y);
                        while (zmask != 0ULL) {
                            const uint64_t z = config::bit_lsb(zmask);
                            zmask = config::bit_clear_lsb_u64(zmask);
                            a_zc = als_collect_holders_for_digit(st, a, z, a_z);
                            c_zc = als_collect_holders_for_digit(st, c, z, c_z);
                            if (a_zc <= 0 || c_zc <= 0) continue;
                            const ApplyResult er = eliminate_from_seen_intersection(
                                st, z, a_z, a_zc, c_z, c_zc, &a, &b, &c);
                            if (er != ApplyResult::NoProgress) return er;
                        }
                    }
                }
            }
        }
    }

    return ApplyResult::NoProgress;
}

inline ApplyResult direct_als_aic_pass(CandidateState& st) {
    auto& sp = shared::exact_pattern_scratchpad();
    const int als_cnt = shared::build_als_list(st, 2, 4);
    if (als_cnt < 4) return ApplyResult::NoProgress;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    const int limit = std::min(als_cnt, 48);

    int a_h[8]{}, b_h[8]{}, c_h[8]{}, d_h[8]{}, az[8]{}, dz[8]{};
    int a_hc = 0, b_hc = 0, c_hc = 0, d_hc = 0, azc = 0, dzc = 0;

    for (int ia = 0; ia < limit; ++ia) {
        const shared::ALS& a = sp.als_list[ia];
        for (int ib = 0; ib < limit; ++ib) {
            if (ib == ia) continue;
            const shared::ALS& b = sp.als_list[ib];
            if (als_overlap(a, b, words)) continue;

            uint64_t xmask = a.digit_mask & b.digit_mask;
            while (xmask != 0ULL) {
                const uint64_t x = config::bit_lsb(xmask);
                xmask = config::bit_clear_lsb_u64(xmask);
                if (!als_restricted_common(st, a, b, x, a_h, a_hc, b_h, b_hc)) continue;

                for (int ic = 0; ic < limit; ++ic) {
                    if (ic == ia || ic == ib) continue;
                    const shared::ALS& c = sp.als_list[ic];
                    if (als_overlap(a, c, words) || als_overlap(b, c, words)) continue;

                    uint64_t ymask = (b.digit_mask & c.digit_mask) & ~x;
                    while (ymask != 0ULL) {
                        const uint64_t y = config::bit_lsb(ymask);
                        ymask = config::bit_clear_lsb_u64(ymask);
                        if (!als_restricted_common(st, b, c, y, b_h, b_hc, c_h, c_hc)) continue;

                        for (int id = 0; id < limit; ++id) {
                            if (id == ia || id == ib || id == ic) continue;
                            const shared::ALS& d = sp.als_list[id];
                            if (als_overlap(a, d, words) || als_overlap(b, d, words) || als_overlap(c, d, words)) continue;

                            uint64_t wmask = (c.digit_mask & d.digit_mask) & ~(x | y);
                            while (wmask != 0ULL) {
                                const uint64_t w = config::bit_lsb(wmask);
                                wmask = config::bit_clear_lsb_u64(wmask);
                                if (!als_restricted_common(st, c, d, w, c_h, c_hc, d_h, d_hc)) continue;

                                uint64_t zmask = (a.digit_mask & d.digit_mask) & ~(x | y | w);
                                while (zmask != 0ULL) {
                                    const uint64_t z = config::bit_lsb(zmask);
                                    zmask = config::bit_clear_lsb_u64(zmask);
                                    azc = als_collect_holders_for_digit(st, a, z, az);
                                    dzc = als_collect_holders_for_digit(st, d, z, dz);
                                    if (azc <= 0 || dzc <= 0) continue;
                                    const ApplyResult er = eliminate_from_seen_intersection(
                                        st, z, az, azc, dz, dzc, &a, &b, &c, &d);
                                    if (er != ApplyResult::NoProgress) return er;
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

inline ApplyResult apply_als_xy_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    ApplyResult ar;

    StrategyStats tmp{};
    ar = p6_diabolical::apply_als_xz(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_xy_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    ar = p5_expert::apply_xyz_wing(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_xy_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    if (!als_deep_pass_allowed(st)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    ar = direct_als_xy_wing_pass(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_xy_wing = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_als_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    ApplyResult ar;

    StrategyStats tmp{};
    ar = p6_diabolical::apply_als_xz(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_chain = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    ar = p6_diabolical::apply_xy_chain(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_chain = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    if (!als_deep_pass_allowed(st)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    ar = direct_als_chain_pass(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_chain = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

inline ApplyResult apply_als_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    ApplyResult ar;

    StrategyStats tmp{};
    ar = p6_diabolical::apply_x_chain(st, tmp, r);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_aic = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    ar = direct_als_chain_pass(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_aic = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    if (!als_deep_pass_allowed(st)) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    ar = direct_als_aic_pass(st);
    if (ar == ApplyResult::Contradiction) {
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }
    if (ar == ApplyResult::Progress) {
        ++s.hit_count;
        r.used_als_aic = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ar;
    }

    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p7_nightmare
