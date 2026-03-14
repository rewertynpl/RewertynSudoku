//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include "pattern_planter_exact_templates.h"

namespace sudoku_hpc::pattern_forcing {

// --- Szablony luĹşne (fallback gdy brakuje Exact Template) ---

inline bool build_chain_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 2) return false;
    const int n = topo.n;
    int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int r2 = r1;
    int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c2 = c1;
    for (int t = 0; t < 64 && r2 == r1; ++t) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int t = 0; t < 64 && c2 == c1; ++t) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (r1 == r2 || c1 == c2) return false;
    sc.add_anchor(r1 * n + c1);
    sc.add_anchor(r1 * n + c2);
    sc.add_anchor(r2 * n + c2);
    sc.add_anchor(r2 * n + c1);
    return sc.anchor_count >= 4;
}

inline bool build_forcing_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 4) return false;
    const int n = topo.n;
    int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int r2 = r1;
    int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c2 = c1;
    for (int t = 0; t < 64 && r2 == r1; ++t) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int t = 0; t < 64 && c2 == c1; ++t) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (r1 == r2 || c1 == c2) return false;

    const int p = r1 * n + c1;
    const int a = r1 * n + c2;
    const int b = r2 * n + c2;
    const int t = r2 * n + c1;
    sc.add_anchor(p);
    sc.add_anchor(a);
    sc.add_anchor(b);
    sc.add_anchor(t);

    int row_support = -1;
    for (int cc = 0; cc < n; ++cc) {
        if (cc == c1 || cc == c2) continue;
        row_support = r1 * n + cc;
        break;
    }
    int col_support = -1;
    for (int rr = 0; rr < n; ++rr) {
        if (rr == r1 || rr == r2) continue;
        col_support = rr * n + c2;
        break;
    }
    if (row_support >= 0) sc.add_anchor(row_support);
    if (col_support >= 0) sc.add_anchor(col_support);

    int cross_support = -1;
    for (int rr = 0; rr < n && cross_support < 0; ++rr) {
        if (rr == r1 || rr == r2) continue;
        for (int cc = 0; cc < n; ++cc) {
            if (cc == c1 || cc == c2) continue;
            const int idx = rr * n + cc;
            const bool sees_a =
                topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(a)] ||
                topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(a)] ||
                topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(a)];
            const bool sees_b =
                topo.cell_row[static_cast<size_t>(idx)] == topo.cell_row[static_cast<size_t>(b)] ||
                topo.cell_col[static_cast<size_t>(idx)] == topo.cell_col[static_cast<size_t>(b)] ||
                topo.cell_box[static_cast<size_t>(idx)] == topo.cell_box[static_cast<size_t>(b)];
            if (sees_a || sees_b) {
                cross_support = idx;
                break;
            }
        }
    }
    if (cross_support >= 0) sc.add_anchor(cross_support);

    return sc.anchor_count >= 6;
}

inline bool build_exocet_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.box_rows <= 1 || topo.box_cols <= 1) return false;
    const int n = topo.n;
    const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
    const int house = 2 * n + box;
    const int start = topo.house_offsets[static_cast<size_t>(house)];
    const int end = topo.house_offsets[static_cast<size_t>(house + 1)];
    if (end - start < 2) return false;

    int b1 = topo.houses_flat[static_cast<size_t>(start + (rng() % static_cast<uint64_t>(end - start)))];
    int b2 = b1;
    for (int t = 0; t < 128; ++t) {
        const int c = topo.houses_flat[static_cast<size_t>(start + (rng() % static_cast<uint64_t>(end - start)))];
        if (c == b1) continue;
        if (topo.cell_row[static_cast<size_t>(c)] == topo.cell_row[static_cast<size_t>(b1)]) continue;
        if (topo.cell_col[static_cast<size_t>(c)] == topo.cell_col[static_cast<size_t>(b1)]) continue;
        b2 = c;
        break;
    }
    if (b1 == b2) return false;
    sc.add_anchor(b1);
    sc.add_anchor(b2);

    const int r1 = topo.cell_row[static_cast<size_t>(b1)];
    const int r2 = topo.cell_row[static_cast<size_t>(b2)];
    const int c1 = topo.cell_col[static_cast<size_t>(b1)];
    const int c2 = topo.cell_col[static_cast<size_t>(b2)];
    sc.add_anchor(r1 * n + c2);
    sc.add_anchor(r2 * n + c1);
    return sc.anchor_count >= 2;
}

inline bool build_loop_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) return false;
    const int n = topo.n;
    int r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    sc.add_anchor(r3 * n + c3);
    r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    c3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    sc.add_anchor(r3 * n + c3);
    return sc.anchor_count >= 4;
}

inline bool build_color_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) return false;
    const int n = topo.n;
    const int r = static_cast<int>(rng() % static_cast<uint64_t>(n));
    const int c = static_cast<int>(rng() % static_cast<uint64_t>(n));
    sc.add_anchor(r * n + c);
    sc.add_anchor(r * n + static_cast<int>(rng() % static_cast<uint64_t>(n)));
    return sc.anchor_count >= 5;
}

inline bool build_petal_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
    sc.add_anchor(pivot);
    const int start = topo.peer_offsets[static_cast<size_t>(pivot)];
    const int end = topo.peer_offsets[static_cast<size_t>(pivot + 1)];
    for (int p = start; p < end && sc.anchor_count < 5; ++p) {
        sc.add_anchor(topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 4;
}

inline bool build_intersection_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.box_rows <= 1 || topo.box_cols <= 1) return false;
    const int n = topo.n;
    const int box = static_cast<int>(rng() % static_cast<uint64_t>(n));
    const int br = box / topo.box_cols_count;
    const int bc = box % topo.box_cols_count;
    const int r0 = br * topo.box_rows;
    const int c0 = bc * topo.box_cols;
    const int row = r0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_rows));
    const int col = c0 + static_cast<int>(rng() % static_cast<uint64_t>(topo.box_cols));
    for (int dc = 0; dc < topo.box_cols; ++dc) sc.add_anchor(row * n + (c0 + dc));
    for (int dr = 0; dr < topo.box_rows; ++dr) sc.add_anchor((r0 + dr) * n + col);
    return sc.anchor_count >= 4;
}

inline bool build_fish_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 4) return false;
    const int n = topo.n;
    const int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int r2 = r1;
    int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c2 = c1;
    for (int g = 0; g < 64 && r2 == r1; ++g) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int g = 0; g < 64 && c2 == c1; ++g) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (r1 == r2 || c1 == c2) return false;
    sc.add_anchor(r1 * n + c1);
    sc.add_anchor(r1 * n + c2);
    sc.add_anchor(r2 * n + c1);
    sc.add_anchor(r2 * n + c2);
    int c3 = c2;
    for (int g = 0; g < 64 && (c3 == c1 || c3 == c2); ++g) c3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (c3 != c1 && c3 != c2) {
        sc.add_anchor(r1 * n + c3);
        sc.add_anchor(r2 * n + c3);
    }
    return sc.anchor_count >= 4;
}

inline bool build_franken_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_fish_like_anchors(topo, sc, rng) || topo.box_rows <= 1 || topo.box_cols <= 1) return false;
    const int seed = sc.anchors[0];
    const int box = topo.cell_box[static_cast<size_t>(seed)];
    const int house = 2 * topo.n + box;
    const int p0 = topo.house_offsets[static_cast<size_t>(house)];
    const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = p0; p < p1 && sc.anchor_count < 7; ++p) {
        sc.add_anchor(topo.houses_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 5;
}

inline bool build_mutant_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_franken_like_anchors(topo, sc, rng)) return false;
    sc.add_anchor(static_cast<int>(rng() % static_cast<uint64_t>(topo.nn)));
    sc.add_anchor(static_cast<int>(rng() % static_cast<uint64_t>(topo.nn)));
    return sc.anchor_count >= 6;
}

inline bool build_squirm_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (topo.n < 6) return false;
    const int n = topo.n;
    int rows[3] = { static_cast<int>(rng() % static_cast<uint64_t>(n)), -1, -1 };
    rows[1] = rows[0];
    rows[2] = rows[0];
    for (int g = 0; g < 64 && rows[1] == rows[0]; ++g) rows[1] = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int g = 0; g < 96 && (rows[2] == rows[0] || rows[2] == rows[1]); ++g) rows[2] = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int cols[3] = { static_cast<int>(rng() % static_cast<uint64_t>(n)), -1, -1 };
    cols[1] = cols[0];
    cols[2] = cols[0];
    for (int g = 0; g < 64 && cols[1] == cols[0]; ++g) cols[1] = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int g = 0; g < 96 && (cols[2] == cols[0] || cols[2] == cols[1]); ++g) cols[2] = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (rows[0] == rows[1] || rows[0] == rows[2] || rows[1] == rows[2] ||
        cols[0] == cols[1] || cols[0] == cols[2] || cols[1] == cols[2]) return false;
    for (int ri = 0; ri < 3; ++ri) {
        for (int ci = 0; ci < 3; ++ci) {
            sc.add_anchor(rows[ri] * n + cols[ci]);
        }
    }
    return sc.anchor_count >= 9;
}

inline bool build_als_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    const int n = topo.n;
    if (n < 4) return false;
    const int pivot = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
    sc.add_anchor(pivot);
    const int start = topo.peer_offsets[static_cast<size_t>(pivot)];
    const int end = topo.peer_offsets[static_cast<size_t>(pivot + 1)];
    for (int p = start; p < end && sc.anchor_count < 5; ++p) {
        sc.add_anchor(topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 3;
}

inline bool build_exclusion_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    const int a = static_cast<int>(rng() % static_cast<uint64_t>(topo.nn));
    sc.add_anchor(a);
    const int start = topo.peer_offsets[static_cast<size_t>(a)];
    const int end = topo.peer_offsets[static_cast<size_t>(a + 1)];
    for (int p = start; p < end && sc.anchor_count < 4; ++p) {
        sc.add_anchor(topo.peers_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 2;
}

inline bool build_aic_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    return build_chain_anchors(topo, sc, rng);
}

inline bool build_grouped_aic_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_chain_anchors(topo, sc, rng)) return false;
    const int seed = sc.anchors[0];
    const int box = topo.cell_box[static_cast<size_t>(seed)];
    const int house = 2 * topo.n + box;
    const int p0 = topo.house_offsets[static_cast<size_t>(house)];
    const int p1 = topo.house_offsets[static_cast<size_t>(house + 1)];
    for (int p = p0; p < p1 && sc.anchor_count < 6; ++p) {
        sc.add_anchor(topo.houses_flat[static_cast<size_t>(p)]);
    }
    return sc.anchor_count >= 5;
}

inline bool build_grouped_cycle_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    return build_loop_like_anchors(topo, sc, rng);
}

inline bool build_niceloop_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    return build_loop_like_anchors(topo, sc, rng);
}

inline bool build_empty_rectangle_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    if (!build_intersection_like_anchors(topo, sc, rng)) return false;
    if (sc.anchor_count <= 0) return false;
    const int pivot = sc.anchors[0];
    const int row = topo.cell_row[static_cast<size_t>(pivot)];
    const int col = topo.cell_col[static_cast<size_t>(pivot)];
    for (int p = topo.peer_offsets[static_cast<size_t>(pivot)];
         p < topo.peer_offsets[static_cast<size_t>(pivot + 1)] && sc.anchor_count < 6; ++p) {
        const int idx = topo.peers_flat[static_cast<size_t>(p)];
        if (topo.cell_row[static_cast<size_t>(idx)] == row || topo.cell_col[static_cast<size_t>(idx)] == col) {
            sc.add_anchor(idx);
        }
    }
    return sc.anchor_count >= 4;
}

inline bool build_remote_pairs_like_anchors(const GenericTopology& topo, PatternScratch& sc, std::mt19937_64& rng) {
    const int n = topo.n;
    if (n < 4) return false;

    int r1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int r2 = r1;
    int r3 = r1;
    int c1 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    int c2 = c1;
    int c3 = c1;
    for (int t = 0; t < 64 && r2 == r1; ++t) r2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int t = 0; t < 64 && r3 == r1; ++t) r3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int t = 0; t < 64 && c2 == c1; ++t) c2 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    for (int t = 0; t < 64 && (c3 == c1 || c3 == c2); ++t) c3 = static_cast<int>(rng() % static_cast<uint64_t>(n));
    if (r1 == r2 || c1 == c2) return false;

    sc.add_anchor(r1 * n + c1);
    sc.add_anchor(r1 * n + c2);
    sc.add_anchor(r2 * n + c2);
    sc.add_anchor(r2 * n + c1);
    if (c3 != c1 && c3 != c2) sc.add_anchor(r1 * n + c3);
    if (r3 != r1 && r3 != r2) sc.add_anchor(r3 * n + c2);
    return sc.anchor_count >= 5;
}

inline int default_anchor_count(const GenericTopology& topo, PatternKind kind) {
    if (topo.n >= 36) {
        switch (kind) {
        case PatternKind::ExocetLike: return 18;
        case PatternKind::LoopLike: return 20;
        case PatternKind::ForcingLike: return 20;
        case PatternKind::ColorLike: return 18;
        case PatternKind::PetalLike: return 18;
        case PatternKind::IntersectionLike: return 18;
        case PatternKind::FishLike: return 18;
        case PatternKind::FrankenLike: return 18;
        case PatternKind::MutantLike: return 18;
        case PatternKind::SquirmLike: return 20;
        case PatternKind::AlsLike: return 18;
        case PatternKind::ExclusionLike: return 16;
        case PatternKind::AicLike: return 18;
        case PatternKind::GroupedAicLike: return 18;
        case PatternKind::GroupedCycleLike: return 18;
        case PatternKind::NiceLoopLike: return 18;
        case PatternKind::XChainLike: return 18;
        case PatternKind::XYChainLike: return 18;
        case PatternKind::EmptyRectangleLike: return 16;
        case PatternKind::RemotePairsLike: return 18;
        case PatternKind::SwordfishLike: return 18;
        case PatternKind::JellyfishLike: return 20;
        case PatternKind::FinnedFishLike: return 18;
        case PatternKind::Chain: return 16;
        default: return 0;
        }
    }
    if (topo.n >= 25) {
        switch (kind) {
        case PatternKind::ExocetLike: return 14;
        case PatternKind::LoopLike: return 16;
        case PatternKind::ForcingLike: return 16;
        case PatternKind::ColorLike: return 14;
        case PatternKind::PetalLike: return 14;
        case PatternKind::IntersectionLike: return 14;
        case PatternKind::FishLike: return 14;
        case PatternKind::FrankenLike: return 14;
        case PatternKind::MutantLike: return 14;
        case PatternKind::SquirmLike: return 16;
        case PatternKind::AlsLike: return 14;
        case PatternKind::ExclusionLike: return 12;
        case PatternKind::AicLike: return 14;
        case PatternKind::GroupedAicLike: return 14;
        case PatternKind::GroupedCycleLike: return 14;
        case PatternKind::NiceLoopLike: return 14;
        case PatternKind::XChainLike: return 14;
        case PatternKind::XYChainLike: return 14;
        case PatternKind::EmptyRectangleLike: return 12;
        case PatternKind::RemotePairsLike: return 14;
        case PatternKind::SwordfishLike: return 14;
        case PatternKind::JellyfishLike: return 16;
        case PatternKind::FinnedFishLike: return 14;
        case PatternKind::Chain: return 12;
        default: return 0;
        }
    }
    switch (kind) {
    case PatternKind::ExocetLike: return std::clamp(topo.n / 2, 4, 10);
    case PatternKind::LoopLike: return std::clamp(topo.n / 2 + 2, 6, 12);
    case PatternKind::ForcingLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::ColorLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::PetalLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::IntersectionLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::FishLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::FrankenLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::MutantLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::SquirmLike: return std::clamp(topo.n / 2 + 2, 8, 14);
    case PatternKind::AlsLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::ExclusionLike: return std::clamp(topo.n / 3 + 4, 4, 10);
    case PatternKind::AicLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::GroupedAicLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::GroupedCycleLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::NiceLoopLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::XChainLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::XYChainLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::EmptyRectangleLike: return std::clamp(topo.n / 3 + 4, 4, 10);
    case PatternKind::RemotePairsLike: return std::clamp(topo.n / 2, 7, 12);
    case PatternKind::SwordfishLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::JellyfishLike: return std::clamp(topo.n / 2 + 2, 8, 14);
    case PatternKind::FinnedFishLike: return std::clamp(topo.n / 2, 6, 12);
    case PatternKind::Chain: return std::clamp(topo.n / 3 + 3, 4, 10);
    default: return 0;
    }
}

inline void apply_anchor_masks(const GenericTopology& topo, PatternScratch& sc, PatternKind kind, std::mt19937_64& rng) {
    if (sc.anchor_count <= 0) return;
    const uint64_t full = pf_full_mask_for_n(topo.n);
    const bool large_geom = topo.n >= 25;
    const int want_a = large_geom ? 3 : 2;
    const int want_b = large_geom ? 4 : 2;
    const int want_c = large_geom ? 5 : 3;
    uint64_t mask_a = random_digit_mask(topo.n, want_a, rng);
    uint64_t mask_b = random_digit_mask(topo.n, want_b, rng);
    uint64_t mask_c = random_digit_mask(topo.n, want_c, rng);
    
    if (kind == PatternKind::ExocetLike) {
        const uint64_t shared = random_digit_mask(topo.n, large_geom ? 4 : 3, rng);
        if (sc.anchor_count >= 1) sc.allowed_masks[static_cast<size_t>(sc.anchors[0])] = shared;
        if (sc.anchor_count >= 2) sc.allowed_masks[static_cast<size_t>(sc.anchors[1])] = shared;
        for (int i = 2; i < sc.anchor_count; ++i) {
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] =
                large_geom ? (mask_b | random_extra_digit(full, mask_b, rng)) & full : mask_c;
        }
        return;
    }
    if (kind == PatternKind::ForcingLike) {
        const uint64_t d1 = random_digit_mask(topo.n, 1, rng);
        const uint64_t d2 = random_extra_digit(full, d1, rng);
        const uint64_t d3 = random_extra_digit(full, d1 | d2, rng);
        const uint64_t d4 = random_extra_digit(full, d1 | d2 | d3, rng);
        const uint64_t d5 = random_extra_digit(full, d1 | d2 | d3 | d4, rng);

        const uint64_t m12 = (d1 | d2) & full;
        const uint64_t m23 = (d2 | d3) & full;
        const uint64_t m13 = (d1 | d3) & full;
        const uint64_t pivot_mask = (m12 | d4) & full;
        const uint64_t branch_a_mask = (m23 | d4) & full;
        const uint64_t branch_b_mask = (m13 | d5) & full;
        const uint64_t target_mask = (m12 | d5) & full;
        uint64_t row_support_mask = (m12 | d3 | d5) & full;
        uint64_t col_support_mask = (m23 | d1 | d5) & full;
        uint64_t cross_support_mask = (m13 | d2 | d4) & full;

        if (large_geom) {
            row_support_mask |= random_extra_digit(full, row_support_mask, rng);
            col_support_mask |= random_extra_digit(full, col_support_mask, rng);
            cross_support_mask |= random_extra_digit(full, cross_support_mask, rng);
        }

        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = target_mask;
            switch (i) {
            case 0: m = pivot_mask; break;       // p
            case 1: m = branch_a_mask; break;    // a
            case 2: m = branch_b_mask; break;    // b
            case 3: m = target_mask; break;      // t
            case 4: m = row_support_mask; break;
            case 5: m = col_support_mask; break;
            default: m = cross_support_mask; break;
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::LoopLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i % 3 == 0) ? mask_c : ((i & 1) ? mask_a : mask_b);
            if (large_geom && (i % 4 == 0)) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::ColorLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = ((i & 1) == 0) ? mask_a : (mask_a | random_extra_digit(full, mask_a, rng));
            if (std::popcount(m) < 2) m |= random_extra_digit(full, m, rng);
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::PetalLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i == 0) ? (mask_a | mask_b) : ((i & 1) ? mask_a : mask_b);
            if (i == 0 || (large_geom && (i % 3 == 0))) m |= random_extra_digit(full, m, rng);
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::IntersectionLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i % 3 == 0) ? (mask_a | mask_b) : ((i & 1) ? mask_b : mask_c);
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::FishLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = core;
            if (i >= 4 || (large_geom && (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::FrankenLike || kind == PatternKind::MutantLike || kind == PatternKind::SquirmLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = core;
            if (kind == PatternKind::SquirmLike) {
                if (i >= 9 || (large_geom && (i % 4 == 0))) m |= random_extra_digit(full, m, rng);
            } else if (i >= 4 || (large_geom && (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            if (kind == PatternKind::MutantLike && i >= 5) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::SwordfishLike || kind == PatternKind::JellyfishLike || kind == PatternKind::FinnedFishLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = core;
            if (kind == PatternKind::JellyfishLike && (i >= 9 || (large_geom && (i % 4 == 0)))) {
                m |= random_extra_digit(full, m, rng);
            }
            if (kind == PatternKind::FinnedFishLike && (i >= 4 || (i % 3 == 0))) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::AlsLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i % 3 == 0) ? (mask_a | mask_b) : (mask_b | mask_c);
            if (std::popcount(m) < 3) m |= random_extra_digit(full, m, rng);
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::ExclusionLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i & 1) ? mask_a : mask_b;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::AicLike || kind == PatternKind::GroupedAicLike ||
        kind == PatternKind::GroupedCycleLike || kind == PatternKind::NiceLoopLike ||
        kind == PatternKind::XChainLike || kind == PatternKind::XYChainLike) {
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i & 1) ? mask_a : mask_b;
            if (kind == PatternKind::GroupedAicLike || kind == PatternKind::NiceLoopLike || (i % 3 == 0)) {
                m |= random_extra_digit(full, m, rng);
            }
            if (kind == PatternKind::GroupedCycleLike && (i % 2 == 0)) {
                m = mask_a;
            }
            if (kind == PatternKind::XYChainLike && (i % 2 == 1)) {
                m |= random_extra_digit(full, m, rng);
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::EmptyRectangleLike) {
        const uint64_t core = random_digit_mask(topo.n, 2, rng);
        const uint64_t support = (core | random_extra_digit(full, core, rng)) & full;
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = (i < 3) ? core : support;
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    if (kind == PatternKind::RemotePairsLike) {
        const uint64_t pair = random_digit_mask(topo.n, 2, rng);
        uint64_t row_victim = (pair | random_extra_digit(full, pair, rng)) & full;
        if (std::popcount(row_victim) < 3) row_victim |= random_extra_digit(full, row_victim, rng);
        uint64_t col_victim = (pair | random_extra_digit(full, pair, rng)) & full;
        if (std::popcount(col_victim) < 3) col_victim |= random_extra_digit(full, col_victim, rng);
        for (int i = 0; i < sc.anchor_count; ++i) {
            uint64_t m = pair;
            if (i >= 4) {
                m = (i & 1) ? row_victim : col_victim;
            }
            sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
        }
        return;
    }
    // DomyĹ›lnie Chain
    for (int i = 0; i < sc.anchor_count; ++i) {
        uint64_t m = (i & 1) ? mask_a : mask_b;
        if (large_geom && (i % 3 == 2)) {
            m |= random_extra_digit(full, m, rng);
        }
        sc.allowed_masks[static_cast<size_t>(sc.anchors[static_cast<size_t>(i)])] = (m & full);
    }
}

} // namespace sudoku_hpc::pattern_forcing
