#pragma once

#include <cstdint>
#include <random>

#include "../../core/board.h"
#include "template_exocet.h"

namespace sudoku_hpc::pattern_forcing {

class TemplateMslsOverlay {
public:
    static bool build_msls(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = r1;
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = c1;
        for (int g = 0; g < 64 && r2 == r1; ++g) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        for (int g = 0; g < 64 && c2 == c1; ++g) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        if (r1 == r2 || c1 == c2) return false;

        const int a = r1 * n + c1;
        const int b = r1 * n + c2;
        const int c = r2 * n + c1;
        const int d = r2 * n + c2;

        const int d1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d2 = d1;
        for (int g = 0; g < 64 && d2 == d1; ++g) d2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d3 = d2;
        for (int g = 0; g < 64 && (d3 == d1 || d3 == d2); ++g) d3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int d4 = d3;
        for (int g = 0; g < 64 && (d4 == d1 || d4 == d2 || d4 == d3); ++g) d4 = static_cast<int>(rng() % static_cast<uint64_t>(n));

        const uint64_t m12 = (1ULL << d1) | (1ULL << d2);
        const uint64_t m123 = m12 | (1ULL << d3);
        const uint64_t m124 = m12 | (1ULL << d4);
        const uint64_t m234 = (1ULL << d2) | (1ULL << d3) | (1ULL << d4);

        plan.add_anchor(a, m123);
        plan.add_anchor(b, m124);
        plan.add_anchor(c, m123);
        plan.add_anchor(d, m124);

        int added = 0;
        for (int cc = 0; cc < n && added < 2; ++cc) {
            if (cc == c1 || cc == c2) continue;
            if (plan.add_anchor(r1 * n + cc, m234)) ++added;
        }
        added = 0;
        for (int rr = 0; rr < n && added < 2; ++rr) {
            if (rr == r1 || rr == r2) continue;
            if (plan.add_anchor(rr * n + c1, m234)) ++added;
        }

        plan.valid = (plan.anchor_count >= 6);
        return plan.valid;
    }

    static bool build_overlay(const GenericTopology& topo, std::mt19937_64& rng, ExactPatternTemplatePlan& plan) {
        plan = {};
        const int n = topo.n;
        if (n < 6) return false;

        int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int r2 = r1;
        int r3 = r2;
        int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        int c2 = c1;
        for (int g = 0; g < 64 && r2 == r1; ++g) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
        for (int g = 0; g < 64 && r3 == r1; ++g) r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
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
        plan.add_anchor(r3 * n + c1, alt_mask);
        plan.add_anchor(r3 * n + c2, alt_mask);

        plan.valid = (plan.anchor_count >= 6);
        return plan.valid;
    }
};

} // namespace sudoku_hpc::pattern_forcing
