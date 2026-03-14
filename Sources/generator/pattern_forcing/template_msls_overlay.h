//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateMslsOverlay {
public:
    static bool push_unique_anchor(
        ExactPatternTemplatePlan& plan,
        int idx,
        uint64_t mask) {
        return plan.add_anchor(idx, mask);
    }

    static bool build_msls(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        if (n < 6) return false;

        const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
        const int br = box / topo.box_cols_count;
        const int bc = box % topo.box_cols_count;
        const int r0 = br * topo.box_rows;
        const int c0 = bc * topo.box_cols;

        const int row_pick = static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));
        const int col_pick = static_cast<int>(rng() % static_cast<uint64_t>(topo.box_cols));
        const int row = r0 + row_pick;
        const int col = c0 + col_pick;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 64 && d2 == d1; ++g) d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d3 = d2;
        for (int g = 0; g < 64 && (d3 == d1 || d3 == d2); ++g) d3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d4 = d3;
        for (int g = 0; g < 64 && (d4 == d1 || d4 == d2 || d4 == d3); ++g) d4 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d5 = d4;
        for (int g = 0; g < 64 && (d5 == d1 || d5 == d2 || d5 == d3 || d5 == d4); ++g) d5 = static_cast<int>(rng() % static_cast<uint64_t>(n));

        int d6 = d5;
        for (int g = 0; g < 64 && (d6 == d1 || d6 == d2 || d6 == d3 || d6 == d4 || d6 == d5); ++g) {
            d6 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }

        const uint64_t core = (1ULL << d1) | (1ULL << d2);
        const uint64_t joint_mask = core | (1ULL << d3) | (1ULL << d4);
        const uint64_t row_mask = core | (1ULL << d3) | (1ULL << d5);
        const uint64_t col_mask = core | (1ULL << d4) | (1ULL << d6);
        const uint64_t box_mask = core | (1ULL << d5) | (1ULL << d6);

        const int joint = row * n + col;
        if (!push_unique_anchor(plan, joint, joint_mask)) return false;

        int row_box_peer = -1;
        for (int dc = 0; dc < topo.box_cols; ++dc) {
            if (dc == col_pick) continue;
            row_box_peer = row * n + (c0 + dc);
            break;
        }
        int col_box_peer = -1;
        for (int dr = 0; dr < topo.box_rows; ++dr) {
            if (dr == row_pick) continue;
            col_box_peer = (r0 + dr) * n + col;
            break;
        }
        if (row_box_peer < 0 || col_box_peer < 0) return false;
        if (!push_unique_anchor(plan, row_box_peer, row_mask)) return false;
        if (!push_unique_anchor(plan, col_box_peer, col_mask)) return false;

        int row_outside = -1;
        int row_support = -1;
        for (int cc = 0; cc < n; ++cc) {
            if (cc >= c0 && cc < c0 + topo.box_cols) continue;
            const int idx = row * n + cc;
            if (row_outside < 0) row_outside = idx;
            else {
                row_support = idx;
                break;
            }
        }
        int col_outside = -1;
        int col_support = -1;
        for (int rr = 0; rr < n; ++rr) {
            if (rr >= r0 && rr < r0 + topo.box_rows) continue;
            const int idx = rr * n + col;
            if (col_outside < 0) col_outside = idx;
            else {
                col_support = idx;
                break;
            }
        }
        if (row_outside < 0 || col_outside < 0) return false;
        if (!push_unique_anchor(plan, row_outside, row_mask)) return false;
        if (!push_unique_anchor(plan, col_outside, col_mask)) return false;

        int box_only = -1;
        for (int dr = 0; dr < topo.box_rows && box_only < 0; ++dr) {
            for (int dc = 0; dc < topo.box_cols; ++dc) {
                const int rr = r0 + dr;
                const int cc = c0 + dc;
                if (rr == row || cc == col) continue;
                box_only = rr * n + cc;
                break;
            }
        }
        if (box_only < 0) return false;

        if (!plan.add_skeleton(box_only, box_mask)) return false;
        if (row_support >= 0) plan.add_skeleton(row_support, row_mask | (1ULL << d4));
        if (col_support >= 0) plan.add_skeleton(col_support, col_mask | (1ULL << d3));

        plan.valid = (plan.anchor_count >= 5 && plan.skeleton_count >= 6);
        return plan.valid;
    }

    static bool build_overlay(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        plan.explicit_skeleton = true;
        const int n = topo.n;
        const uint64_t full = pf_full_mask_for_n(n);
        if (n < 6) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = r1;
        int r3 = r2;
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = c1;
        for (int g = 0; g < 64 && r2 == r1; ++g) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        for (int g = 0; g < 64 && (r3 == r1 || r3 == r2); ++g) {
            r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        }
        for (int g = 0; g < 64 && c2 == c1; ++g) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        if (r1 == r2 || r1 == r3 || r2 == r3 || c1 == c2) return false;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 64 && d2 == d1; ++g) d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d3 = d2;
        for (int g = 0; g < 64 && (d3 == d1 || d3 == d2); ++g) d3 = static_cast<int>(rng() % static_cast<uint64_t>(n));

        const uint64_t pivot_mask = (1ULL << d1) | (1ULL << d2);
        const uint64_t alt_mask = pivot_mask | (1ULL << d3);

        plan.add_anchor(r1 * n + c1, pivot_mask);
        plan.add_anchor(r1 * n + c2, alt_mask);
        plan.add_anchor(r2 * n + c1, alt_mask);
        plan.add_anchor(r2 * n + c2, pivot_mask);
        // Trzeci rzÄ…d jest tylko miÄ™kkim szkieletem overlay, nie twardÄ… kotwicÄ….
        plan.add_skeleton(r3 * n + c1, alt_mask);
        plan.add_skeleton(r3 * n + c2, alt_mask);

        plan.valid = (plan.anchor_count >= 4);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing
