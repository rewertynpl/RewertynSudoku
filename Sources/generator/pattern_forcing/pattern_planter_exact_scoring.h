//Author copyright Marcin Matysek (Rewertyn)


#pragma once

#include "pattern_planter_policy.h"

namespace sudoku_hpc::pattern_forcing {

inline int exact_plan_mask_tightness(uint64_t mask) {
    const int bits = std::popcount(mask);
    if (bits <= 1) return 8;
    if (bits == 2) return 6;
    if (bits == 3) return 4;
    if (bits == 4) return 2;
    return 1;
}

inline int score_generic_exact_plan(
    const GenericTopology& topo,
    PatternKind kind,
    const ExactPatternTemplatePlan& plan) {
    if (!plan.valid || plan.anchor_count <= 0) return -1;

    int score = plan.anchor_count * 6;
    uint64_t row_seen = 0ULL;
    uint64_t col_seen = 0ULL;
    uint64_t box_seen = 0ULL;
    int bivalue_count = 0;
    int trivalue_count = 0;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int idx = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        const int bits = std::popcount(mask);
        score += exact_plan_mask_tightness(mask);
        if (bits <= 2) ++bivalue_count;
        else if (bits == 3) ++trivalue_count;

        const int row = topo.cell_row[static_cast<size_t>(idx)];
        const int col = topo.cell_col[static_cast<size_t>(idx)];
        const int box = topo.cell_box[static_cast<size_t>(idx)];
        row_seen |= (row < 64) ? (1ULL << row) : 0ULL;
        col_seen |= (col < 64) ? (1ULL << col) : 0ULL;
        box_seen |= (box < 64) ? (1ULL << box) : 0ULL;
    }

    score += 2 * std::popcount(row_seen);
    score += 2 * std::popcount(col_seen);
    score += std::popcount(box_seen);

    if (kind == PatternKind::ExocetLike) score += 12;
    else if (kind == PatternKind::LoopLike) score += 10;
    else if (kind == PatternKind::ForcingLike) score += 10;
    else if (kind == PatternKind::FishLike || kind == PatternKind::FrankenLike ||
             kind == PatternKind::MutantLike || kind == PatternKind::SquirmLike ||
             kind == PatternKind::SwordfishLike || kind == PatternKind::JellyfishLike ||
             kind == PatternKind::FinnedFishLike) score += 11;
    else if (kind == PatternKind::AicLike || kind == PatternKind::GroupedAicLike ||
             kind == PatternKind::GroupedCycleLike || kind == PatternKind::NiceLoopLike ||
             kind == PatternKind::XChainLike || kind == PatternKind::XYChainLike ||
             kind == PatternKind::EmptyRectangleLike || kind == PatternKind::RemotePairsLike) score += 10;
    score += 2 * bivalue_count + trivalue_count;

    return score;
}

inline int score_exocet_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    bool senior_mode) {
    int score = score_generic_exact_plan(topo, PatternKind::ExocetLike, plan);
    if (score < 0) return score;

    int base_pair_score = 0;
    int cross_target_score = 0;
    int gate_score = 0;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mi) != 2) continue;
        const int row_i = topo.cell_row[static_cast<size_t>(ai)];
        const int col_i = topo.cell_col[static_cast<size_t>(ai)];
        const int box_i = topo.cell_box[static_cast<size_t>(ai)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const uint64_t mj = plan.anchor_masks[static_cast<size_t>(j)];
            if (mi != mj || std::popcount(mj) != 2) continue;
            const int row_j = topo.cell_row[static_cast<size_t>(aj)];
            const int col_j = topo.cell_col[static_cast<size_t>(aj)];
            const int box_j = topo.cell_box[static_cast<size_t>(aj)];
            if (box_i == box_j && row_i != row_j && col_i != col_j) {
                base_pair_score += 24;
                const int t1_row = row_i;
                const int t1_col = col_j;
                const int t2_row = row_j;
                const int t2_col = col_i;
                for (int k = 0; k < plan.anchor_count; ++k) {
                    const int ak = plan.anchor_idx[static_cast<size_t>(k)];
                    const uint64_t mk = plan.anchor_masks[static_cast<size_t>(k)];
                    const int row_k = topo.cell_row[static_cast<size_t>(ak)];
                    const int col_k = topo.cell_col[static_cast<size_t>(ak)];
                    if ((row_k == t1_row && col_k == t1_col) || (row_k == t2_row && col_k == t2_col)) {
                        cross_target_score += (std::popcount(mk) >= 3) ? 8 : 4;
                    }
                    if (row_k == row_i && col_k != col_i && col_k != col_j && topo.cell_box[static_cast<size_t>(ak)] != box_i) {
                        gate_score += 3;
                    }
                    if (col_k == col_j && row_k != row_i && row_k != row_j && topo.cell_box[static_cast<size_t>(ak)] != box_j) {
                        gate_score += 3;
                    }
                }
            }
        }
    }

    score += std::min(base_pair_score, 48);
    score += std::min(cross_target_score, 24);
    score += std::min(gate_score, senior_mode ? 24 : 16);
    if (senior_mode && plan.anchor_count >= 6) score += 10;
    return score;
}

inline int score_skloop_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::LoopLike, plan);
    if (score < 0) return score;

    int rectangle_score = 0;
    int core_score = 0;
    int exit_score = 0;
    int wing_score = 0;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const int row_i = topo.cell_row[static_cast<size_t>(ai)];
        const int col_i = topo.cell_col[static_cast<size_t>(ai)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const int row_j = topo.cell_row[static_cast<size_t>(aj)];
            const int col_j = topo.cell_col[static_cast<size_t>(aj)];
            if (row_i == row_j || col_i == col_j) continue;

            int corners = 0;
            int bivalue_corners = 0;
            uint64_t core_mask = 0ULL;
            for (int k = 0; k < plan.anchor_count; ++k) {
                const int ak = plan.anchor_idx[static_cast<size_t>(k)];
                const int row_k = topo.cell_row[static_cast<size_t>(ak)];
                const int col_k = topo.cell_col[static_cast<size_t>(ak)];
                if (!((row_k == row_i || row_k == row_j) && (col_k == col_i || col_k == col_j))) continue;
                ++corners;
                const uint64_t mk = plan.anchor_masks[static_cast<size_t>(k)];
                if (std::popcount(mk) == 2) {
                    ++bivalue_corners;
                    core_mask = (core_mask == 0ULL) ? mk : (core_mask & mk);
                } else if (std::popcount(mk) >= 3) {
                    exit_score += 3;
                }
            }
            if (corners >= 4) rectangle_score += 16;
            if (bivalue_corners >= 2 && std::popcount(core_mask) >= 2) core_score += 12;
        }
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mi) != 3) continue;
        const int row_i = topo.cell_row[static_cast<size_t>(ai)];
        const int col_i = topo.cell_col[static_cast<size_t>(ai)];
        for (int j = 0; j < plan.anchor_count; ++j) {
            if (i == j) continue;
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const uint64_t mj = plan.anchor_masks[static_cast<size_t>(j)];
            if (std::popcount(mj) != 2) continue;
            const int row_j = topo.cell_row[static_cast<size_t>(aj)];
            const int col_j = topo.cell_col[static_cast<size_t>(aj)];
            if (row_i == row_j || col_i == col_j) {
                const uint64_t inter = mi & mj;
                if (std::popcount(inter) == 2) wing_score += 2;
            }
        }
    }

    score += std::min(rectangle_score, 32);
    score += std::min(core_score, 24);
    score += std::min(exit_score, 16);
    score += std::min(wing_score, 12);
    return score;
}

inline int score_msls_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::LoopLike, plan);
    if (score < 0) return score;

    int row_count[64]{};
    int col_count[64]{};
    int box_count[64]{};
    int row_box_links = 0;
    int col_box_links = 0;
    int row_col_links = 0;
    uint64_t core_mask = ~0ULL;

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int idx = plan.anchor_idx[static_cast<size_t>(i)];
        ++row_count[static_cast<size_t>(topo.cell_row[static_cast<size_t>(idx)])];
        ++col_count[static_cast<size_t>(topo.cell_col[static_cast<size_t>(idx)])];
        ++box_count[static_cast<size_t>(topo.cell_box[static_cast<size_t>(idx)])];
        core_mask &= plan.anchor_masks[static_cast<size_t>(i)];
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const int ar = topo.cell_row[static_cast<size_t>(ai)];
        const int ac = topo.cell_col[static_cast<size_t>(ai)];
        const int ab = topo.cell_box[static_cast<size_t>(ai)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const int br = topo.cell_row[static_cast<size_t>(aj)];
            const int bc = topo.cell_col[static_cast<size_t>(aj)];
            const int bb = topo.cell_box[static_cast<size_t>(aj)];
            if (ar == br && ab == bb) row_box_links += 3;
            if (ac == bc && ab == bb) col_box_links += 3;
            if (ar == br && ac == bc) row_col_links += 5;
        }
    }

    int active_rows = 0;
    int active_cols = 0;
    int active_boxes = 0;
    int compact_rows = 0;
    int compact_cols = 0;
    int compact_boxes = 0;
    for (int i = 0; i < topo.n; ++i) {
        if (row_count[i] > 0) {
            ++active_rows;
            if (row_count[i] >= 2 && row_count[i] <= std::clamp(3 + topo.n / 8, 4, 10)) compact_rows += 4;
        }
        if (col_count[i] > 0) {
            ++active_cols;
            if (col_count[i] >= 2 && col_count[i] <= std::clamp(3 + topo.n / 8, 4, 10)) compact_cols += 4;
        }
        if (box_count[i] > 0) {
            ++active_boxes;
            if (box_count[i] >= 2 && box_count[i] <= std::clamp(3 + topo.n / 8, 4, 10)) compact_boxes += 5;
        }
    }

    score += std::min(row_box_links, 36);
    score += std::min(col_box_links, 36);
    score += std::min(row_col_links, 12);
    score += std::min(compact_rows + compact_cols + compact_boxes, 36);
    if (active_rows >= 2) score += 8;
    if (active_cols >= 2) score += 8;
    if (active_boxes >= 1) score += 8;
    score += 3 * std::min(std::popcount(core_mask), 3);
    return score;
}

inline int score_forcing_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    bool dynamic_mode) {
    int score = score_generic_exact_plan(topo, PatternKind::ForcingLike, plan);
    if (score < 0) return score;

    int bivalue_nodes = 0;
    int trivalue_nodes = 0;
    int link_score = 0;
    int branch_score = 0;
    int degree_hist[64]{};

    for (int i = 0; i < plan.anchor_count; ++i) {
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        const int bits = std::popcount(mi);
        if (bits == 2) ++bivalue_nodes;
        else if (bits == 3) ++trivalue_nodes;
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        int degree = 0;
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const uint64_t mj = plan.anchor_masks[static_cast<size_t>(j)];
            const bool sees =
                topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)] ||
                topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)];
            if (!sees) continue;
            const int overlap = std::popcount(mi & mj);
            if (overlap <= 0) continue;
            link_score += (overlap == 1) ? 4 : 2;
            ++degree;
            ++degree_hist[static_cast<size_t>(j)];
        }
        degree_hist[static_cast<size_t>(i)] += degree;
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const int deg = degree_hist[static_cast<size_t>(i)];
        if (deg >= 2) branch_score += 3;
        if (deg >= 3) branch_score += 4;
    }

    score += 5 * bivalue_nodes + 3 * trivalue_nodes;
    score += std::min(link_score, dynamic_mode ? 36 : 28);
    score += std::min(branch_score, dynamic_mode ? 28 : 18);
    if (dynamic_mode && plan.anchor_count >= 6) score += 8;
    return score;
}

inline int score_medusa_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::ColorLike, plan);
    if (score < 0) return score;
    int bivalue = 0;
    int peer_links = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const uint64_t mi = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mi) == 2) ++bivalue;
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int ai = plan.anchor_idx[static_cast<size_t>(i)];
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            const bool sees =
                topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)] ||
                topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)];
            if (sees && std::popcount(mi & plan.anchor_masks[static_cast<size_t>(j)]) >= 1) {
                peer_links += 3;
            }
        }
    }
    score += 5 * bivalue + std::min(peer_links, 30);
    return score;
}

inline int score_death_blossom_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::PetalLike, plan);
    if (score < 0) return score;
    int pivot_idx = -1;
    int pivot_bits = -1;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int bits = std::popcount(plan.anchor_masks[static_cast<size_t>(i)]);
        if (bits > pivot_bits) {
            pivot_bits = bits;
            pivot_idx = i;
        }
    }
    if (pivot_idx < 0) return score;
    const int pivot = plan.anchor_idx[static_cast<size_t>(pivot_idx)];
    const uint64_t pivot_mask = plan.anchor_masks[static_cast<size_t>(pivot_idx)];
    int petals = 0;
    int shared = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        if (i == pivot_idx) continue;
        const int idx = plan.anchor_idx[static_cast<size_t>(i)];
        const bool sees =
            topo.cell_row[static_cast<size_t>(pivot)] == topo.cell_row[static_cast<size_t>(idx)] ||
            topo.cell_col[static_cast<size_t>(pivot)] == topo.cell_col[static_cast<size_t>(idx)] ||
            topo.cell_box[static_cast<size_t>(pivot)] == topo.cell_box[static_cast<size_t>(idx)];
        if (!sees) continue;
        ++petals;
        const int overlap = std::popcount(pivot_mask & plan.anchor_masks[static_cast<size_t>(i)]);
        if (overlap >= 1) shared += 4;
    }
    score += 6 * petals + std::min(shared, 24);
    return score;
}

inline int score_suedecoq_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::IntersectionLike, plan);
    if (score < 0) return score;
    int box_cluster = 0;
    int row_col_spread = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            if (topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)]) {
                ++box_cluster;
            }
            if (topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)]) {
                ++row_col_spread;
            }
        }
    }
    score += std::min(box_cluster * 2, 24) + std::min(row_col_spread * 2, 24);
    return score;
}

inline int score_kraken_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_generic_exact_plan(topo, PatternKind::FishLike, plan);
    if (score < 0) return score;
    uint64_t shared = ~0ULL;
    for (int i = 0; i < plan.anchor_count; ++i) {
        shared &= plan.anchor_masks[static_cast<size_t>(i)];
    }
    int line_pairs = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            if (topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)]) {
                ++line_pairs;
            }
        }
    }
    score += 8 * std::min(std::popcount(shared), 2) + std::min(line_pairs * 2, 28);
    return score;
}

inline int score_fish_variant_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    PatternKind kind,
    int box_weight,
    int line_cap,
    int box_cap) {
    int score = score_generic_exact_plan(topo, kind, plan);
    if (score < 0) return score;
    int line_pairs = 0;
    int box_pairs = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            if (topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)]) {
                ++line_pairs;
            }
            if (topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)]) {
                ++box_pairs;
            }
        }
    }
    score += std::min(line_pairs * 2, line_cap) + std::min(box_pairs * box_weight, box_cap);
    return score;
}

inline int score_aic_slot_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    PatternKind kind,
    int box_bonus,
    int line_bonus_cap) {
    int score = score_generic_exact_plan(topo, kind, plan);
    if (score < 0) return score;
    int line_pairs = 0;
    int box_pairs = 0;
    int overlaps = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int ai = plan.anchor_idx[static_cast<size_t>(i)];
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            if (topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)]) {
                ++line_pairs;
            }
            if (topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)]) {
                ++box_pairs;
            }
            overlaps += std::popcount(plan.anchor_masks[static_cast<size_t>(i)] &
                                      plan.anchor_masks[static_cast<size_t>(j)]);
        }
    }
    score += std::min(line_pairs * 2, line_bonus_cap);
    score += std::min(box_pairs * box_bonus + overlaps, 28);
    return score;
}

inline int score_als_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    bool aic_mode) {
    int score = score_generic_exact_plan(topo, PatternKind::AlsLike, plan);
    if (score < 0) return score;
    int overlaps = 0;
    int tri_masks = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        if (std::popcount(plan.anchor_masks[static_cast<size_t>(i)]) >= 3) ++tri_masks;
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            overlaps += std::popcount(plan.anchor_masks[static_cast<size_t>(i)] & plan.anchor_masks[static_cast<size_t>(j)]);
        }
    }
    score += std::min(4 * tri_masks + overlaps, aic_mode ? 34 : 28);
    return score;
}

inline int score_als_xz_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_als_exact_plan(topo, plan, false) + 6;
    if (score < 0) return score;

    int row_hist[64]{};
    int col_hist[64]{};
    int witness_score = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int idx = plan.anchor_idx[static_cast<size_t>(i)];
        ++row_hist[static_cast<size_t>(topo.cell_row[static_cast<size_t>(idx)])];
        ++col_hist[static_cast<size_t>(topo.cell_col[static_cast<size_t>(idx)])];
        if (std::popcount(plan.anchor_masks[static_cast<size_t>(i)]) >= 2) {
            witness_score += 2;
        }
    }

    int row_pairs = 0;
    int col_pairs = 0;
    for (int i = 0; i < topo.n; ++i) {
        if (row_hist[i] >= 2) row_pairs += 4;
        if (col_hist[i] >= 2) col_pairs += 5;
    }
    score += std::min(row_pairs, 12);
    score += std::min(col_pairs, 16);
    score += std::min(witness_score, 10);
    return score;
}

inline int score_wxyz_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_als_exact_plan(topo, plan, false) + 8;
    if (score < 0) return score;

    int box_cluster = 0;
    int witness_score = 0;
    int tri_score = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        const int bits = std::popcount(plan.anchor_masks[static_cast<size_t>(i)]);
        if (bits == 3) tri_score += 3;
        if (bits == 2) witness_score += 6;
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            if (topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)]) {
                box_cluster += 3;
            }
        }
    }

    score += std::min(box_cluster, 30);
    score += std::min(witness_score, 12);
    score += std::min(tri_score, 12);
    return score;
}

inline int score_exclusion_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan,
    bool triple_mode) {
    int score = score_generic_exact_plan(topo, PatternKind::ExclusionLike, plan);
    if (score < 0) return score;
    int clique = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const int ai = plan.anchor_idx[static_cast<size_t>(i)];
        for (int j = i + 1; j < plan.anchor_count; ++j) {
            const int aj = plan.anchor_idx[static_cast<size_t>(j)];
            if (topo.cell_row[static_cast<size_t>(ai)] == topo.cell_row[static_cast<size_t>(aj)] ||
                topo.cell_col[static_cast<size_t>(ai)] == topo.cell_col[static_cast<size_t>(aj)] ||
                topo.cell_box[static_cast<size_t>(ai)] == topo.cell_box[static_cast<size_t>(aj)]) {
                clique += 3;
            }
        }
    }
    score += std::min(clique, triple_mode ? 26 : 18);
    return score;
}

inline int score_remote_pairs_exact_plan(
    const GenericTopology& topo,
    const ExactPatternTemplatePlan& plan) {
    int score = score_aic_slot_exact_plan(topo, plan, PatternKind::RemotePairsLike, 4, 38);
    if (score < 0) return score;

    uint64_t masks[16]{};
    int freq[16]{};
    int used = 0;
    int witness = 0;
    for (int i = 0; i < plan.anchor_count; ++i) {
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mask) == 2) {
            int slot = -1;
            for (int j = 0; j < used; ++j) {
                if (masks[j] == mask) {
                    slot = j;
                    break;
                }
            }
            if (slot < 0 && used < 16) {
                slot = used;
                masks[used] = mask;
                freq[used] = 0;
                ++used;
            }
            if (slot >= 0) ++freq[slot];
        }
    }

    int best = 0;
    uint64_t best_mask = 0ULL;
    for (int i = 0; i < used; ++i) {
        if (freq[i] > best) {
            best = freq[i];
            best_mask = masks[i];
        }
    }

    for (int i = 0; i < plan.anchor_count; ++i) {
        const uint64_t mask = plan.anchor_masks[static_cast<size_t>(i)];
        if (std::popcount(mask) >= 3 && best_mask != 0ULL && (mask & best_mask) == best_mask) {
            witness += 5;
        }
    }

    score += std::min(best * 8, 32);
    score += std::min(witness, 12);
    return score;
}

inline bool exact_kind_matches_required_strategy(
    RequiredStrategy required_strategy,
    PatternKind kind) {
    switch (required_strategy) {
    case RequiredStrategy::Exocet:
    case RequiredStrategy::SeniorExocet:
        return kind == PatternKind::ExocetLike;
    case RequiredStrategy::ALSXYWing:
    case RequiredStrategy::ALSXZ:
    case RequiredStrategy::ALSChain:
    case RequiredStrategy::ALSAIC:
    case RequiredStrategy::WXYZWing:
        return kind == PatternKind::AlsLike;
    case RequiredStrategy::Medusa3D:
    case RequiredStrategy::SimpleColoring:
        return kind == PatternKind::ColorLike;
    case RequiredStrategy::DeathBlossom:
        return kind == PatternKind::PetalLike;
    case RequiredStrategy::SueDeCoq:
        return kind == PatternKind::IntersectionLike;
    case RequiredStrategy::KrakenFish:
        return kind == PatternKind::FishLike;
    case RequiredStrategy::FrankenFish:
        return kind == PatternKind::FrankenLike;
    case RequiredStrategy::MutantFish:
        return kind == PatternKind::MutantLike;
    case RequiredStrategy::Squirmbag:
        return kind == PatternKind::SquirmLike;
    case RequiredStrategy::AIC:
        return kind == PatternKind::AicLike;
    case RequiredStrategy::GroupedAIC:
        return kind == PatternKind::GroupedAicLike;
    case RequiredStrategy::GroupedXCycle:
        return kind == PatternKind::GroupedCycleLike;
    case RequiredStrategy::ContinuousNiceLoop:
        return kind == PatternKind::NiceLoopLike;
    case RequiredStrategy::XChain:
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
        return kind == PatternKind::XChainLike;
    case RequiredStrategy::XYChain:
        return kind == PatternKind::XYChainLike;
    case RequiredStrategy::MSLS:
    case RequiredStrategy::PatternOverlayMethod:
        return kind == PatternKind::LoopLike;
    case RequiredStrategy::ForcingChains:
    case RequiredStrategy::DynamicForcingChains:
        return kind == PatternKind::ForcingLike;
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::XWing:
        return kind == PatternKind::SwordfishLike;
    case RequiredStrategy::Jellyfish:
        return kind == PatternKind::JellyfishLike;
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
        return kind == PatternKind::FinnedFishLike;
    case RequiredStrategy::EmptyRectangle:
        return kind == PatternKind::EmptyRectangleLike;
    case RequiredStrategy::RemotePairs:
        return kind == PatternKind::RemotePairsLike;
    default:
        return true;
    }
}

inline int score_exact_plan(
    const GenericTopology& topo,
    RequiredStrategy required_strategy,
    PatternKind kind,
    const ExactPatternTemplatePlan& plan) {
    if (!exact_kind_matches_required_strategy(required_strategy, kind)) {
        return -1;
    }
    switch (required_strategy) {
    case RequiredStrategy::Exocet:
        return score_exocet_exact_plan(topo, plan, false);
    case RequiredStrategy::SeniorExocet:
        return score_exocet_exact_plan(topo, plan, true);
    case RequiredStrategy::SKLoop:
        return score_skloop_exact_plan(topo, plan);
    case RequiredStrategy::MSLS:
        return score_msls_exact_plan(topo, plan);
    case RequiredStrategy::Medusa3D:
        return score_medusa_exact_plan(topo, plan);
    case RequiredStrategy::DeathBlossom:
        return score_death_blossom_exact_plan(topo, plan);
    case RequiredStrategy::SueDeCoq:
        return score_suedecoq_exact_plan(topo, plan);
    case RequiredStrategy::KrakenFish:
        return score_kraken_exact_plan(topo, plan);
    case RequiredStrategy::FrankenFish:
        return score_fish_variant_exact_plan(topo, plan, PatternKind::FrankenLike, 4, 24, 34);
    case RequiredStrategy::MutantFish:
        return score_fish_variant_exact_plan(topo, plan, PatternKind::MutantLike, 5, 24, 38);
    case RequiredStrategy::Squirmbag:
        return score_fish_variant_exact_plan(topo, plan, PatternKind::SquirmLike, 3, 36, 24);
    case RequiredStrategy::Swordfish:
    case RequiredStrategy::XWing:
        return score_fish_variant_exact_plan(topo, plan, PatternKind::SwordfishLike, 2, 30, 20);
    case RequiredStrategy::Jellyfish:
        return score_fish_variant_exact_plan(topo, plan, PatternKind::JellyfishLike, 2, 34, 22);
    case RequiredStrategy::FinnedXWingSashimi:
    case RequiredStrategy::FinnedSwordfishJellyfish:
        return score_fish_variant_exact_plan(topo, plan, PatternKind::FinnedFishLike, 4, 30, 30);
    case RequiredStrategy::ALSXYWing:
        return score_als_exact_plan(topo, plan, false);
    case RequiredStrategy::ALSXZ:
        return score_als_xz_exact_plan(topo, plan);
    case RequiredStrategy::WXYZWing:
        return score_wxyz_exact_plan(topo, plan);
    case RequiredStrategy::ALSChain:
        return score_als_exact_plan(topo, plan, false) + 4;
    case RequiredStrategy::ALSAIC:
        return score_als_exact_plan(topo, plan, true);
    case RequiredStrategy::AlignedPairExclusion:
        return score_exclusion_exact_plan(topo, plan, false);
    case RequiredStrategy::AlignedTripleExclusion:
        return score_exclusion_exact_plan(topo, plan, true);
    case RequiredStrategy::AIC:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::AicLike, 2, 26);
    case RequiredStrategy::GroupedAIC:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::GroupedAicLike, 4, 28);
    case RequiredStrategy::GroupedXCycle:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::GroupedCycleLike, 3, 30);
    case RequiredStrategy::ContinuousNiceLoop:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::NiceLoopLike, 3, 32);
    case RequiredStrategy::XChain:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::XChainLike, 3, 36);
    case RequiredStrategy::Skyscraper:
    case RequiredStrategy::TwoStringKite:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::XChainLike, 2, 30);
    case RequiredStrategy::EmptyRectangle:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::EmptyRectangleLike, 4, 40);
    case RequiredStrategy::XYChain:
        return score_aic_slot_exact_plan(topo, plan, PatternKind::XYChainLike, 2, 30);
    case RequiredStrategy::RemotePairs:
        return score_remote_pairs_exact_plan(topo, plan);
    case RequiredStrategy::SimpleColoring:
        return score_medusa_exact_plan(topo, plan);
    case RequiredStrategy::ForcingChains:
        return score_forcing_exact_plan(topo, plan, false);
    case RequiredStrategy::DynamicForcingChains:
        return score_forcing_exact_plan(topo, plan, true);
    default:
        return score_generic_exact_plan(topo, kind, plan);
    }
}

} // namespace sudoku_hpc::pattern_forcing
