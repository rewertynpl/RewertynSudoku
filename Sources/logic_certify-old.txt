//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "geometry.h"
#include "strategieSource/strategylevels1_runtime.h"

namespace sudoku_hpc {

enum class RuntimeStrategyId : uint8_t {
    NakedSingle = 0,
    HiddenSingle = 1,
    PointingPairs = 2,
    BoxLineReduction = 3,
    NakedPair = 4,
    HiddenPair = 5,
    NakedTriple = 6,
    HiddenTriple = 7,
    NakedQuad = 8,
    HiddenQuad = 9,
    XWing = 10,
    YWing = 11,
    Skyscraper = 12,
    TwoStringKite = 13,
    EmptyRectangle = 14,
    RemotePairs = 15,
    Swordfish = 16,
    FinnedXWingSashimi = 17,
    SimpleColoring = 18,
    BUGPlusOne = 19,
    UniqueRectangle = 20,
    XYZWing = 21,
    WWing = 22,
    Jellyfish = 23,
    XChain = 24,
    XYChain = 25,
    WXYZWing = 26,
    FinnedSwordfishJellyfish = 27,
    ALSXZ = 28,
    UniqueLoop = 29,
    AvoidableRectangle = 30,
    BivalueOddagon = 31,
    Medusa3D = 32,
    AIC = 33,
    GroupedAIC = 34,
    GroupedXCycle = 35,
    ContinuousNiceLoop = 36,
    ALSXYWing = 37,
    ALSChain = 38,
    SueDeCoq = 39,
    DeathBlossom = 40,
    FrankenFish = 41,
    MutantFish = 42,
    KrakenFish = 43,
    MSLS = 44,
    Exocet = 45,
    SeniorExocet = 46,
    SKLoop = 47,
    PatternOverlayMethod = 48,
    ForcingChains = 49
};

struct RuntimeStrategyMeta {
    RuntimeStrategyId id{};
    const char* name = "";
    int level = 1;
    bool runtime_ready = false;
};

inline const std::vector<RuntimeStrategyMeta>& runtime_strategy_registry() {
    static const std::vector<RuntimeStrategyMeta> registry = {
        {RuntimeStrategyId::NakedSingle, "NakedSingle", 1, true},
        {RuntimeStrategyId::HiddenSingle, "HiddenSingle", 1, true},
        {RuntimeStrategyId::PointingPairs, "PointingPairs", 2, true},
        {RuntimeStrategyId::BoxLineReduction, "BoxLineReduction", 2, true},
        {RuntimeStrategyId::NakedPair, "NakedPair", 3, true},
        {RuntimeStrategyId::HiddenPair, "HiddenPair", 3, true},
        {RuntimeStrategyId::NakedTriple, "NakedTriple", 3, true},
        {RuntimeStrategyId::HiddenTriple, "HiddenTriple", 3, true},
        {RuntimeStrategyId::NakedQuad, "NakedQuad", 4, true},
        {RuntimeStrategyId::HiddenQuad, "HiddenQuad", 4, true},
        {RuntimeStrategyId::XWing, "XWing", 4, true},
        {RuntimeStrategyId::YWing, "YWing", 4, true},
        {RuntimeStrategyId::Skyscraper, "Skyscraper", 4, true},
        {RuntimeStrategyId::TwoStringKite, "TwoStringKite", 4, true},
        {RuntimeStrategyId::EmptyRectangle, "EmptyRectangle", 4, true},
        {RuntimeStrategyId::RemotePairs, "RemotePairs", 4, true},
        {RuntimeStrategyId::Swordfish, "Swordfish", 5, true},
        {RuntimeStrategyId::FinnedXWingSashimi, "FinnedXWingSashimi", 5, true},
        {RuntimeStrategyId::SimpleColoring, "SimpleColoring", 5, true},
        {RuntimeStrategyId::BUGPlusOne, "BUGPlusOne", 5, true},
        {RuntimeStrategyId::UniqueRectangle, "UniqueRectangle", 5, true},
        {RuntimeStrategyId::XYZWing, "XYZWing", 5, true},
        {RuntimeStrategyId::WWing, "WWing", 5, true},
        {RuntimeStrategyId::Jellyfish, "Jellyfish", 6, true},
        {RuntimeStrategyId::XChain, "XChain", 6, true},
        {RuntimeStrategyId::XYChain, "XYChain", 6, true},
        {RuntimeStrategyId::WXYZWing, "WXYZWing", 6, true},
        {RuntimeStrategyId::FinnedSwordfishJellyfish, "FinnedSwordfishJellyfish", 6, true},
        {RuntimeStrategyId::ALSXZ, "ALSXZ", 6, true},
        {RuntimeStrategyId::UniqueLoop, "UniqueLoop", 6, true},
        {RuntimeStrategyId::AvoidableRectangle, "AvoidableRectangle", 6, true},
        {RuntimeStrategyId::BivalueOddagon, "BivalueOddagon", 6, true},
        {RuntimeStrategyId::Medusa3D, "Medusa3D", 7, true},
        {RuntimeStrategyId::AIC, "AIC", 7, true},
        {RuntimeStrategyId::GroupedAIC, "GroupedAIC", 7, true},
        {RuntimeStrategyId::GroupedXCycle, "GroupedXCycle", 7, true},
        {RuntimeStrategyId::ContinuousNiceLoop, "ContinuousNiceLoop", 7, true},
        {RuntimeStrategyId::ALSXYWing, "ALSXYWing", 7, true},
        {RuntimeStrategyId::ALSChain, "ALSChain", 7, true},
        {RuntimeStrategyId::SueDeCoq, "SueDeCoq", 7, true},
        {RuntimeStrategyId::DeathBlossom, "DeathBlossom", 7, true},
        {RuntimeStrategyId::FrankenFish, "FrankenFish", 7, true},
        {RuntimeStrategyId::MutantFish, "MutantFish", 7, true},
        {RuntimeStrategyId::KrakenFish, "KrakenFish", 7, true},
        {RuntimeStrategyId::MSLS, "MSLS", 8, true},
        {RuntimeStrategyId::Exocet, "Exocet", 8, true},
        {RuntimeStrategyId::SeniorExocet, "SeniorExocet", 8, true},
        {RuntimeStrategyId::SKLoop, "SKLoop", 8, true},
        {RuntimeStrategyId::PatternOverlayMethod, "PatternOverlayMethod", 8, true},
        {RuntimeStrategyId::ForcingChains, "ForcingChains", 8, true},
    };
    return registry;
}

struct StrategyStats {
    uint64_t use_count = 0;
    uint64_t hit_count = 0;
    uint64_t placements = 0;
    uint64_t elapsed_ns = 0;
};

struct GenericLogicCertifyResult {
    bool solved = false;
    bool timed_out = false;
    bool used_naked_single = false;
    bool used_hidden_single = false;
    bool used_pointing_pairs = false;
    bool used_box_line = false;
    bool used_naked_pair = false;
    bool used_hidden_pair = false;
    bool used_naked_triple = false;
    bool used_hidden_triple = false;
    bool used_naked_quad = false;
    bool used_hidden_quad = false;
    bool used_x_wing = false;
    bool used_y_wing = false;
    bool used_skyscraper = false;
    bool used_two_string_kite = false;
    bool used_empty_rectangle = false;
    bool used_remote_pairs = false;
    bool used_swordfish = false;
    bool used_finned_x_wing_sashimi = false;
    bool used_simple_coloring = false;
    bool used_bug_plus_one = false;
    bool used_unique_rectangle = false;
    bool used_xyz_wing = false;
    bool used_w_wing = false;
    bool used_jellyfish = false;
    bool used_x_chain = false;
    bool used_xy_chain = false;
    bool used_wxyz_wing = false;
    bool used_finned_swordfish_jellyfish = false;
    bool used_als_xz = false;
    bool used_unique_loop = false;
    bool used_avoidable_rectangle = false;
    bool used_bivalue_oddagon = false;
    bool used_medusa_3d = false;
    bool used_aic = false;
    bool used_grouped_aic = false;
    bool used_grouped_x_cycle = false;
    bool used_continuous_nice_loop = false;
    bool used_als_xy_wing = false;
    bool used_als_chain = false;
    bool used_sue_de_coq = false;
    bool used_death_blossom = false;
    bool used_franken_fish = false;
    bool used_mutant_fish = false;
    bool used_kraken_fish = false;
    bool used_msls = false;
    bool used_exocet = false;
    bool used_senior_exocet = false;
    bool used_sk_loop = false;
    bool used_pattern_overlay_method = false;
    bool used_forcing_chains = false;
    bool naked_single_scanned = false;
    bool hidden_single_scanned = false;
    int steps = 0;
    std::vector<uint16_t> solved_grid;
    std::array<StrategyStats, 50> strategy_stats{};
};

struct GenericLogicCertify {
    enum class ApplyResult : uint8_t { NoProgress = 0, Progress = 1, Contradiction = 2 };
    enum StrategySlot : size_t {
        SlotNakedSingle = 0,
        SlotHiddenSingle = 1,
        SlotPointingPairs = 2,
        SlotBoxLineReduction = 3,
        SlotNakedPair = 4,
        SlotHiddenPair = 5,
        SlotNakedTriple = 6,
        SlotHiddenTriple = 7,
        SlotNakedQuad = 8,
        SlotHiddenQuad = 9,
        SlotXWing = 10,
        SlotYWing = 11,
        SlotSkyscraper = 12,
        SlotTwoStringKite = 13,
        SlotEmptyRectangle = 14,
        SlotRemotePairs = 15,
        SlotSwordfish = 16,
        SlotFinnedXWingSashimi = 17,
        SlotSimpleColoring = 18,
        SlotBUGPlusOne = 19,
        SlotUniqueRectangle = 20,
        SlotXYZWing = 21,
        SlotWWing = 22,
        SlotJellyfish = 23,
        SlotXChain = 24,
        SlotXYChain = 25,
        SlotWXYZWing = 26,
        SlotFinnedSwordfishJellyfish = 27,
        SlotALSXZ = 28,
        SlotUniqueLoop = 29,
        SlotAvoidableRectangle = 30,
        SlotBivalueOddagon = 31,
        SlotMedusa3D = 32,
        SlotAIC = 33,
        SlotGroupedAIC = 34,
        SlotGroupedXCycle = 35,
        SlotContinuousNiceLoop = 36,
        SlotALSXYWing = 37,
        SlotALSChain = 38,
        SlotSueDeCoq = 39,
        SlotDeathBlossom = 40,
        SlotFrankenFish = 41,
        SlotMutantFish = 42,
        SlotKrakenFish = 43,
        SlotMSLS = 44,
        SlotExocet = 45,
        SlotSeniorExocet = 46,
        SlotSKLoop = 47,
        SlotPatternOverlayMethod = 48,
        SlotForcingChains = 49
    };

private:
    static uint64_t now_ns() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    struct CandidateState {
        GenericBoard* board = nullptr;
        const GenericTopology* topo = nullptr;
        std::vector<uint64_t> cands;

        bool init(GenericBoard& b, const GenericTopology& t) {
            board = &b;
            topo = &t;
            cands.assign(static_cast<size_t>(t.nn), 0ULL);
            for (int idx = 0; idx < t.nn; ++idx) {
                if (b.values[static_cast<size_t>(idx)] != 0) continue;
                const uint64_t m = b.candidate_mask_for_idx(idx);
                if (m == 0ULL) return false;
                cands[static_cast<size_t>(idx)] = m;
            }
            return true;
        }

        bool place(int idx, int d) {
            if (board->values[static_cast<size_t>(idx)] != 0) {
                return board->values[static_cast<size_t>(idx)] == static_cast<uint16_t>(d);
            }
            const uint64_t bit = (1ULL << (d - 1));
            if ((cands[static_cast<size_t>(idx)] & bit) == 0ULL) return false;
            if (!board->can_place(idx, d)) return false;
            board->place(idx, d);
            cands[static_cast<size_t>(idx)] = 0ULL;
            const int p0 = topo->peer_offsets[static_cast<size_t>(idx)];
            const int p1 = topo->peer_offsets[static_cast<size_t>(idx + 1)];
            for (int p = p0; p < p1; ++p) {
                const int peer = topo->peers_flat[static_cast<size_t>(p)];
                if (board->values[static_cast<size_t>(peer)] != 0) continue;
                uint64_t& pm = cands[static_cast<size_t>(peer)];
                if ((pm & bit) == 0ULL) continue;
                pm &= ~bit;
                if (pm == 0ULL) return false;
            }
            return true;
        }

        ApplyResult eliminate(int idx, uint64_t rm) {
            if (rm == 0ULL || board->values[static_cast<size_t>(idx)] != 0) return ApplyResult::NoProgress;
            uint64_t& m = cands[static_cast<size_t>(idx)];
            if ((m & rm) == 0ULL) return ApplyResult::NoProgress;
            m &= ~rm;
            if (m == 0ULL) return ApplyResult::Contradiction;
            return ApplyResult::Progress;
        }

        ApplyResult keep_only(int idx, uint64_t allowed) {
            if (board->values[static_cast<size_t>(idx)] != 0) return ApplyResult::NoProgress;
            uint64_t& m = cands[static_cast<size_t>(idx)];
            const uint64_t nm = m & allowed;
            if (nm == m) return ApplyResult::NoProgress;
            if (nm == 0ULL) return ApplyResult::Contradiction;
            m = nm;
            return ApplyResult::Progress;
        }
    };

    static int popcnt(uint64_t x) { return std::popcount(x); }
    static int single_digit(uint64_t m) {
        if (m == 0ULL || (m & (m - 1ULL)) != 0ULL) return 0;
        return static_cast<int>(std::countr_zero(m)) + 1;
    }
    static bool is_peer(const CandidateState& st, int a, int b) {
        if (a == b) return false;
        const auto& topo = *st.topo;
        return topo.cell_row[static_cast<size_t>(a)] == topo.cell_row[static_cast<size_t>(b)] ||
               topo.cell_col[static_cast<size_t>(a)] == topo.cell_col[static_cast<size_t>(b)] ||
               topo.cell_box[static_cast<size_t>(a)] == topo.cell_box[static_cast<size_t>(b)];
    }
    static uint64_t lsb(uint64_t x) { return x & (~x + 1ULL); }
    static int bit_to_index(uint64_t bit) { return static_cast<int>(std::countr_zero(bit)); }
    static bool decode_two_bits(uint64_t m, uint64_t& a, uint64_t& b) {
        if (popcnt(m) != 2) return false;
        a = lsb(m);
        b = m ^ a;
        return true;
    }

    static ApplyResult apply_naked_single(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            const uint64_t m = st.cands[static_cast<size_t>(idx)];
            if (m == 0ULL) { s.elapsed_ns += now_ns() - t0; return ApplyResult::Contradiction; }
            const int d = single_digit(m);
            if (d == 0) continue;
            if (!st.place(idx, d)) { s.elapsed_ns += now_ns() - t0; return ApplyResult::Contradiction; }
            ++s.hit_count; ++s.placements; ++r.steps; r.used_naked_single = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_hidden_single(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
            const int p0 = st.topo->house_offsets[h], p1 = st.topo->house_offsets[h + 1];
            for (int d = 1; d <= n; ++d) {
                const uint64_t bit = (1ULL << (d - 1));
                int pos = -1, cnt = 0;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                    if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                    pos = idx; ++cnt; if (cnt > 1) break;
                }
                if (cnt != 1) continue;
                if (!st.place(pos, d)) { s.elapsed_ns += now_ns() - t0; return ApplyResult::Contradiction; }
                ++s.hit_count; ++s.placements; ++r.steps; r.used_hidden_single = true;
                s.elapsed_ns += now_ns() - t0;
                return ApplyResult::Progress;
            }
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_house_subset(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r, int subset, bool hidden) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        bool progress = false;
        std::vector<uint64_t> pos;
        if (hidden) {
            pos.assign(static_cast<size_t>(n), 0ULL);
        }

        for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
            const int p0 = st.topo->house_offsets[h], p1 = st.topo->house_offsets[h + 1];
            if (!hidden) {
                std::vector<int> cells;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                    const int bits = popcnt(st.cands[static_cast<size_t>(idx)]);
                    if (bits >= 2 && bits <= subset) cells.push_back(idx);
                }
                const int m = static_cast<int>(cells.size());
                if (m < subset) continue;
                auto apply_naked_union = [&](int a, int b, int c, int d) -> ApplyResult {
                    uint64_t um = st.cands[static_cast<size_t>(cells[static_cast<size_t>(a)])] |
                                  st.cands[static_cast<size_t>(cells[static_cast<size_t>(b)])];
                    if (c >= 0) um |= st.cands[static_cast<size_t>(cells[static_cast<size_t>(c)])];
                    if (d >= 0) um |= st.cands[static_cast<size_t>(cells[static_cast<size_t>(d)])];
                    if (popcnt(um) != subset) return ApplyResult::NoProgress;
                    for (int p = p0; p < p1; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (idx == cells[static_cast<size_t>(a)] || idx == cells[static_cast<size_t>(b)] ||
                            (c >= 0 && idx == cells[static_cast<size_t>(c)]) ||
                            (d >= 0 && idx == cells[static_cast<size_t>(d)])) {
                            continue;
                        }
                        const ApplyResult er = st.eliminate(idx, um);
                        if (er == ApplyResult::Contradiction) return er;
                        progress = progress || (er == ApplyResult::Progress);
                    }
                    return ApplyResult::NoProgress;
                };

                for (int i = 0; i < m; ++i) {
                    for (int j = i + 1; j < m; ++j) {
                        if (subset == 2) {
                            const ApplyResult rr = apply_naked_union(i, j, -1, -1);
                            if (rr == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return rr; }
                            continue;
                        }
                        for (int k = j + 1; k < m; ++k) {
                            if (subset == 3) {
                                const ApplyResult rr = apply_naked_union(i, j, k, -1);
                                if (rr == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return rr; }
                                continue;
                            }
                            for (int l = k + 1; l < m; ++l) {
                                const ApplyResult rr = apply_naked_union(i, j, k, l);
                                if (rr == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return rr; }
                            }
                        }
                    }
                }
            } else {
                std::fill(pos.begin(), pos.end(), 0ULL);
                for (int d = 1; d <= n; ++d) {
                    const uint64_t bit = (1ULL << (d - 1));
                    uint64_t bits = 0ULL;
                    for (int p = p0; p < p1; ++p) {
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                        if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                        if ((st.cands[static_cast<size_t>(idx)] & bit) != 0ULL) bits |= (1ULL << (p - p0));
                    }
                    pos[static_cast<size_t>(d - 1)] = bits;
                }
                std::vector<int> active_digits;
                active_digits.reserve(static_cast<size_t>(n));
                for (int d = 1; d <= n; ++d) {
                    const int cnt = popcnt(pos[static_cast<size_t>(d - 1)]);
                    if (cnt >= 1 && cnt <= subset) {
                        active_digits.push_back(d);
                    }
                }
                if (static_cast<int>(active_digits.size()) < subset) {
                    continue;
                }
                auto apply_hidden_union = [&](int d1, int d2, int d3, int d4) -> ApplyResult {
                    uint64_t up = pos[static_cast<size_t>(d1 - 1)] | pos[static_cast<size_t>(d2 - 1)];
                    uint64_t allowed = (1ULL << (d1 - 1)) | (1ULL << (d2 - 1));
                    if (d3 > 0) {
                        up |= pos[static_cast<size_t>(d3 - 1)];
                        allowed |= (1ULL << (d3 - 1));
                    }
                    if (d4 > 0) {
                        up |= pos[static_cast<size_t>(d4 - 1)];
                        allowed |= (1ULL << (d4 - 1));
                    }
                    if (popcnt(up) != subset) return ApplyResult::NoProgress;
                    for (uint64_t w = up; w != 0ULL; w &= (w - 1ULL)) {
                        const int b = static_cast<int>(std::countr_zero(w));
                        const int idx = st.topo->houses_flat[static_cast<size_t>(p0 + b)];
                        const ApplyResult rr = st.keep_only(idx, allowed);
                        if (rr == ApplyResult::Contradiction) return rr;
                        progress = progress || (rr == ApplyResult::Progress);
                    }
                    return ApplyResult::NoProgress;
                };
                const int ad = static_cast<int>(active_digits.size());
                for (int i = 0; i < ad; ++i) {
                    const int d1 = active_digits[static_cast<size_t>(i)];
                    for (int j = i + 1; j < ad; ++j) {
                        const int d2 = active_digits[static_cast<size_t>(j)];
                        if (subset == 2) {
                            const ApplyResult rr = apply_hidden_union(d1, d2, -1, -1);
                            if (rr == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return rr; }
                            continue;
                        }
                        for (int k = j + 1; k < ad; ++k) {
                            const int d3 = active_digits[static_cast<size_t>(k)];
                            if (subset == 3) {
                                const ApplyResult rr = apply_hidden_union(d1, d2, d3, -1);
                                if (rr == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return rr; }
                                continue;
                            }
                            for (int l = k + 1; l < ad; ++l) {
                                const int d4 = active_digits[static_cast<size_t>(l)];
                                const ApplyResult rr = apply_hidden_union(d1, d2, d3, d4);
                                if (rr == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return rr; }
                            }
                        }
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            if (!hidden && subset == 2) r.used_naked_pair = true;
            if (!hidden && subset == 3) r.used_naked_triple = true;
            if (!hidden && subset == 4) r.used_naked_quad = true;
            if (hidden && subset == 2) r.used_hidden_pair = true;
            if (hidden && subset == 3) r.used_hidden_triple = true;
            if (hidden && subset == 4) r.used_hidden_quad = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_pointing_and_boxline(CandidateState& st, StrategyStats& sp, StrategyStats& sb, GenericLogicCertifyResult& r) {
        const uint64_t t0p = now_ns();
        ++sp.use_count;
        const int n = st.topo->n;
        bool p_progress = false;
        for (int brg = 0; brg < st.topo->box_rows_count; ++brg) {
            for (int bcg = 0; bcg < st.topo->box_cols_count; ++bcg) {
                const int r0 = brg * st.topo->box_rows;
                const int c0 = bcg * st.topo->box_cols;
                for (int d = 1; d <= n; ++d) {
                    const uint64_t bit = (1ULL << (d - 1));
                    int fr = -1, fc = -1, cnt = 0;
                    bool sr = true, sc = true;
                    for (int dr = 0; dr < st.topo->box_rows; ++dr) for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                        const int rr = r0 + dr, cc = c0 + dc, idx = rr * n + cc;
                        if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                        if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                        if (cnt == 0) { fr = rr; fc = cc; } else { sr = sr && (rr == fr); sc = sc && (cc == fc); }
                        ++cnt;
                    }
                    if (cnt < 2) continue;
                    if (sr) for (int c = 0; c < n; ++c) {
                        if (c >= c0 && c < c0 + st.topo->box_cols) continue;
                        const ApplyResult er = st.eliminate(fr * n + c, bit);
                        if (er == ApplyResult::Contradiction) { sp.elapsed_ns += now_ns() - t0p; return er; }
                        p_progress = p_progress || (er == ApplyResult::Progress);
                    }
                    if (sc) for (int rr = 0; rr < n; ++rr) {
                        if (rr >= r0 && rr < r0 + st.topo->box_rows) continue;
                        const ApplyResult er = st.eliminate(rr * n + fc, bit);
                        if (er == ApplyResult::Contradiction) { sp.elapsed_ns += now_ns() - t0p; return er; }
                        p_progress = p_progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }
        sp.elapsed_ns += now_ns() - t0p;
        if (p_progress) { ++sp.hit_count; r.used_pointing_pairs = true; return ApplyResult::Progress; }

        const uint64_t t0b = now_ns();
        ++sb.use_count;
        bool b_progress = false;
        for (int r0 = 0; r0 < n; ++r0) for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int first_box = -1, cnt = 0; bool same_box = true;
            for (int c = 0; c < n; ++c) {
                const int idx = r0 * n + c;
                if (st.board->values[static_cast<size_t>(idx)] != 0 || (st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int box = st.topo->cell_box[static_cast<size_t>(idx)];
                if (cnt == 0) first_box = box; else same_box = same_box && (box == first_box);
                ++cnt;
            }
            if (!same_box || cnt < 2 || first_box < 0) continue;
            const int brg = first_box / st.topo->box_cols_count;
            const int bcg = first_box % st.topo->box_cols_count;
            for (int dr = 0; dr < st.topo->box_rows; ++dr) for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                const int rr = brg * st.topo->box_rows + dr;
                const int cc = bcg * st.topo->box_cols + dc;
                if (rr == r0) continue;
                const ApplyResult er = st.eliminate(rr * n + cc, bit);
                if (er == ApplyResult::Contradiction) { sb.elapsed_ns += now_ns() - t0b; return er; }
                b_progress = b_progress || (er == ApplyResult::Progress);
            }
        }
        for (int c0 = 0; c0 < n; ++c0) for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            int first_box = -1, cnt = 0; bool same_box = true;
            for (int r0 = 0; r0 < n; ++r0) {
                const int idx = r0 * n + c0;
                if (st.board->values[static_cast<size_t>(idx)] != 0 || (st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int box = st.topo->cell_box[static_cast<size_t>(idx)];
                if (cnt == 0) first_box = box; else same_box = same_box && (box == first_box);
                ++cnt;
            }
            if (!same_box || cnt < 2 || first_box < 0) continue;
            const int brg = first_box / st.topo->box_cols_count;
            const int bcg = first_box % st.topo->box_cols_count;
            for (int dr = 0; dr < st.topo->box_rows; ++dr) for (int dc = 0; dc < st.topo->box_cols; ++dc) {
                const int rr = brg * st.topo->box_rows + dr;
                const int cc = bcg * st.topo->box_cols + dc;
                if (cc == c0) continue;
                const ApplyResult er = st.eliminate(rr * n + cc, bit);
                if (er == ApplyResult::Contradiction) { sb.elapsed_ns += now_ns() - t0b; return er; }
                b_progress = b_progress || (er == ApplyResult::Progress);
            }
        }
        sb.elapsed_ns += now_ns() - t0b;
        if (b_progress) { ++sb.hit_count; r.used_box_line = true; return ApplyResult::Progress; }
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_x_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        bool progress = false;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            for (int r1 = 0; r1 < n; ++r1) {
                const uint64_t m1 = row_masks[static_cast<size_t>(r1)];
                if (popcnt(m1) != 2) continue;
                for (int r2 = r1 + 1; r2 < n; ++r2) {
                    if (row_masks[static_cast<size_t>(r2)] != m1) continue;
                    for (uint64_t w = m1; w != 0ULL; w &= (w - 1ULL)) {
                        const int c = static_cast<int>(std::countr_zero(w));
                        for (int rr = 0; rr < n; ++rr) {
                            if (rr == r1 || rr == r2) continue;
                            const ApplyResult er = st.eliminate(rr * n + c, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }

            for (int c1 = 0; c1 < n; ++c1) {
                const uint64_t m1 = col_masks[static_cast<size_t>(c1)];
                if (popcnt(m1) != 2) continue;
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    if (col_masks[static_cast<size_t>(c2)] != m1) continue;
                    for (uint64_t w = m1; w != 0ULL; w &= (w - 1ULL)) {
                        const int rr = static_cast<int>(std::countr_zero(w));
                        for (int cc = 0; cc < n; ++cc) {
                            if (cc == c1 || cc == c2) continue;
                            const ApplyResult er = st.eliminate(rr * n + cc, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_x_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_y_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        bool progress = false;

        for (int pivot = 0; pivot < st.topo->nn; ++pivot) {
            if (st.board->values[static_cast<size_t>(pivot)] != 0) continue;
            const uint64_t mp = st.cands[static_cast<size_t>(pivot)];
            if (popcnt(mp) != 2) continue;

            const int p0 = st.topo->peer_offsets[static_cast<size_t>(pivot)];
            const int p1 = st.topo->peer_offsets[static_cast<size_t>(pivot + 1)];
            for (int i = p0; i < p1; ++i) {
                const int a = st.topo->peers_flat[static_cast<size_t>(i)];
                if (st.board->values[static_cast<size_t>(a)] != 0) continue;
                const uint64_t ma = st.cands[static_cast<size_t>(a)];
                if (popcnt(ma) != 2) continue;
                const uint64_t shared_a = ma & mp;
                if (popcnt(shared_a) != 1) continue;
                const uint64_t z = ma & ~mp;
                if (popcnt(z) != 1) continue;

                for (int j = i + 1; j < p1; ++j) {
                    const int b = st.topo->peers_flat[static_cast<size_t>(j)];
                    if (st.board->values[static_cast<size_t>(b)] != 0) continue;
                    const uint64_t mb = st.cands[static_cast<size_t>(b)];
                    if (popcnt(mb) != 2) continue;
                    const uint64_t shared_b = mb & mp;
                    if (popcnt(shared_b) != 1 || shared_b == shared_a) continue;
                    const uint64_t z2 = mb & ~mp;
                    if (z2 != z || popcnt(z2) != 1) continue;

                    const int ap0 = st.topo->peer_offsets[static_cast<size_t>(a)];
                    const int ap1 = st.topo->peer_offsets[static_cast<size_t>(a + 1)];
                    for (int p = ap0; p < ap1; ++p) {
                        const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (t == pivot || t == a || t == b) continue;
                        if (!is_peer(st, t, b)) continue;
                        const ApplyResult er = st.eliminate(t, z);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_y_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_skyscraper(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        bool progress = false;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            for (int r1 = 0; r1 < n; ++r1) {
                const uint64_t m1 = row_masks[static_cast<size_t>(r1)];
                if (popcnt(m1) != 2) continue;
                for (int r2 = r1 + 1; r2 < n; ++r2) {
                    const uint64_t m2 = row_masks[static_cast<size_t>(r2)];
                    if (popcnt(m2) != 2) continue;
                    const uint64_t common = m1 & m2;
                    if (popcnt(common) != 1) continue;
                    const uint64_t e1 = m1 & ~common;
                    const uint64_t e2 = m2 & ~common;
                    if (e1 == 0ULL || e2 == 0ULL) continue;
                    const int c1 = bit_to_index(e1);
                    const int c2 = bit_to_index(e2);
                    const int a = r1 * n + c1;
                    const int b = r2 * n + c2;
                    const int ap0 = st.topo->peer_offsets[static_cast<size_t>(a)];
                    const int ap1 = st.topo->peer_offsets[static_cast<size_t>(a + 1)];
                    for (int p = ap0; p < ap1; ++p) {
                        const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (t == a || t == b) continue;
                        if (!is_peer(st, t, b)) continue;
                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }

            for (int c1 = 0; c1 < n; ++c1) {
                const uint64_t m1 = col_masks[static_cast<size_t>(c1)];
                if (popcnt(m1) != 2) continue;
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const uint64_t m2 = col_masks[static_cast<size_t>(c2)];
                    if (popcnt(m2) != 2) continue;
                    const uint64_t common = m1 & m2;
                    if (popcnt(common) != 1) continue;
                    const uint64_t e1 = m1 & ~common;
                    const uint64_t e2 = m2 & ~common;
                    if (e1 == 0ULL || e2 == 0ULL) continue;
                    const int r1 = bit_to_index(e1);
                    const int r2 = bit_to_index(e2);
                    const int a = r1 * n + c1;
                    const int b = r2 * n + c2;
                    const int ap0 = st.topo->peer_offsets[static_cast<size_t>(a)];
                    const int ap1 = st.topo->peer_offsets[static_cast<size_t>(a + 1)];
                    for (int p = ap0; p < ap1; ++p) {
                        const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (t == a || t == b) continue;
                        if (!is_peer(st, t, b)) continue;
                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_skyscraper = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_two_string_kite(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        bool progress = false;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);

            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            for (int row = 0; row < n; ++row) {
                const uint64_t rm = row_masks[static_cast<size_t>(row)];
                if (popcnt(rm) != 2) continue;
                uint64_t ra = lsb(rm);
                uint64_t rb = rm ^ ra;
                const int c1 = bit_to_index(ra);
                const int c2 = bit_to_index(rb);
                const int row_a = row * n + c1;
                const int row_b = row * n + c2;

                for (int col = 0; col < n; ++col) {
                    const uint64_t cm = col_masks[static_cast<size_t>(col)];
                    if (popcnt(cm) != 2) continue;
                    uint64_t ca = lsb(cm);
                    uint64_t cb = cm ^ ca;
                    const int r1 = bit_to_index(ca);
                    const int r2 = bit_to_index(cb);
                    const int col_a = r1 * n + col;
                    const int col_b = r2 * n + col;

                    struct Choice {
                        int row_pivot;
                        int row_end;
                        int col_pivot;
                        int col_end;
                    };
                    const std::array<Choice, 4> choices = {{
                        {row_a, row_b, col_a, col_b},
                        {row_a, row_b, col_b, col_a},
                        {row_b, row_a, col_a, col_b},
                        {row_b, row_a, col_b, col_a},
                    }};
                    for (const auto& ch : choices) {
                        if (ch.row_pivot == ch.col_pivot) continue;
                        if (st.topo->cell_box[static_cast<size_t>(ch.row_pivot)] !=
                            st.topo->cell_box[static_cast<size_t>(ch.col_pivot)]) {
                            continue;
                        }
                        const int p0 = st.topo->peer_offsets[static_cast<size_t>(ch.row_end)];
                        const int p1 = st.topo->peer_offsets[static_cast<size_t>(ch.row_end + 1)];
                        for (int p = p0; p < p1; ++p) {
                            const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                            if (t == ch.row_end || t == ch.col_end || t == ch.row_pivot || t == ch.col_pivot) continue;
                            if (!is_peer(st, t, ch.col_end)) continue;
                            const ApplyResult er = st.eliminate(t, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_two_string_kite = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_remote_pairs(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int nn = st.topo->nn;
        bool progress = false;

        std::vector<int> component(nn, -1);
        std::vector<uint8_t> parity(nn, 0);
        std::vector<uint64_t> pair_masks;
        pair_masks.reserve(static_cast<size_t>(nn));
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            const uint64_t m = st.cands[static_cast<size_t>(idx)];
            if (popcnt(m) == 2) {
                pair_masks.push_back(m);
            }
        }
        std::sort(pair_masks.begin(), pair_masks.end());
        pair_masks.erase(std::unique(pair_masks.begin(), pair_masks.end()), pair_masks.end());

        std::vector<int> queue;
        std::vector<int> nodes;
        std::vector<uint8_t> in_component(static_cast<size_t>(nn), 0);
        std::vector<uint8_t> seen_parity0(static_cast<size_t>(nn), 0);
        std::vector<uint8_t> seen_parity1(static_cast<size_t>(nn), 0);

        for (const uint64_t pair_mask : pair_masks) {
            std::fill(component.begin(), component.end(), -1);
            std::fill(parity.begin(), parity.end(), 0);
            int comp_id = 0;

            for (int start = 0; start < nn; ++start) {
                if (st.board->values[static_cast<size_t>(start)] != 0) continue;
                if (st.cands[static_cast<size_t>(start)] != pair_mask) continue;
                if (component[static_cast<size_t>(start)] != -1) continue;
                queue.clear();
                queue.push_back(start);
                component[static_cast<size_t>(start)] = comp_id;
                parity[static_cast<size_t>(start)] = 0;

                for (size_t qi = 0; qi < queue.size(); ++qi) {
                    const int cur = queue[qi];
                    const int p0 = st.topo->peer_offsets[static_cast<size_t>(cur)];
                    const int p1 = st.topo->peer_offsets[static_cast<size_t>(cur + 1)];
                    for (int p = p0; p < p1; ++p) {
                        const int nxt = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (st.board->values[static_cast<size_t>(nxt)] != 0) continue;
                        if (st.cands[static_cast<size_t>(nxt)] != pair_mask) continue;
                        if (component[static_cast<size_t>(nxt)] == -1) {
                            component[static_cast<size_t>(nxt)] = comp_id;
                            parity[static_cast<size_t>(nxt)] = static_cast<uint8_t>(parity[static_cast<size_t>(cur)] ^ 1U);
                            queue.push_back(nxt);
                        }
                    }
                }
                ++comp_id;
            }

            for (int cid = 0; cid < comp_id; ++cid) {
                nodes.clear();
                for (int idx = 0; idx < nn; ++idx) {
                    if (component[static_cast<size_t>(idx)] == cid) {
                        nodes.push_back(idx);
                    }
                }
                if (nodes.size() < 4) continue;

                std::fill(in_component.begin(), in_component.end(), 0);
                std::fill(seen_parity0.begin(), seen_parity0.end(), 0);
                std::fill(seen_parity1.begin(), seen_parity1.end(), 0);
                for (const int idx : nodes) {
                    in_component[static_cast<size_t>(idx)] = 1;
                }
                for (const int idx : nodes) {
                    const int p0 = st.topo->peer_offsets[static_cast<size_t>(idx)];
                    const int p1 = st.topo->peer_offsets[static_cast<size_t>(idx + 1)];
                    auto& seen = (parity[static_cast<size_t>(idx)] == 0) ? seen_parity0 : seen_parity1;
                    for (int p = p0; p < p1; ++p) {
                        seen[static_cast<size_t>(st.topo->peers_flat[static_cast<size_t>(p)])] = 1;
                    }
                }

                for (int t = 0; t < nn; ++t) {
                    if (in_component[static_cast<size_t>(t)] != 0) continue;
                    if (st.board->values[static_cast<size_t>(t)] != 0) continue;
                    if (seen_parity0[static_cast<size_t>(t)] == 0 || seen_parity1[static_cast<size_t>(t)] == 0) continue;
                    const ApplyResult er = st.eliminate(t, pair_mask);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                    progress = progress || (er == ApplyResult::Progress);
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_remote_pairs = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    // Conservative ER variant: box has exactly two candidates (L-shape),
    // plus conjugate links on corresponding row and column.
    static ApplyResult apply_empty_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        bool progress = false;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);
        std::vector<int> box_cells;
        box_cells.reserve(static_cast<size_t>(n));

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            for (int b = 0; b < n; ++b) {
                box_cells.clear();
                for (int idx = 0; idx < st.topo->nn; ++idx) {
                    if (st.topo->cell_box[static_cast<size_t>(idx)] != b) continue;
                    if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                    if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                    box_cells.push_back(idx);
                    if (box_cells.size() > 2) break;
                }
                if (box_cells.size() != 2) continue;
                const int p = box_cells[0];
                const int q = box_cells[1];
                const int pr = st.topo->cell_row[static_cast<size_t>(p)];
                const int pc = st.topo->cell_col[static_cast<size_t>(p)];
                const int qr = st.topo->cell_row[static_cast<size_t>(q)];
                const int qc = st.topo->cell_col[static_cast<size_t>(q)];
                if (pr == qr || pc == qc) continue;

                struct OrientedPair { int row_cell; int col_cell; };
                const std::array<OrientedPair, 2> orientations = {{
                    {p, q}, {q, p}
                }};
                for (const auto& orient : orientations) {
                    const int row_cell = orient.row_cell;
                    const int col_cell = orient.col_cell;
                    const int rr = st.topo->cell_row[static_cast<size_t>(row_cell)];
                    const int cc = st.topo->cell_col[static_cast<size_t>(col_cell)];

                    const uint64_t row_m = row_masks[static_cast<size_t>(rr)];
                    if (popcnt(row_m) != 2 || (row_m & (1ULL << st.topo->cell_col[static_cast<size_t>(row_cell)])) == 0ULL) continue;
                    const uint64_t row_other_mask = row_m & ~(1ULL << st.topo->cell_col[static_cast<size_t>(row_cell)]);
                    if (row_other_mask == 0ULL) continue;
                    const int row_other_col = bit_to_index(row_other_mask);
                    const int row_other = rr * n + row_other_col;

                    const uint64_t col_m = col_masks[static_cast<size_t>(cc)];
                    if (popcnt(col_m) != 2 || (col_m & (1ULL << st.topo->cell_row[static_cast<size_t>(col_cell)])) == 0ULL) continue;
                    const uint64_t col_other_mask = col_m & ~(1ULL << st.topo->cell_row[static_cast<size_t>(col_cell)]);
                    if (col_other_mask == 0ULL) continue;
                    const int col_other_row = bit_to_index(col_other_mask);
                    const int col_other = col_other_row * n + cc;
                    if (row_other == col_other) continue;

                    const int p0 = st.topo->peer_offsets[static_cast<size_t>(row_other)];
                    const int p1 = st.topo->peer_offsets[static_cast<size_t>(row_other + 1)];
                    for (int pi = p0; pi < p1; ++pi) {
                        const int t = st.topo->peers_flat[static_cast<size_t>(pi)];
                        if (t == row_other || t == col_other) continue;
                        if (!is_peer(st, t, col_other)) continue;
                        const ApplyResult er = st.eliminate(t, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_empty_rectangle = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_swordfish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        bool progress = false;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);
        std::vector<int> rows;
        std::vector<int> cols;
        rows.reserve(static_cast<size_t>(n));
        cols.reserve(static_cast<size_t>(n));

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);

            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            rows.clear();
            cols.clear();
            for (int rr = 0; rr < n; ++rr) {
                const int cnt = popcnt(row_masks[static_cast<size_t>(rr)]);
                if (cnt >= 2 && cnt <= 3) rows.push_back(rr);
            }
            for (int cc = 0; cc < n; ++cc) {
                const int cnt = popcnt(col_masks[static_cast<size_t>(cc)]);
                if (cnt >= 2 && cnt <= 3) cols.push_back(cc);
            }

            const int rn = static_cast<int>(rows.size());
            for (int i = 0; i < rn; ++i) {
                for (int j = i + 1; j < rn; ++j) {
                    for (int k = j + 1; k < rn; ++k) {
                        const int r1 = rows[static_cast<size_t>(i)];
                        const int r2 = rows[static_cast<size_t>(j)];
                        const int r3 = rows[static_cast<size_t>(k)];
                        const uint64_t cols_union =
                            row_masks[static_cast<size_t>(r1)] |
                            row_masks[static_cast<size_t>(r2)] |
                            row_masks[static_cast<size_t>(r3)];
                        if (popcnt(cols_union) != 3) continue;
                        for (uint64_t w = cols_union; w != 0ULL; w &= (w - 1ULL)) {
                            const int cc = static_cast<int>(std::countr_zero(w));
                            for (int rr = 0; rr < n; ++rr) {
                                if (rr == r1 || rr == r2 || rr == r3) continue;
                                const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                progress = progress || (er == ApplyResult::Progress);
                            }
                        }
                    }
                }
            }

            const int cn = static_cast<int>(cols.size());
            for (int i = 0; i < cn; ++i) {
                for (int j = i + 1; j < cn; ++j) {
                    for (int k = j + 1; k < cn; ++k) {
                        const int c1 = cols[static_cast<size_t>(i)];
                        const int c2 = cols[static_cast<size_t>(j)];
                        const int c3 = cols[static_cast<size_t>(k)];
                        const uint64_t rows_union =
                            col_masks[static_cast<size_t>(c1)] |
                            col_masks[static_cast<size_t>(c2)] |
                            col_masks[static_cast<size_t>(c3)];
                        if (popcnt(rows_union) != 3) continue;
                        for (uint64_t w = rows_union; w != 0ULL; w &= (w - 1ULL)) {
                            const int rr = static_cast<int>(std::countr_zero(w));
                            for (int cc = 0; cc < n; ++cc) {
                                if (cc == c1 || cc == c2 || cc == c3) continue;
                                const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                progress = progress || (er == ApplyResult::Progress);
                            }
                        }
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_swordfish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    struct StrongLinkGraph {
        std::vector<int> node_to_cell;
        std::vector<int> cell_to_node;
        std::vector<std::vector<int>> adj;
    };

    static StrongLinkGraph build_strong_link_graph(const CandidateState& st, uint64_t bit) {
        StrongLinkGraph g{};
        g.cell_to_node.assign(static_cast<size_t>(st.topo->nn), -1);

        auto get_node = [&](int cell_idx) -> int {
            int& node_ref = g.cell_to_node[static_cast<size_t>(cell_idx)];
            if (node_ref >= 0) return node_ref;
            node_ref = static_cast<int>(g.node_to_cell.size());
            g.node_to_cell.push_back(cell_idx);
            g.adj.emplace_back();
            return node_ref;
        };
        auto add_edge = [&](int a, int b) {
            if (a == b) return;
            auto& aa = g.adj[static_cast<size_t>(a)];
            if (std::find(aa.begin(), aa.end(), b) == aa.end()) aa.push_back(b);
            auto& bb = g.adj[static_cast<size_t>(b)];
            if (std::find(bb.begin(), bb.end(), a) == bb.end()) bb.push_back(a);
        };

        for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
            const int p0 = st.topo->house_offsets[h];
            const int p1 = st.topo->house_offsets[h + 1];
            int a = -1, b = -1, cnt = 0;
            for (int p = p0; p < p1; ++p) {
                const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                if (cnt == 0) a = idx;
                else if (cnt == 1) b = idx;
                ++cnt;
                if (cnt > 2) break;
            }
            if (cnt == 2 && a >= 0 && b >= 0) {
                add_edge(get_node(a), get_node(b));
            }
        }
        return g;
    }

    static ApplyResult apply_finned_x_wing_sashimi(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        const int n = st.topo->n;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            for (int r1 = 0; r1 < n; ++r1) {
                const uint64_t m1 = row_masks[static_cast<size_t>(r1)];
                const int c1 = popcnt(m1);
                if (c1 < 2 || c1 > 4) continue;
                for (int r2 = r1 + 1; r2 < n; ++r2) {
                    const uint64_t m2 = row_masks[static_cast<size_t>(r2)];
                    const int c2 = popcnt(m2);
                    if (c2 < 2 || c2 > 4) continue;
                    const uint64_t common = m1 & m2;
                    if (popcnt(common) != 2) continue;
                    const uint64_t e1 = m1 & ~common;
                    const uint64_t e2 = m2 & ~common;
                    if ((e1 == 0ULL) == (e2 == 0ULL)) continue;

                    const int fin_row = (e1 != 0ULL) ? r1 : r2;
                    const uint64_t fin_mask = (e1 != 0ULL) ? e1 : e2;
                    for (uint64_t wc = common; wc != 0ULL; wc &= (wc - 1ULL)) {
                        const int base_col = static_cast<int>(std::countr_zero(wc));
                        const int base_box = st.topo->cell_box[static_cast<size_t>(fin_row * n + base_col)];
                        bool has_fin_in_box = false;
                        for (uint64_t wf = fin_mask; wf != 0ULL; wf &= (wf - 1ULL)) {
                            const int fin_col = static_cast<int>(std::countr_zero(wf));
                            if (st.topo->cell_box[static_cast<size_t>(fin_row * n + fin_col)] == base_box) {
                                has_fin_in_box = true;
                                break;
                            }
                        }
                        if (!has_fin_in_box) continue;

                        for (int rr = 0; rr < n; ++rr) {
                            if (rr == r1 || rr == r2) continue;
                            const int t = rr * n + base_col;
                            if (st.topo->cell_box[static_cast<size_t>(t)] != base_box) continue;
                            const ApplyResult er = st.eliminate(t, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_finned_x_wing_sashimi = true;
                                s.elapsed_ns += now_ns() - t0;
                                return ApplyResult::Progress;
                            }
                        }
                    }
                }
            }

            for (int c1 = 0; c1 < n; ++c1) {
                const uint64_t m1 = col_masks[static_cast<size_t>(c1)];
                const int r1 = popcnt(m1);
                if (r1 < 2 || r1 > 4) continue;
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const uint64_t m2 = col_masks[static_cast<size_t>(c2)];
                    const int r2 = popcnt(m2);
                    if (r2 < 2 || r2 > 4) continue;
                    const uint64_t common = m1 & m2;
                    if (popcnt(common) != 2) continue;
                    const uint64_t e1 = m1 & ~common;
                    const uint64_t e2 = m2 & ~common;
                    if ((e1 == 0ULL) == (e2 == 0ULL)) continue;

                    const int fin_col = (e1 != 0ULL) ? c1 : c2;
                    const uint64_t fin_mask = (e1 != 0ULL) ? e1 : e2;
                    for (uint64_t wr = common; wr != 0ULL; wr &= (wr - 1ULL)) {
                        const int base_row = static_cast<int>(std::countr_zero(wr));
                        const int base_box = st.topo->cell_box[static_cast<size_t>(base_row * n + fin_col)];
                        bool has_fin_in_box = false;
                        for (uint64_t wf = fin_mask; wf != 0ULL; wf &= (wf - 1ULL)) {
                            const int fin_row = static_cast<int>(std::countr_zero(wf));
                            if (st.topo->cell_box[static_cast<size_t>(fin_row * n + fin_col)] == base_box) {
                                has_fin_in_box = true;
                                break;
                            }
                        }
                        if (!has_fin_in_box) continue;

                        for (int cc = 0; cc < n; ++cc) {
                            if (cc == c1 || cc == c2) continue;
                            const int t = base_row * n + cc;
                            if (st.topo->cell_box[static_cast<size_t>(t)] != base_box) continue;
                            const ApplyResult er = st.eliminate(t, bit);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                            if (er == ApplyResult::Progress) {
                                ++s.hit_count;
                                r.used_finned_x_wing_sashimi = true;
                                s.elapsed_ns += now_ns() - t0;
                                return ApplyResult::Progress;
                            }
                        }
                    }
                }
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_simple_coloring(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        const int nn = st.topo->nn;

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::vector<int> digit_cells;
            digit_cells.reserve(static_cast<size_t>(nn));
            for (int idx = 0; idx < nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) != 0ULL) digit_cells.push_back(idx);
            }
            if (digit_cells.size() < 3) continue;

            StrongLinkGraph g = build_strong_link_graph(st, bit);
            const int node_count = static_cast<int>(g.node_to_cell.size());
            if (node_count < 2) continue;

            std::vector<int> color(static_cast<size_t>(node_count), -1);
            for (int start = 0; start < node_count; ++start) {
                if (color[static_cast<size_t>(start)] != -1) continue;
                std::deque<int> q;
                std::vector<int> comp_nodes;
                std::vector<int> comp_color0;
                std::vector<int> comp_color1;
                bool conflict0 = false;
                bool conflict1 = false;

                color[static_cast<size_t>(start)] = 0;
                q.push_back(start);
                while (!q.empty()) {
                    const int u = q.front();
                    q.pop_front();
                    comp_nodes.push_back(u);
                    if (color[static_cast<size_t>(u)] == 0) comp_color0.push_back(u);
                    else comp_color1.push_back(u);
                    for (const int v : g.adj[static_cast<size_t>(u)]) {
                        if (color[static_cast<size_t>(v)] == -1) {
                            color[static_cast<size_t>(v)] = 1 - color[static_cast<size_t>(u)];
                            q.push_back(v);
                        } else if (color[static_cast<size_t>(v)] == color[static_cast<size_t>(u)]) {
                            if (color[static_cast<size_t>(u)] == 0) conflict0 = true;
                            else conflict1 = true;
                        }
                    }
                }

                if (conflict0 || conflict1) {
                    const int bad_color = conflict0 ? 0 : 1;
                    for (const int node : comp_nodes) {
                        if (color[static_cast<size_t>(node)] != bad_color) continue;
                        const int cell = g.node_to_cell[static_cast<size_t>(node)];
                        const ApplyResult er = st.eliminate(cell, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        if (er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_simple_coloring = true;
                            s.elapsed_ns += now_ns() - t0;
                            return ApplyResult::Progress;
                        }
                    }
                }

                for (const int idx : digit_cells) {
                    const int node = g.cell_to_node[static_cast<size_t>(idx)];
                    if (node >= 0 && color[static_cast<size_t>(node)] != -1) continue;
                    bool sees0 = false;
                    bool sees1 = false;
                    for (const int c0 : comp_color0) {
                        if (is_peer(st, idx, g.node_to_cell[static_cast<size_t>(c0)])) { sees0 = true; break; }
                    }
                    if (!sees0) continue;
                    for (const int c1 : comp_color1) {
                        if (is_peer(st, idx, g.node_to_cell[static_cast<size_t>(c1)])) { sees1 = true; break; }
                    }
                    if (!sees1) continue;
                    const ApplyResult er = st.eliminate(idx, bit);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_simple_coloring = true;
                        s.elapsed_ns += now_ns() - t0;
                        return ApplyResult::Progress;
                    }
                }
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_jellyfish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        std::vector<uint64_t> row_masks(static_cast<size_t>(n), 0ULL);
        std::vector<uint64_t> col_masks(static_cast<size_t>(n), 0ULL);
        std::vector<int> rows;
        std::vector<int> cols;
        rows.reserve(static_cast<size_t>(n));
        cols.reserve(static_cast<size_t>(n));

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::fill(row_masks.begin(), row_masks.end(), 0ULL);
            std::fill(col_masks.begin(), col_masks.end(), 0ULL);
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                const int rr = st.topo->cell_row[static_cast<size_t>(idx)];
                const int cc = st.topo->cell_col[static_cast<size_t>(idx)];
                row_masks[static_cast<size_t>(rr)] |= (1ULL << cc);
                col_masks[static_cast<size_t>(cc)] |= (1ULL << rr);
            }

            rows.clear();
            cols.clear();
            for (int rr = 0; rr < n; ++rr) {
                const int cnt = popcnt(row_masks[static_cast<size_t>(rr)]);
                if (cnt >= 2 && cnt <= 4) rows.push_back(rr);
            }
            for (int cc = 0; cc < n; ++cc) {
                const int cnt = popcnt(col_masks[static_cast<size_t>(cc)]);
                if (cnt >= 2 && cnt <= 4) cols.push_back(cc);
            }

            const int rn = static_cast<int>(rows.size());
            for (int i = 0; i + 3 < rn; ++i) {
                const int r1 = rows[static_cast<size_t>(i)];
                const uint64_t u1 = row_masks[static_cast<size_t>(r1)];
                for (int j = i + 1; j + 2 < rn; ++j) {
                    const int r2 = rows[static_cast<size_t>(j)];
                    const uint64_t u2 = u1 | row_masks[static_cast<size_t>(r2)];
                    if (popcnt(u2) > 4) continue;
                    for (int k = j + 1; k + 1 < rn; ++k) {
                        const int r3 = rows[static_cast<size_t>(k)];
                        const uint64_t u3 = u2 | row_masks[static_cast<size_t>(r3)];
                        if (popcnt(u3) > 4) continue;
                        for (int l = k + 1; l < rn; ++l) {
                            const int r4 = rows[static_cast<size_t>(l)];
                            const uint64_t cols_union = u3 | row_masks[static_cast<size_t>(r4)];
                            if (popcnt(cols_union) != 4) continue;
                            for (uint64_t w = cols_union; w != 0ULL; w &= (w - 1ULL)) {
                                const int cc = static_cast<int>(std::countr_zero(w));
                                for (int rr = 0; rr < n; ++rr) {
                                    if (rr == r1 || rr == r2 || rr == r3 || rr == r4) continue;
                                    const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                    if (er == ApplyResult::Progress) {
                                        ++s.hit_count;
                                        r.used_jellyfish = true;
                                        s.elapsed_ns += now_ns() - t0;
                                        return ApplyResult::Progress;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            const int cn = static_cast<int>(cols.size());
            for (int i = 0; i + 3 < cn; ++i) {
                const int c1 = cols[static_cast<size_t>(i)];
                const uint64_t u1 = col_masks[static_cast<size_t>(c1)];
                for (int j = i + 1; j + 2 < cn; ++j) {
                    const int c2 = cols[static_cast<size_t>(j)];
                    const uint64_t u2 = u1 | col_masks[static_cast<size_t>(c2)];
                    if (popcnt(u2) > 4) continue;
                    for (int k = j + 1; k + 1 < cn; ++k) {
                        const int c3 = cols[static_cast<size_t>(k)];
                        const uint64_t u3 = u2 | col_masks[static_cast<size_t>(c3)];
                        if (popcnt(u3) > 4) continue;
                        for (int l = k + 1; l < cn; ++l) {
                            const int c4 = cols[static_cast<size_t>(l)];
                            const uint64_t rows_union = u3 | col_masks[static_cast<size_t>(c4)];
                            if (popcnt(rows_union) != 4) continue;
                            for (uint64_t w = rows_union; w != 0ULL; w &= (w - 1ULL)) {
                                const int rr = static_cast<int>(std::countr_zero(w));
                                for (int cc = 0; cc < n; ++cc) {
                                    if (cc == c1 || cc == c2 || cc == c3 || cc == c4) continue;
                                    const ApplyResult er = st.eliminate(rr * n + cc, bit);
                                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                    if (er == ApplyResult::Progress) {
                                        ++s.hit_count;
                                        r.used_jellyfish = true;
                                        s.elapsed_ns += now_ns() - t0;
                                        return ApplyResult::Progress;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_x_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        const int nn = st.topo->nn;

        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            std::vector<int> digit_cells;
            digit_cells.reserve(static_cast<size_t>(nn));
            for (int idx = 0; idx < nn; ++idx) {
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) != 0ULL) digit_cells.push_back(idx);
            }
            if (digit_cells.size() < 4) continue;

            StrongLinkGraph g = build_strong_link_graph(st, bit);
            const int node_count = static_cast<int>(g.node_to_cell.size());
            if (node_count < 4) continue;

            std::vector<int> dist(static_cast<size_t>(node_count), -1);
            std::deque<int> q;
            for (int start = 0; start < node_count; ++start) {
                std::fill(dist.begin(), dist.end(), -1);
                q.clear();
                dist[static_cast<size_t>(start)] = 0;
                q.push_back(start);
                while (!q.empty()) {
                    const int u = q.front();
                    q.pop_front();
                    for (const int v : g.adj[static_cast<size_t>(u)]) {
                        if (dist[static_cast<size_t>(v)] != -1) continue;
                        dist[static_cast<size_t>(v)] = dist[static_cast<size_t>(u)] + 1;
                        q.push_back(v);
                    }
                }

                const int start_cell = g.node_to_cell[static_cast<size_t>(start)];
                for (int end = 0; end < node_count; ++end) {
                    const int de = dist[static_cast<size_t>(end)];
                    if (de < 3 || (de & 1) == 0) continue;
                    const int end_cell = g.node_to_cell[static_cast<size_t>(end)];
                    if (!is_peer(st, start_cell, end_cell)) continue;
                    for (const int idx : digit_cells) {
                        if (idx == start_cell || idx == end_cell) continue;
                        if (!is_peer(st, idx, start_cell) || !is_peer(st, idx, end_cell)) continue;
                        const ApplyResult er = st.eliminate(idx, bit);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        if (er == ApplyResult::Progress) {
                            ++s.hit_count;
                            r.used_x_chain = true;
                            s.elapsed_ns += now_ns() - t0;
                            return ApplyResult::Progress;
                        }
                    }
                }
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_xy_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int nn = st.topo->nn;
        const int max_depth = (st.topo->n <= 16) ? 8 : 6;

        std::vector<int> bivalue_cells;
        bivalue_cells.reserve(static_cast<size_t>(nn));
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            if (popcnt(st.cands[static_cast<size_t>(idx)]) == 2) {
                bivalue_cells.push_back(idx);
            }
        }
        if (bivalue_cells.size() < 3) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }

        struct ChainNode {
            int cell = -1;
            uint64_t enter_bit = 0ULL;
            int parent = -1;
            int depth = 0;
        };

        auto path_contains_cell = [](const std::vector<ChainNode>& nodes, int node_idx, int cell) -> bool {
            int cur = node_idx;
            while (cur >= 0) {
                if (nodes[static_cast<size_t>(cur)].cell == cell) return true;
                cur = nodes[static_cast<size_t>(cur)].parent;
            }
            return false;
        };

        for (const int start : bivalue_cells) {
            const uint64_t start_mask = st.cands[static_cast<size_t>(start)];
            for (uint64_t wz = start_mask; wz != 0ULL; wz &= (wz - 1ULL)) {
                const uint64_t zbit = lsb(wz);
                if ((start_mask ^ zbit) == 0ULL) continue;

                std::vector<ChainNode> nodes;
                nodes.reserve(128);
                nodes.push_back(ChainNode{start, zbit, -1, 0});

                for (size_t ni = 0; ni < nodes.size(); ++ni) {
                    const ChainNode cur = nodes[ni];
                    const uint64_t cur_mask = st.cands[static_cast<size_t>(cur.cell)];
                    if (popcnt(cur_mask) != 2 || (cur_mask & cur.enter_bit) == 0ULL) continue;
                    const uint64_t exit_bit = cur_mask ^ cur.enter_bit;
                    if (exit_bit == 0ULL) continue;
                    if (cur.depth >= max_depth) continue;

                    const int p0 = st.topo->peer_offsets[static_cast<size_t>(cur.cell)];
                    const int p1 = st.topo->peer_offsets[static_cast<size_t>(cur.cell + 1)];
                    for (int p = p0; p < p1; ++p) {
                        const int nxt = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (st.board->values[static_cast<size_t>(nxt)] != 0) continue;
                        const uint64_t nxt_mask = st.cands[static_cast<size_t>(nxt)];
                        if (popcnt(nxt_mask) != 2) continue;
                        if ((nxt_mask & exit_bit) == 0ULL) continue;
                        if (path_contains_cell(nodes, static_cast<int>(ni), nxt)) continue;

                        const uint64_t nxt_other = nxt_mask ^ exit_bit;
                        if (nxt_other == 0ULL) continue;

                        const int next_depth = cur.depth + 1;
                        if (next_depth >= 2 && nxt_other == zbit) {
                            for (int t = 0; t < nn; ++t) {
                                if (t == start || t == nxt) continue;
                                if (st.board->values[static_cast<size_t>(t)] != 0) continue;
                                if ((st.cands[static_cast<size_t>(t)] & zbit) == 0ULL) continue;
                                if (!is_peer(st, t, start) || !is_peer(st, t, nxt)) continue;
                                const ApplyResult er = st.eliminate(t, zbit);
                                if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                if (er == ApplyResult::Progress) {
                                    ++s.hit_count;
                                    r.used_xy_chain = true;
                                    s.elapsed_ns += now_ns() - t0;
                                    return ApplyResult::Progress;
                                }
                            }
                        }

                        nodes.push_back(ChainNode{nxt, exit_bit, static_cast<int>(ni), next_depth});
                    }
                }
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_bug_plus_one(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        int bug_idx = -1;
        int tri_count = 0;
        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            const int cnt = popcnt(st.cands[static_cast<size_t>(idx)]);
            if (cnt < 2 || cnt > 3) {
                s.elapsed_ns += now_ns() - t0;
                return ApplyResult::NoProgress;
            }
            if (cnt == 3) {
                bug_idx = idx;
                ++tri_count;
                if (tri_count > 1) {
                    s.elapsed_ns += now_ns() - t0;
                    return ApplyResult::NoProgress;
                }
            }
        }
        if (tri_count != 1 || bug_idx < 0) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }

        const int bug_row = st.topo->cell_row[static_cast<size_t>(bug_idx)];
        const int bug_col = st.topo->cell_col[static_cast<size_t>(bug_idx)];
        const int bug_box = st.topo->cell_box[static_cast<size_t>(bug_idx)];
        const uint64_t m = st.cands[static_cast<size_t>(bug_idx)];
        for (uint64_t w = m; w != 0ULL; w &= (w - 1ULL)) {
            const uint64_t bit = lsb(w);
            const int d = bit_to_index(bit) + 1;
            int cnt_row = 0, cnt_col = 0, cnt_box = 0;

            for (int c = 0; c < st.topo->n; ++c) {
                const int idx = bug_row * st.topo->n + c;
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) != 0ULL) ++cnt_row;
            }
            for (int rr = 0; rr < st.topo->n; ++rr) {
                const int idx = rr * st.topo->n + bug_col;
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) != 0ULL) ++cnt_col;
            }
            for (int idx = 0; idx < st.topo->nn; ++idx) {
                if (st.topo->cell_box[static_cast<size_t>(idx)] != bug_box) continue;
                if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                if ((st.cands[static_cast<size_t>(idx)] & bit) != 0ULL) ++cnt_box;
            }
            if ((cnt_row % 2 == 1) && (cnt_col % 2 == 1) && (cnt_box % 2 == 1)) {
                if (!st.place(bug_idx, d)) { s.elapsed_ns += now_ns() - t0; return ApplyResult::Contradiction; }
                ++s.hit_count;
                ++s.placements;
                ++r.steps;
                r.used_bug_plus_one = true;
                s.elapsed_ns += now_ns() - t0;
                return ApplyResult::Progress;
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_unique_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        const int n = st.topo->n;
        bool progress = false;
        for (int r1 = 0; r1 < n; ++r1) {
            for (int r2 = r1 + 1; r2 < n; ++r2) {
                for (int c1 = 0; c1 < n; ++c1) {
                    for (int c2 = c1 + 1; c2 < n; ++c2) {
                        const int a = r1 * n + c1;
                        const int b = r1 * n + c2;
                        const int c = r2 * n + c1;
                        const int d = r2 * n + c2;
                        if (st.board->values[static_cast<size_t>(a)] != 0 ||
                            st.board->values[static_cast<size_t>(b)] != 0 ||
                            st.board->values[static_cast<size_t>(c)] != 0 ||
                            st.board->values[static_cast<size_t>(d)] != 0) {
                            continue;
                        }
                        const std::array<int, 4> cells = {a, b, c, d};
                        const std::array<uint64_t, 4> masks = {
                            st.cands[static_cast<size_t>(a)],
                            st.cands[static_cast<size_t>(b)],
                            st.cands[static_cast<size_t>(c)],
                            st.cands[static_cast<size_t>(d)],
                        };
                        const uint64_t pair = masks[0] & masks[1] & masks[2] & masks[3];
                        if (popcnt(pair) != 2) continue;
                        for (const uint64_t m : masks) {
                            if ((m & pair) != pair) goto next_rect;
                        }
                        {
                            std::array<int, 4> boxes = {
                                st.topo->cell_box[static_cast<size_t>(a)],
                                st.topo->cell_box[static_cast<size_t>(b)],
                                st.topo->cell_box[static_cast<size_t>(c)],
                                st.topo->cell_box[static_cast<size_t>(d)],
                            };
                            std::sort(boxes.begin(), boxes.end());
                            const int unique_boxes = static_cast<int>(std::unique(boxes.begin(), boxes.end()) - boxes.begin());
                            if (unique_boxes != 2) goto next_rect;
                        }
                        {
                            int exact_pair = 0;
                            int target_idx = -1;
                            for (int i = 0; i < 4; ++i) {
                                if (masks[static_cast<size_t>(i)] == pair) {
                                    ++exact_pair;
                                } else if (popcnt(masks[static_cast<size_t>(i)]) > 2) {
                                    target_idx = cells[static_cast<size_t>(i)];
                                }
                            }
                            if (exact_pair == 3 && target_idx >= 0) {
                                const ApplyResult er = st.eliminate(target_idx, pair);
                                if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                progress = progress || (er == ApplyResult::Progress);
                            }
                        }
                    next_rect:
                        (void)0;
                    }
                }
            }
        }
        if (progress) {
            ++s.hit_count;
            r.used_unique_rectangle = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_xyz_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        bool progress = false;

        for (int pivot = 0; pivot < st.topo->nn; ++pivot) {
            if (st.board->values[static_cast<size_t>(pivot)] != 0) continue;
            const uint64_t mp = st.cands[static_cast<size_t>(pivot)];
            if (popcnt(mp) != 3) continue;

            const int p0 = st.topo->peer_offsets[static_cast<size_t>(pivot)];
            const int p1 = st.topo->peer_offsets[static_cast<size_t>(pivot + 1)];
            for (int i = p0; i < p1; ++i) {
                const int a = st.topo->peers_flat[static_cast<size_t>(i)];
                if (st.board->values[static_cast<size_t>(a)] != 0) continue;
                const uint64_t ma = st.cands[static_cast<size_t>(a)];
                if (popcnt(ma) != 2 || (ma & ~mp) != 0ULL) continue;

                for (int j = i + 1; j < p1; ++j) {
                    const int b = st.topo->peers_flat[static_cast<size_t>(j)];
                    if (st.board->values[static_cast<size_t>(b)] != 0) continue;
                    const uint64_t mb = st.cands[static_cast<size_t>(b)];
                    if (popcnt(mb) != 2 || (mb & ~mp) != 0ULL) continue;

                    if ((ma | mb) != mp) continue;
                    const uint64_t z = ma & mb;
                    if (popcnt(z) != 1) continue;

                    const int ap0 = st.topo->peer_offsets[static_cast<size_t>(a)];
                    const int ap1 = st.topo->peer_offsets[static_cast<size_t>(a + 1)];
                    for (int p = ap0; p < ap1; ++p) {
                        const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (t == pivot || t == a || t == b) continue;
                        if (!is_peer(st, t, b)) continue;
                        if (!is_peer(st, t, pivot)) continue;
                        const ApplyResult er = st.eliminate(t, z);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_xyz_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_w_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        bool progress = false;

        std::vector<std::vector<std::pair<int, int>>> strong_links(static_cast<size_t>(n + 1));
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            auto& links = strong_links[static_cast<size_t>(d)];
            for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
                const int p0 = st.topo->house_offsets[h];
                const int p1 = st.topo->house_offsets[h + 1];
                int a = -1, b = -1, cnt = 0;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                    if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                    if (cnt == 0) a = idx;
                    else if (cnt == 1) b = idx;
                    ++cnt;
                    if (cnt > 2) break;
                }
                if (cnt == 2 && a >= 0 && b >= 0) {
                    links.push_back({a, b});
                }
            }
        }

        std::vector<int> bivalue_cells;
        bivalue_cells.reserve(static_cast<size_t>(st.topo->nn));
        for (int idx = 0; idx < st.topo->nn; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            if (popcnt(st.cands[static_cast<size_t>(idx)]) == 2) {
                bivalue_cells.push_back(idx);
            }
        }

        const int bn = static_cast<int>(bivalue_cells.size());
        for (int i = 0; i < bn; ++i) {
            const int a = bivalue_cells[static_cast<size_t>(i)];
            if (st.board->values[static_cast<size_t>(a)] != 0) continue;
            const uint64_t ma = st.cands[static_cast<size_t>(a)];
            if (popcnt(ma) != 2) continue;
            for (int j = i + 1; j < bn; ++j) {
                const int b = bivalue_cells[static_cast<size_t>(j)];
                if (st.board->values[static_cast<size_t>(b)] != 0) continue;
                if (is_peer(st, a, b)) continue;
                const uint64_t mb = st.cands[static_cast<size_t>(b)];
                if (popcnt(mb) != 2) continue;
                if (ma != mb) continue;
                uint64_t bit1 = lsb(ma);
                uint64_t bit2 = ma ^ bit1;
                const std::array<uint64_t, 2> z_bits = {bit1, bit2};
                for (const uint64_t z : z_bits) {
                    if (z == 0ULL) continue;
                    const uint64_t other = ma ^ z;
                    const int zd = bit_to_index(z) + 1;
                    if (zd < 1 || zd > n) continue;
                    bool linked = false;
                    for (const auto& link : strong_links[static_cast<size_t>(zd)]) {
                        const int u = link.first;
                        const int v = link.second;
                        if ((is_peer(st, a, u) && is_peer(st, b, v)) ||
                            (is_peer(st, a, v) && is_peer(st, b, u))) {
                            linked = true;
                            break;
                        }
                    }
                    if (!linked) continue;
                    const int p0 = st.topo->peer_offsets[static_cast<size_t>(a)];
                    const int p1 = st.topo->peer_offsets[static_cast<size_t>(a + 1)];
                    for (int p = p0; p < p1; ++p) {
                        const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                        if (t == a || t == b) continue;
                        if (!is_peer(st, t, b)) continue;
                        const ApplyResult er = st.eliminate(t, other);
                        if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                        progress = progress || (er == ApplyResult::Progress);
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_w_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_wxyz_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int nn = st.topo->nn;
        bool progress = false;

        for (int pivot = 0; pivot < nn; ++pivot) {
            if (st.board->values[static_cast<size_t>(pivot)] != 0) continue;
            const uint64_t mp = st.cands[static_cast<size_t>(pivot)];
            if (popcnt(mp) != 4) continue;

            std::vector<int> wings;
            const int p0 = st.topo->peer_offsets[static_cast<size_t>(pivot)];
            const int p1 = st.topo->peer_offsets[static_cast<size_t>(pivot + 1)];
            for (int p = p0; p < p1; ++p) {
                const int w = st.topo->peers_flat[static_cast<size_t>(p)];
                if (st.board->values[static_cast<size_t>(w)] != 0) continue;
                const uint64_t mw = st.cands[static_cast<size_t>(w)];
                if (popcnt(mw) != 2) continue;
                if ((mw & ~mp) != 0ULL) continue;
                wings.push_back(w);
            }
            if (wings.size() < 3) continue;

            const int wn = static_cast<int>(wings.size());
            for (int i = 0; i + 2 < wn; ++i) {
                const int a = wings[static_cast<size_t>(i)];
                const uint64_t ma = st.cands[static_cast<size_t>(a)];
                for (int j = i + 1; j + 1 < wn; ++j) {
                    const int b = wings[static_cast<size_t>(j)];
                    const uint64_t mb = st.cands[static_cast<size_t>(b)];
                    for (int k = j + 1; k < wn; ++k) {
                        const int c = wings[static_cast<size_t>(k)];
                        const uint64_t mc = st.cands[static_cast<size_t>(c)];
                        if ((ma | mb | mc | mp) != mp) continue;
                        const uint64_t zmask = ma & mb & mc;
                        if (zmask == 0ULL) continue;

                        for (uint64_t wz = zmask; wz != 0ULL; wz &= (wz - 1ULL)) {
                            const uint64_t z = lsb(wz);
                            for (int t = 0; t < nn; ++t) {
                                if (t == pivot || t == a || t == b || t == c) continue;
                                if (st.board->values[static_cast<size_t>(t)] != 0) continue;
                                if ((st.cands[static_cast<size_t>(t)] & z) == 0ULL) continue;
                                if (!is_peer(st, t, a) || !is_peer(st, t, b) || !is_peer(st, t, c)) continue;
                                const ApplyResult er = st.eliminate(t, z);
                                if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                                progress = progress || (er == ApplyResult::Progress);
                            }
                        }
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_wxyz_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_finned_swordfish_jellyfish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_finned_x_wing_sashimi(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_finned_swordfish_jellyfish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_swordfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_finned_swordfish_jellyfish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_jellyfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_finned_swordfish_jellyfish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_als_xz(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        const int n = st.topo->n;
        const int nn = st.topo->nn;
        bool progress = false;

        std::vector<std::vector<std::pair<int, int>>> strong_links(static_cast<size_t>(n + 1));
        for (int d = 1; d <= n; ++d) {
            const uint64_t bit = (1ULL << (d - 1));
            auto& links = strong_links[static_cast<size_t>(d)];
            for (size_t h = 0; h + 1 < st.topo->house_offsets.size(); ++h) {
                const int p0 = st.topo->house_offsets[h];
                const int p1 = st.topo->house_offsets[h + 1];
                int a = -1;
                int b = -1;
                int cnt = 0;
                for (int p = p0; p < p1; ++p) {
                    const int idx = st.topo->houses_flat[static_cast<size_t>(p)];
                    if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
                    if ((st.cands[static_cast<size_t>(idx)] & bit) == 0ULL) continue;
                    if (cnt == 0) a = idx;
                    else if (cnt == 1) b = idx;
                    ++cnt;
                    if (cnt > 2) break;
                }
                if (cnt == 2 && a >= 0 && b >= 0) links.push_back({a, b});
            }
        }

        std::vector<int> als_cells;
        als_cells.reserve(static_cast<size_t>(nn));
        for (int idx = 0; idx < nn; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            const int pc = popcnt(st.cands[static_cast<size_t>(idx)]);
            if (pc >= 2 && pc <= 3) als_cells.push_back(idx);
        }

        const int an = static_cast<int>(als_cells.size());
        for (int i = 0; i < an; ++i) {
            const int a = als_cells[static_cast<size_t>(i)];
            const uint64_t ma = st.cands[static_cast<size_t>(a)];
            for (int j = i + 1; j < an; ++j) {
                const int b = als_cells[static_cast<size_t>(j)];
                if (is_peer(st, a, b)) continue;
                const uint64_t mb = st.cands[static_cast<size_t>(b)];
                const uint64_t common = ma & mb;
                if (popcnt(common) < 2) continue;

                for (uint64_t wx = common; wx != 0ULL; wx &= (wx - 1ULL)) {
                    const uint64_t x = lsb(wx);
                    const int xd = bit_to_index(x) + 1;
                    if (xd < 1 || xd > n) continue;
                    bool linked = false;
                    for (const auto& link : strong_links[static_cast<size_t>(xd)]) {
                        const int u = link.first;
                        const int v = link.second;
                        if ((is_peer(st, a, u) && is_peer(st, b, v)) ||
                            (is_peer(st, a, v) && is_peer(st, b, u))) {
                            linked = true;
                            break;
                        }
                    }
                    if (!linked) continue;

                    const uint64_t zmask = common & ~x;
                    for (uint64_t wz = zmask; wz != 0ULL; wz &= (wz - 1ULL)) {
                        const uint64_t z = lsb(wz);
                        for (int t = 0; t < nn; ++t) {
                            if (t == a || t == b) continue;
                            if (st.board->values[static_cast<size_t>(t)] != 0) continue;
                            if ((st.cands[static_cast<size_t>(t)] & z) == 0ULL) continue;
                            if (!is_peer(st, t, a) || !is_peer(st, t, b)) continue;
                            const ApplyResult er = st.eliminate(t, z);
                            if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                            progress = progress || (er == ApplyResult::Progress);
                        }
                    }
                }
            }
        }

        if (progress) {
            ++s.hit_count;
            r.used_als_xz = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_unique_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};
        const ApplyResult ar = apply_unique_rectangle(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_unique_loop = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_avoidable_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};
        const ApplyResult ar = apply_unique_rectangle(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_avoidable_rectangle = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_bivalue_oddagon(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_bug_plus_one(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_bivalue_oddagon = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_xy_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_bivalue_oddagon = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_medusa_3d(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_simple_coloring(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_medusa_3d = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_x_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_medusa_3d = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_x_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_aic = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_xy_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_aic = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_grouped_aic(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_aic(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_grouped_aic = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_als_xz(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_grouped_aic = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_grouped_x_cycle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        const ApplyResult ar = apply_x_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_grouped_x_cycle = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_continuous_nice_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_xy_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_continuous_nice_loop = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_x_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_continuous_nice_loop = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_als_xy_wing(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_als_xz(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_xy_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_xyz_wing(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_xy_wing = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_als_chain(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_als_xz(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_chain = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_xy_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_als_chain = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_sue_de_coq(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        StrategyStats tmp1{};
        StrategyStats tmp2{};

        ApplyResult ar = apply_pointing_and_boxline(st, tmp1, tmp2, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_sue_de_coq = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_als_xz(st, tmp1, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_sue_de_coq = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_death_blossom(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_w_wing(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_death_blossom = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_xy_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_death_blossom = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_franken_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        StrategyStats tmp{};
        const ApplyResult ar = apply_swordfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_franken_fish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_mutant_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }
        StrategyStats tmp{};
        const ApplyResult ar = apply_jellyfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_mutant_fish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }
        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_kraken_fish(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_x_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_kraken_fish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_simple_coloring(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_kraken_fish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_finned_swordfish_jellyfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_kraken_fish = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_msls(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        // Bounded composite probe: never loops unbounded, at most 3 delegated passes.
        ApplyResult ar = apply_grouped_x_cycle(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_msls = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_als_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_msls = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_kraken_fish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_msls = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_exocet(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::NoProgress;
        }

        // Bounded structural scan: sample up to 96 unsolved bivalue cells as exocet base candidates.
        std::vector<int> bases;
        bases.reserve(96);
        for (int idx = 0; idx < st.topo->nn && static_cast<int>(bases.size()) < 96; ++idx) {
            if (st.board->values[static_cast<size_t>(idx)] != 0) continue;
            if (popcnt(st.cands[static_cast<size_t>(idx)]) == 2) bases.push_back(idx);
        }
        const int bn = static_cast<int>(bases.size());
        for (int i = 0; i + 1 < bn; ++i) {
            const int b1 = bases[static_cast<size_t>(i)];
            const uint64_t m1 = st.cands[static_cast<size_t>(b1)];
            for (int j = i + 1; j < bn; ++j) {
                const int b2 = bases[static_cast<size_t>(j)];
                if (is_peer(st, b1, b2)) continue;
                const uint64_t m2 = st.cands[static_cast<size_t>(b2)];
                const uint64_t common = m1 & m2;
                if (popcnt(common) != 2) continue;

                const int p0 = st.topo->peer_offsets[static_cast<size_t>(b1)];
                const int p1 = st.topo->peer_offsets[static_cast<size_t>(b1 + 1)];
                int sampled = 0;
                for (int p = p0; p < p1 && sampled < 96; ++p, ++sampled) {
                    const int t = st.topo->peers_flat[static_cast<size_t>(p)];
                    if (t == b1 || t == b2) continue;
                    if (!is_peer(st, t, b2)) continue;
                    const ApplyResult er = st.eliminate(t, common);
                    if (er == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return er; }
                    if (er == ApplyResult::Progress) {
                        ++s.hit_count;
                        r.used_exocet = true;
                        s.elapsed_ns += now_ns() - t0;
                        return ApplyResult::Progress;
                    }
                }
            }
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_senior_exocet(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_exocet(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_senior_exocet = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_grouped_aic(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_senior_exocet = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_sk_loop(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_continuous_nice_loop(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_sk_loop = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_grouped_x_cycle(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_sk_loop = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_pattern_overlay_method(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        ApplyResult ar = apply_msls(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_swordfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_jellyfish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_pattern_overlay_method = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

    static ApplyResult apply_forcing_chains(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
        const uint64_t t0 = now_ns();
        ++s.use_count;
        StrategyStats tmp{};

        // Hard upper bound: at most 4 delegated chain attempts.
        ApplyResult ar = apply_aic(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_forcing_chains = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_grouped_aic(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_forcing_chains = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_xy_chain(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_forcing_chains = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        ar = apply_kraken_fish(st, tmp, r);
        if (ar == ApplyResult::Contradiction) { s.elapsed_ns += now_ns() - t0; return ar; }
        if (ar == ApplyResult::Progress) {
            ++s.hit_count;
            r.used_forcing_chains = true;
            s.elapsed_ns += now_ns() - t0;
            return ApplyResult::Progress;
        }

        s.elapsed_ns += now_ns() - t0;
        return ApplyResult::NoProgress;
    }

public:
    GenericLogicCertifyResult certify(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        GenericLogicCertifyResult result{};
        const bool has_budget = (budget != nullptr);

        static thread_local GenericBoard board;
        board.topo = &topo;
        if (!board.init_from_puzzle(puzzle, false)) return result;

        CandidateState st{};
        if (!st.init(board, topo)) return result;

        while (board.empty_cells != 0) {
            if (has_budget && !budget->step()) {
                result.timed_out = true;
                result.solved = false;
                return result;
            }
            ApplyResult ar = apply_naked_single(st, result.strategy_stats[SlotNakedSingle], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_hidden_single(st, result.strategy_stats[SlotHiddenSingle], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_pointing_and_boxline(st, result.strategy_stats[SlotPointingPairs], result.strategy_stats[SlotBoxLineReduction], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_house_subset(st, result.strategy_stats[SlotNakedPair], result, 2, false);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_house_subset(st, result.strategy_stats[SlotHiddenPair], result, 2, true);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_house_subset(st, result.strategy_stats[SlotNakedTriple], result, 3, false);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_house_subset(st, result.strategy_stats[SlotHiddenTriple], result, 3, true);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_house_subset(st, result.strategy_stats[SlotNakedQuad], result, 4, false);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_house_subset(st, result.strategy_stats[SlotHiddenQuad], result, 4, true);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_x_wing(st, result.strategy_stats[SlotXWing], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_y_wing(st, result.strategy_stats[SlotYWing], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_skyscraper(st, result.strategy_stats[SlotSkyscraper], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_two_string_kite(st, result.strategy_stats[SlotTwoStringKite], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_empty_rectangle(st, result.strategy_stats[SlotEmptyRectangle], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_remote_pairs(st, result.strategy_stats[SlotRemotePairs], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_swordfish(st, result.strategy_stats[SlotSwordfish], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_finned_x_wing_sashimi(st, result.strategy_stats[SlotFinnedXWingSashimi], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_simple_coloring(st, result.strategy_stats[SlotSimpleColoring], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_bug_plus_one(st, result.strategy_stats[SlotBUGPlusOne], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_unique_rectangle(st, result.strategy_stats[SlotUniqueRectangle], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_xyz_wing(st, result.strategy_stats[SlotXYZWing], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_w_wing(st, result.strategy_stats[SlotWWing], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_jellyfish(st, result.strategy_stats[SlotJellyfish], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_x_chain(st, result.strategy_stats[SlotXChain], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_xy_chain(st, result.strategy_stats[SlotXYChain], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_wxyz_wing(st, result.strategy_stats[SlotWXYZWing], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_finned_swordfish_jellyfish(st, result.strategy_stats[SlotFinnedSwordfishJellyfish], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_als_xz(st, result.strategy_stats[SlotALSXZ], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_unique_loop(st, result.strategy_stats[SlotUniqueLoop], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_avoidable_rectangle(st, result.strategy_stats[SlotAvoidableRectangle], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_bivalue_oddagon(st, result.strategy_stats[SlotBivalueOddagon], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_medusa_3d(st, result.strategy_stats[SlotMedusa3D], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_aic(st, result.strategy_stats[SlotAIC], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_grouped_aic(st, result.strategy_stats[SlotGroupedAIC], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_grouped_x_cycle(st, result.strategy_stats[SlotGroupedXCycle], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_continuous_nice_loop(st, result.strategy_stats[SlotContinuousNiceLoop], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_als_xy_wing(st, result.strategy_stats[SlotALSXYWing], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_als_chain(st, result.strategy_stats[SlotALSChain], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_sue_de_coq(st, result.strategy_stats[SlotSueDeCoq], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_death_blossom(st, result.strategy_stats[SlotDeathBlossom], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_franken_fish(st, result.strategy_stats[SlotFrankenFish], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_mutant_fish(st, result.strategy_stats[SlotMutantFish], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_kraken_fish(st, result.strategy_stats[SlotKrakenFish], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_msls(st, result.strategy_stats[SlotMSLS], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_exocet(st, result.strategy_stats[SlotExocet], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_senior_exocet(st, result.strategy_stats[SlotSeniorExocet], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_sk_loop(st, result.strategy_stats[SlotSKLoop], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_pattern_overlay_method(st, result.strategy_stats[SlotPatternOverlayMethod], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;

            ar = apply_forcing_chains(st, result.strategy_stats[SlotForcingChains], result);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;
            break;
        }

        result.solved = (board.empty_cells == 0);
        if (capture_solution_grid) result.solved_grid = board.values;
        result.naked_single_scanned = result.strategy_stats[SlotNakedSingle].use_count > 0;
        result.hidden_single_scanned = result.strategy_stats[SlotHiddenSingle].use_count > 0;
        return result;
    }
};

} // namespace sudoku_hpc
