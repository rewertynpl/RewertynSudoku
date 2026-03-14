//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"
#include "../shared/als_builder.h"
#include "../shared/exact_pattern_scratchpad.h"

namespace sudoku_hpc::logic::p6_diabolical {

inline int als_xz_max_size(const CandidateState& st) {
    const int n = st.topo->n;
    if (n <= 16) return 6;
    if (n <= 25) return 5;
    return 4;
}

inline int als_xz_min_size(const CandidateState& st) {
    const int n = st.topo->n;
    return (n <= 25) ? 1 : 2;
}

inline int als_xz_pair_limit(const CandidateState& st, int als_cnt) {
    const int n = st.topo->n;
    if (n <= 16) return std::min(als_cnt, 512);
    return std::min(als_cnt, std::clamp(128 + 6 * n, 160, 256));
}

inline ApplyResult apply_als_xz(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;

    const int nn = st.topo->nn;
    const int words = (nn + 63) >> 6;
    bool progress = false;
    auto& sp = shared::exact_pattern_scratchpad();

    const int als_cnt = shared::build_als_list(st, als_xz_min_size(st), als_xz_max_size(st));
    if (als_cnt < 2) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    const int limit = als_xz_pair_limit(st, als_cnt);
    int a_holders[8]{};
    int b_holders[8]{};
    int a_z_holders[8]{};
    int b_z_holders[8]{};
    int a_cnt = 0;
    int b_cnt = 0;
    int a_z_cnt = 0;
    int b_z_cnt = 0;

    for (int i = 0; i < limit; ++i) {
        const shared::ALS& a = sp.als_list[i];
        for (int j = i + 1; j < limit; ++j) {
            const shared::ALS& b = sp.als_list[j];
            if (shared::als_overlap(a, b, words)) continue;

            uint64_t common = a.digit_mask & b.digit_mask;
            if (std::popcount(common) < 2) continue;

            while (common != 0ULL) {
                const uint64_t x = config::bit_lsb(common);
                common = config::bit_clear_lsb_u64(common);

                if (!shared::als_restricted_common(
                        st, a, b, x, a_holders, a_cnt, b_holders, b_cnt)) {
                    continue;
                }

                uint64_t zmask = (a.digit_mask & b.digit_mask) & ~x;
                while (zmask != 0ULL) {
                    const uint64_t z = config::bit_lsb(zmask);
                    zmask = config::bit_clear_lsb_u64(zmask);

                    a_z_cnt = shared::als_collect_holders_for_digit(st, a, z, a_z_holders);
                    b_z_cnt = shared::als_collect_holders_for_digit(st, b, z, b_z_holders);
                    if (a_z_cnt <= 0 || b_z_cnt <= 0 || a_z_cnt > 8 || b_z_cnt > 8) continue;

                    const ApplyResult er = shared::als_eliminate_from_seen_intersection(
                        st, z, a_z_holders, a_z_cnt, b_z_holders, b_z_cnt, &a, &b);
                    if (er == ApplyResult::Contradiction) {
                        s.elapsed_ns += st.now_ns() - t0;
                        return er;
                    }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_als_xz = true;
                        progress = true;
                    }
                }
            }
        }
    }

    s.elapsed_ns += st.now_ns() - t0;
    return progress ? ApplyResult::Progress : ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p6_diabolical
