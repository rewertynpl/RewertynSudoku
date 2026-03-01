
// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// ModuĹ‚: sudoku_logic_engine.h
// Opis: Centralny dyspozytor logiki (Fasada). Deleguje testy Ĺ‚amigĹ‚Ăłwki 
//       do poszczegĂłlnych moduĹ‚Ăłw. ĹÄ…czy nowÄ…, rozbitÄ… architekturÄ™ w caĹ‚oĹ›Ä‡.
//       Gwarantuje wywoĹ‚ania strategii od Poziomu 1 (Easy) do Poziomu 8 
//       (Theoretical) bez ominiÄ™cia ĹĽadnego wariantu. Zero-Allocation.
// ============================================================================
//Author copyright Marcin Matysek (Rewertyn)

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "../config/run_config.h"
#include "../core/board.h"
#include "../core/candidate_state.h"
#include "../generator/core_engines/dlx_solver.h" // dla SearchAbortControl
#include "logic_result.h"

// ============================================================================
// DOĹÄ„CZENIE WSZYSTKICH SPECJALISTYCZNYCH MODUĹĂ“W (P1 - P8)
// ============================================================================
#include "p1_easy/naked_hidden_single.h"
#include "p2_intersections/intersections.h"
#include "p3_subsets/house_subsets.h"
#include "p4_hard/fish_basic.h"
#include "p4_hard/skyscraper_kite.h"
#include "p4_hard/empty_rectangle.h"
#include "p4_hard/y_wing_remote_pairs.h"
#include "p5_expert/finned_fish.h"
#include "p5_expert/simple_coloring.h"
#include "p5_expert/bug_plus_one.h"
#include "p5_expert/unique_rectangle_t1.h"
#include "p5_expert/xyz_w_wing.h"
#include "p6_diabolical/finned_jelly_sword.h" // Zawiera Jellyfish
#include "p6_diabolical/chains_basic.h"
#include "p6_diabolical/wxyz_wing.h"
#include "p6_diabolical/als_xz.h"
#include "p6_diabolical/unique_loop_avoidable_oddagon.h" // Nowe uzupeĹ‚nienia
#include "p6_diabolical/ur_extended.h"
#include "p6_diabolical/bug_variants.h"
#include "p6_diabolical/borescoper_qiu.h"
#include "p7_nightmare/medusa_3d.h"
#include "p7_nightmare/aic_grouped_aic.h"
#include "p7_nightmare/grouped_x_cycle.h"
#include "p7_nightmare/continuous_nice_loop.h"
#include "p7_nightmare/als_xy_wing_chain.h" // Zawiera ALS-AIC
#include "p7_nightmare/sue_de_coq.h"
#include "p7_nightmare/death_blossom.h"
#include "p7_nightmare/franken_mutant_fish.h"
#include "p7_nightmare/kraken_fish.h"
#include "p7_nightmare/squirmbag.h" // Nowa ryba 5x5
#include "p7_nightmare/aligned_exclusion.h"
#include "p8_theoretical/msls.h"
#include "p8_theoretical/exocet_family.h"
#include "p8_theoretical/sk_loop.h"
#include "p8_theoretical/pattern_overlay.h"
#include "p8_theoretical/forcing_chains_dynamic.h"

namespace sudoku_hpc::logic {

struct GenericLogicCertify {
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
        SlotForcingChains = 49,
        SlotSquirmbag = 50,
        SlotURExtended = 51,
        SlotHiddenUR = 52,
        SlotBUGType2 = 53,
        SlotBUGType3 = 54,
        SlotBUGType4 = 55,
        SlotBorescoperQiuDeadlyPattern = 56,
        SlotAlignedPairExclusion = 57,
        SlotAlignedTripleExclusion = 58,
        SlotALSAIC = 59,
        SlotDynamicForcingChains = 60
    };

    struct StrategyMeta {
        const char* id = "unknown";
        uint8_t level = 0;
        StrategyImplTier impl_tier = StrategyImplTier::Disabled;
        bool supports_asymmetric = true;
        uint8_t max_n_verified = 64;
        const char* proof_tag = "none";
    };

    static const StrategyMeta& strategy_meta_for_slot(size_t slot) {
        static constexpr std::array<StrategyMeta, kStrategySlotCount> kMeta = {{
            {"NakedSingle", 1, StrategyImplTier::Full, true, 64, "P1.NakedSingle"},
            {"HiddenSingle", 1, StrategyImplTier::Full, true, 64, "P1.HiddenSingle"},
            {"PointingPairs", 2, StrategyImplTier::Full, true, 64, "P2.PointingPairs"},
            {"BoxLineReduction", 2, StrategyImplTier::Full, true, 64, "P2.BoxLineReduction"},
            {"NakedPair", 2, StrategyImplTier::Full, true, 64, "P2.NakedPair"},
            {"HiddenPair", 2, StrategyImplTier::Full, true, 64, "P2.HiddenPair"},
            {"NakedTriple", 2, StrategyImplTier::Full, true, 64, "P2.NakedTriple"},
            {"HiddenTriple", 2, StrategyImplTier::Full, true, 64, "P2.HiddenTriple"},
            {"NakedQuad", 4, StrategyImplTier::Full, true, 64, "P3.NakedQuad"},
            {"HiddenQuad", 4, StrategyImplTier::Full, true, 64, "P3.HiddenQuad"},
            {"XWing", 4, StrategyImplTier::Full, true, 64, "P3.XWing"},
            {"YWing", 4, StrategyImplTier::Full, true, 64, "P3.YWing"},
            {"Skyscraper", 4, StrategyImplTier::Full, true, 64, "P3.Skyscraper"},
            {"TwoStringKite", 4, StrategyImplTier::Full, true, 64, "P3.TwoStringKite"},
            {"EmptyRectangle", 4, StrategyImplTier::Full, true, 64, "P3.EmptyRectangle"},
            {"RemotePairs", 4, StrategyImplTier::Full, true, 64, "P3.RemotePairs"},
            {"Swordfish", 5, StrategyImplTier::Full, true, 64, "P4.Swordfish"},
            {"FinnedXWingSashimi", 5, StrategyImplTier::Full, true, 64, "P4.FinnedXWingSashimi"},
            {"SimpleColoring", 5, StrategyImplTier::Full, true, 64, "P4.SimpleColoring"},
            {"BUGPlusOne", 5, StrategyImplTier::Full, true, 64, "P4.BUGPlusOne"},
            {"UniqueRectangle", 5, StrategyImplTier::Full, true, 64, "P4.UniqueRectangle"},
            {"XYZWing", 5, StrategyImplTier::Full, true, 64, "P4.XYZWing"},
            {"WWing", 5, StrategyImplTier::Full, true, 64, "P4.WWing"},
            {"Jellyfish", 6, StrategyImplTier::Full, true, 64, "P5.Jellyfish"},
            {"XChain", 6, StrategyImplTier::Full, true, 64, "P5.XChain"},
            {"XYChain", 6, StrategyImplTier::Full, true, 64, "P5.XYChain"},
            {"WXYZWing", 6, StrategyImplTier::Full, true, 64, "P5.WXYZWing"},
            {"FinnedSwordfishJellyfish", 6, StrategyImplTier::Full, true, 64, "P5.FinnedSwordfishJellyfish"},
            {"ALSXZ", 6, StrategyImplTier::Full, true, 64, "P5.ALSXZ"},
            {"UniqueLoop", 6, StrategyImplTier::Full, true, 64, "P5.UniqueLoop"},
            {"AvoidableRectangle", 6, StrategyImplTier::Full, true, 64, "P5.AvoidableRectangle"},
            {"BivalueOddagon", 6, StrategyImplTier::Full, true, 64, "P5.BivalueOddagon"},
            {"Medusa3D", 7, StrategyImplTier::Hybrid, true, 64, "P6.Medusa3D"},
            {"AIC", 7, StrategyImplTier::Full, true, 64, "P6.AIC"},
            {"GroupedAIC", 7, StrategyImplTier::Full, true, 64, "P6.GroupedAIC"},
            {"GroupedXCycle", 7, StrategyImplTier::Full, true, 64, "P6.GroupedXCycle"},
            {"ContinuousNiceLoop", 7, StrategyImplTier::Full, true, 64, "P6.ContinuousNiceLoop"},
            {"ALSXYWing", 7, StrategyImplTier::Hybrid, true, 64, "P6.ALSXYWing"},
            {"ALSChain", 7, StrategyImplTier::Hybrid, true, 64, "P6.ALSChain"},
            {"SueDeCoq", 7, StrategyImplTier::Hybrid, true, 64, "P6.SueDeCoq"},
            {"DeathBlossom", 7, StrategyImplTier::Hybrid, true, 64, "P6.DeathBlossom"},
            {"FrankenFish", 7, StrategyImplTier::Full, true, 64, "P6.FrankenFish"},
            {"MutantFish", 7, StrategyImplTier::Full, true, 64, "P6.MutantFish"},
            {"KrakenFish", 7, StrategyImplTier::Hybrid, true, 64, "P6.KrakenFish"},
            {"MSLS", 8, StrategyImplTier::Hybrid, true, 64, "P7.MSLS"},
            {"Exocet", 8, StrategyImplTier::Hybrid, true, 64, "P7.Exocet"},
            {"SeniorExocet", 8, StrategyImplTier::Hybrid, true, 64, "P7.SeniorExocet"},
            {"SKLoop", 8, StrategyImplTier::Hybrid, true, 64, "P7.SKLoop"},
            {"PatternOverlayMethod", 8, StrategyImplTier::Hybrid, true, 64, "P7.PatternOverlay"},
            {"ForcingChains", 8, StrategyImplTier::Hybrid, true, 64, "P7.ForcingChains"},
            {"Squirmbag", 7, StrategyImplTier::Hybrid, true, 64, "P6.Squirmbag"},
            {"URExtended", 6, StrategyImplTier::Full, true, 64, "P5.URExtended"},
            {"HiddenUR", 6, StrategyImplTier::Full, true, 64, "P5.HiddenUR"},
            {"BUGType2", 6, StrategyImplTier::Full, true, 64, "P5.BUGType2"},
            {"BUGType3", 6, StrategyImplTier::Full, true, 64, "P5.BUGType3"},
            {"BUGType4", 6, StrategyImplTier::Full, true, 64, "P5.BUGType4"},
            {"BorescoperQiuDeadlyPattern", 6, StrategyImplTier::Full, true, 64, "P5.BorescoperQiu"},
            {"AlignedPairExclusion", 7, StrategyImplTier::Hybrid, true, 64, "P6.AlignedPairExclusion"},
            {"AlignedTripleExclusion", 7, StrategyImplTier::Hybrid, true, 64, "P6.AlignedTripleExclusion"},
            {"ALSAIC", 7, StrategyImplTier::Hybrid, true, 64, "P6.ALSAIC"},
            {"DynamicForcingChains", 8, StrategyImplTier::Hybrid, true, 64, "P7.DynamicForcingChains"},
        }};
        if (slot >= kMeta.size()) {
            static constexpr StrategyMeta kInvalid{"invalid", 0, StrategyImplTier::Disabled, false, 0, "invalid"};
            return kInvalid;
        }
        return kMeta[slot];
    }

    static StrategyImplTier strategy_impl_tier_for_slot(size_t slot) {
        return strategy_meta_for_slot(slot).impl_tier;
    }

    static bool is_proxy_slot(size_t slot) {
        const StrategyImplTier tier = strategy_impl_tier_for_slot(slot);
        return tier == StrategyImplTier::Proxy || tier == StrategyImplTier::Disabled;
    }

    static bool is_full_canonical_slot(size_t slot) {
        return strategy_impl_tier_for_slot(slot) == StrategyImplTier::Full;
    }

    static bool slot_from_required_strategy(RequiredStrategy rs, size_t& out_slot) {
        switch (rs) {
            case RequiredStrategy::NakedSingle: out_slot = SlotNakedSingle; return true;
            case RequiredStrategy::HiddenSingle: out_slot = SlotHiddenSingle; return true;
            case RequiredStrategy::PointingPairs: out_slot = SlotPointingPairs; return true;
            case RequiredStrategy::BoxLineReduction: out_slot = SlotBoxLineReduction; return true;
            case RequiredStrategy::NakedPair: out_slot = SlotNakedPair; return true;
            case RequiredStrategy::HiddenPair: out_slot = SlotHiddenPair; return true;
            case RequiredStrategy::NakedTriple: out_slot = SlotNakedTriple; return true;
            case RequiredStrategy::HiddenTriple: out_slot = SlotHiddenTriple; return true;
            case RequiredStrategy::NakedQuad: out_slot = SlotNakedQuad; return true;
            case RequiredStrategy::HiddenQuad: out_slot = SlotHiddenQuad; return true;
            case RequiredStrategy::XWing: out_slot = SlotXWing; return true;
            case RequiredStrategy::YWing: out_slot = SlotYWing; return true;
            case RequiredStrategy::Skyscraper: out_slot = SlotSkyscraper; return true;
            case RequiredStrategy::TwoStringKite: out_slot = SlotTwoStringKite; return true;
            case RequiredStrategy::EmptyRectangle: out_slot = SlotEmptyRectangle; return true;
            case RequiredStrategy::RemotePairs: out_slot = SlotRemotePairs; return true;
            case RequiredStrategy::Swordfish: out_slot = SlotSwordfish; return true;
            case RequiredStrategy::FinnedXWingSashimi: out_slot = SlotFinnedXWingSashimi; return true;
            case RequiredStrategy::SimpleColoring: out_slot = SlotSimpleColoring; return true;
            case RequiredStrategy::BUGPlusOne: out_slot = SlotBUGPlusOne; return true;
            case RequiredStrategy::UniqueRectangle: out_slot = SlotUniqueRectangle; return true;
            case RequiredStrategy::XYZWing: out_slot = SlotXYZWing; return true;
            case RequiredStrategy::WWing: out_slot = SlotWWing; return true;
            case RequiredStrategy::Jellyfish: out_slot = SlotJellyfish; return true;
            case RequiredStrategy::XChain: out_slot = SlotXChain; return true;
            case RequiredStrategy::XYChain: out_slot = SlotXYChain; return true;
            case RequiredStrategy::WXYZWing: out_slot = SlotWXYZWing; return true;
            case RequiredStrategy::FinnedSwordfishJellyfish: out_slot = SlotFinnedSwordfishJellyfish; return true;
            case RequiredStrategy::ALSXZ: out_slot = SlotALSXZ; return true;
            case RequiredStrategy::UniqueLoop: out_slot = SlotUniqueLoop; return true;
            case RequiredStrategy::AvoidableRectangle: out_slot = SlotAvoidableRectangle; return true;
            case RequiredStrategy::BivalueOddagon: out_slot = SlotBivalueOddagon; return true;
            case RequiredStrategy::UniqueRectangleExtended: out_slot = SlotURExtended; return true;
            case RequiredStrategy::HiddenUniqueRectangle: out_slot = SlotHiddenUR; return true;
            case RequiredStrategy::BUGType2: out_slot = SlotBUGType2; return true;
            case RequiredStrategy::BUGType3: out_slot = SlotBUGType3; return true;
            case RequiredStrategy::BUGType4: out_slot = SlotBUGType4; return true;
            case RequiredStrategy::BorescoperQiuDeadlyPattern: out_slot = SlotBorescoperQiuDeadlyPattern; return true;
            case RequiredStrategy::Medusa3D: out_slot = SlotMedusa3D; return true;
            case RequiredStrategy::AIC: out_slot = SlotAIC; return true;
            case RequiredStrategy::GroupedAIC: out_slot = SlotGroupedAIC; return true;
            case RequiredStrategy::GroupedXCycle: out_slot = SlotGroupedXCycle; return true;
            case RequiredStrategy::ContinuousNiceLoop: out_slot = SlotContinuousNiceLoop; return true;
            case RequiredStrategy::ALSXYWing: out_slot = SlotALSXYWing; return true;
            case RequiredStrategy::ALSChain: out_slot = SlotALSChain; return true;
            case RequiredStrategy::AlignedPairExclusion: out_slot = SlotAlignedPairExclusion; return true;
            case RequiredStrategy::AlignedTripleExclusion: out_slot = SlotAlignedTripleExclusion; return true;
            case RequiredStrategy::ALSAIC: out_slot = SlotALSAIC; return true;
            case RequiredStrategy::SueDeCoq: out_slot = SlotSueDeCoq; return true;
            case RequiredStrategy::DeathBlossom: out_slot = SlotDeathBlossom; return true;
            case RequiredStrategy::FrankenFish: out_slot = SlotFrankenFish; return true;
            case RequiredStrategy::MutantFish: out_slot = SlotMutantFish; return true;
            case RequiredStrategy::KrakenFish: out_slot = SlotKrakenFish; return true;
            case RequiredStrategy::Squirmbag: out_slot = SlotSquirmbag; return true;
            case RequiredStrategy::MSLS: out_slot = SlotMSLS; return true;
            case RequiredStrategy::Exocet: out_slot = SlotExocet; return true;
            case RequiredStrategy::SeniorExocet: out_slot = SlotSeniorExocet; return true;
            case RequiredStrategy::SKLoop: out_slot = SlotSKLoop; return true;
            case RequiredStrategy::PatternOverlayMethod: out_slot = SlotPatternOverlayMethod; return true;
            case RequiredStrategy::ForcingChains: out_slot = SlotForcingChains; return true;
            case RequiredStrategy::DynamicForcingChains: out_slot = SlotDynamicForcingChains; return true;
            case RequiredStrategy::None:
            case RequiredStrategy::Backtracking:
                return false;
        }
        return false;
    }

private:
    static inline void note_strategy_slot(GenericLogicCertifyResult& result, size_t slot, ApplyResult ar) {
        if (ar == ApplyResult::NoProgress) {
            return;
        }
        const StrategyMeta& meta = strategy_meta_for_slot(slot);
        result.record_step(
            static_cast<uint16_t>(slot),
            meta.impl_tier,
            ar,
            0,
            0,
            meta.proof_tag);
    }

    // GĹĂ“WNA PÄTLA CERTYFIKATORA LOGICZNEGO
    static ApplyResult apply_round_up_to_level(CandidateState& st, GenericLogicCertifyResult& result, int max_level) {
        
        // ====================================================================
        // POZIOM 1: EASY
        // ====================================================================
        ApplyResult ar = p1_easy::apply_naked_single(st, result.strategy_stats[SlotNakedSingle], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotNakedSingle, ar); return ar; }
        ar = p1_easy::apply_hidden_single(st, result.strategy_stats[SlotHiddenSingle], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotHiddenSingle, ar); return ar; }
        
        if (max_level <= 1) return ApplyResult::NoProgress;

        // ====================================================================
        // POZIOM 2: MEDIUM
        // ====================================================================
        ar = p2_intersections::apply_pointing_and_boxline(st, result.strategy_stats[SlotPointingPairs], result.strategy_stats[SlotBoxLineReduction], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotPointingPairs, ar); return ar; }
        
        // Zgodnie z oficjalnÄ… klasyfikacjÄ…: Podzbiory 2, 3 elementowe wchodzÄ… tu jako medium
        ar = p3_subsets::apply_house_subset(st, result.strategy_stats[SlotNakedPair], result, 2, false);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotNakedPair, ar); return ar; }
        ar = p3_subsets::apply_house_subset(st, result.strategy_stats[SlotHiddenPair], result, 2, true);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotHiddenPair, ar); return ar; }
        ar = p3_subsets::apply_house_subset(st, result.strategy_stats[SlotNakedTriple], result, 3, false);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotNakedTriple, ar); return ar; }
        ar = p3_subsets::apply_house_subset(st, result.strategy_stats[SlotHiddenTriple], result, 3, true);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotHiddenTriple, ar); return ar; }

        if (max_level <= 2) return ApplyResult::NoProgress;

        // ====================================================================
        // POZIOM 3/4: HARD / EXPERT (Wg wytycznych poĹ‚Ä…czone jako P3/P4 w silniku)
        // ====================================================================
        ar = p3_subsets::apply_house_subset(st, result.strategy_stats[SlotNakedQuad], result, 4, false);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotNakedQuad, ar); return ar; }
        ar = p3_subsets::apply_house_subset(st, result.strategy_stats[SlotHiddenQuad], result, 4, true);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotHiddenQuad, ar); return ar; }
        
        ar = p4_hard::apply_x_wing(st, result.strategy_stats[SlotXWing], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotXWing, ar); return ar; }
        ar = p4_hard::apply_y_wing(st, result.strategy_stats[SlotYWing], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotYWing, ar); return ar; }
        ar = p4_hard::apply_skyscraper(st, result.strategy_stats[SlotSkyscraper], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSkyscraper, ar); return ar; }
        ar = p4_hard::apply_two_string_kite(st, result.strategy_stats[SlotTwoStringKite], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotTwoStringKite, ar); return ar; }
        ar = p4_hard::apply_empty_rectangle(st, result.strategy_stats[SlotEmptyRectangle], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotEmptyRectangle, ar); return ar; }
        ar = p4_hard::apply_remote_pairs(st, result.strategy_stats[SlotRemotePairs], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotRemotePairs, ar); return ar; }
        
        if (max_level <= 4) return ApplyResult::NoProgress;

        // ====================================================================
        // POZIOM 5: DIABOLICAL / EXPERT (ZĹ‚oĹĽone Ryby, W-Wing, Coloring)
        // ====================================================================
        ar = p4_hard::apply_swordfish(st, result.strategy_stats[SlotSwordfish], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSwordfish, ar); return ar; }
        ar = p5_expert::apply_finned_x_wing_sashimi(st, result.strategy_stats[SlotFinnedXWingSashimi], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotFinnedXWingSashimi, ar); return ar; }
        ar = p5_expert::apply_simple_coloring(st, result.strategy_stats[SlotSimpleColoring], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSimpleColoring, ar); return ar; }
        ar = p5_expert::apply_bug_plus_one(st, result.strategy_stats[SlotBUGPlusOne], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotBUGPlusOne, ar); return ar; }
        ar = p5_expert::apply_unique_rectangle(st, result.strategy_stats[SlotUniqueRectangle], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotUniqueRectangle, ar); return ar; }
        ar = p5_expert::apply_xyz_wing(st, result.strategy_stats[SlotXYZWing], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotXYZWing, ar); return ar; }
        ar = p5_expert::apply_w_wing(st, result.strategy_stats[SlotWWing], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotWWing, ar); return ar; }
        
        if (max_level <= 5) return ApplyResult::NoProgress;

        // ====================================================================
        // POZIOM 6: NIGHTMARE / DIABOLICAL (ĹaĹ„cuchy, Rozszerzone Deadly Patterns)
        // ====================================================================
        ar = p6_diabolical::apply_jellyfish(st, result.strategy_stats[SlotJellyfish], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotJellyfish, ar); return ar; }
        ar = p6_diabolical::apply_x_chain(st, result.strategy_stats[SlotXChain], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotXChain, ar); return ar; }
        ar = p6_diabolical::apply_xy_chain(st, result.strategy_stats[SlotXYChain], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotXYChain, ar); return ar; }
        ar = p6_diabolical::apply_wxyz_wing(st, result.strategy_stats[SlotWXYZWing], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotWXYZWing, ar); return ar; }
        ar = p6_diabolical::apply_finned_swordfish_jellyfish(st, result.strategy_stats[SlotFinnedSwordfishJellyfish], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotFinnedSwordfishJellyfish, ar); return ar; }
        ar = p6_diabolical::apply_als_xz(st, result.strategy_stats[SlotALSXZ], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotALSXZ, ar); return ar; }
        
        // NOWE PODPIÄCIA - UzupeĹ‚nione Deadly Patterns
        ar = p6_diabolical::apply_unique_loop(st, result.strategy_stats[SlotUniqueLoop], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotUniqueLoop, ar); return ar; }
        ar = p6_diabolical::apply_avoidable_rectangle(st, result.strategy_stats[SlotAvoidableRectangle], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotAvoidableRectangle, ar); return ar; }
        ar = p6_diabolical::apply_bivalue_oddagon(st, result.strategy_stats[SlotBivalueOddagon], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotBivalueOddagon, ar); return ar; }
        
        ar = p6_diabolical::apply_ur_extended(st, result.strategy_stats[SlotURExtended], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotURExtended, ar); return ar; }
        ar = p6_diabolical::apply_hidden_ur(st, result.strategy_stats[SlotHiddenUR], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotHiddenUR, ar); return ar; }
        ar = p6_diabolical::apply_bug_type2(st, result.strategy_stats[SlotBUGType2], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotBUGType2, ar); return ar; }
        ar = p6_diabolical::apply_bug_type3(st, result.strategy_stats[SlotBUGType3], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotBUGType3, ar); return ar; }
        ar = p6_diabolical::apply_bug_type4(st, result.strategy_stats[SlotBUGType4], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotBUGType4, ar); return ar; }
        ar = p6_diabolical::apply_borescoper_qiu_deadly_pattern(st, result.strategy_stats[SlotBorescoperQiuDeadlyPattern], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotBorescoperQiuDeadlyPattern, ar); return ar; }
        
        if (max_level <= 6) return ApplyResult::NoProgress;

        // ====================================================================
        // POZIOM 7: NIGHTMARE / THEORETICAL (Grupy ALS, Mutanty, APE)
        // ====================================================================
        ar = p7_nightmare::apply_medusa_3d(st, result.strategy_stats[SlotMedusa3D], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotMedusa3D, ar); return ar; }
        ar = p7_nightmare::apply_aic(st, result.strategy_stats[SlotAIC], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotAIC, ar); return ar; }
        ar = p7_nightmare::apply_grouped_aic(st, result.strategy_stats[SlotGroupedAIC], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotGroupedAIC, ar); return ar; }
        
        // PODPIÄCIA BRAKUJÄ„CYCH "SIEROT" P7
        ar = p7_nightmare::apply_grouped_x_cycle(st, result.strategy_stats[SlotGroupedXCycle], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotGroupedXCycle, ar); return ar; }
        ar = p7_nightmare::apply_continuous_nice_loop(st, result.strategy_stats[SlotContinuousNiceLoop], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotContinuousNiceLoop, ar); return ar; }
        ar = p7_nightmare::apply_als_xy_wing(st, result.strategy_stats[SlotALSXYWing], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotALSXYWing, ar); return ar; }
        ar = p7_nightmare::apply_als_chain(st, result.strategy_stats[SlotALSChain], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotALSChain, ar); return ar; }
        
        ar = p7_nightmare::apply_sue_de_coq(st, result.strategy_stats[SlotSueDeCoq], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSueDeCoq, ar); return ar; }
        
        // PODPIÄCIA BRAKUJÄ„CYCH "SIEROT" P7 cd.
        ar = p7_nightmare::apply_death_blossom(st, result.strategy_stats[SlotDeathBlossom], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotDeathBlossom, ar); return ar; }
        ar = p7_nightmare::apply_franken_fish(st, result.strategy_stats[SlotFrankenFish], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotFrankenFish, ar); return ar; }
        ar = p7_nightmare::apply_mutant_fish(st, result.strategy_stats[SlotMutantFish], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotMutantFish, ar); return ar; }
        ar = p7_nightmare::apply_kraken_fish(st, result.strategy_stats[SlotKrakenFish], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotKrakenFish, ar); return ar; }
        
        // NOWA RYBA
        ar = p7_nightmare::apply_squirmbag(st, result.strategy_stats[SlotSquirmbag], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSquirmbag, ar); return ar; }
        
        ar = p7_nightmare::apply_aligned_pair_exclusion(st, result.strategy_stats[SlotAlignedPairExclusion], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotAlignedPairExclusion, ar); return ar; }
        ar = p7_nightmare::apply_aligned_triple_exclusion(st, result.strategy_stats[SlotAlignedTripleExclusion], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotAlignedTripleExclusion, ar); return ar; }
        
        // PODPIÄCIE ALS-AIC
        ar = p7_nightmare::apply_als_aic(st, result.strategy_stats[SlotALSAIC], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotALSAIC, ar); return ar; }
        
        if (max_level <= 7) return ApplyResult::NoProgress;

        // ====================================================================
        // POZIOM 8: THEORETICAL / BRUTE FORCE (Wzorcowe Maski, MSLS)
        // ====================================================================
        
        // PODPIÄCIA BRAKUJÄ„CYCH "SIEROT" P8
        ar = p8_theoretical::apply_msls(st, result.strategy_stats[SlotMSLS], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotMSLS, ar); return ar; }
        ar = p8_theoretical::apply_exocet(st, result.strategy_stats[SlotExocet], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotExocet, ar); return ar; }
        ar = p8_theoretical::apply_senior_exocet(st, result.strategy_stats[SlotSeniorExocet], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSeniorExocet, ar); return ar; }
        ar = p8_theoretical::apply_sk_loop(st, result.strategy_stats[SlotSKLoop], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotSKLoop, ar); return ar; }
        ar = p8_theoretical::apply_pattern_overlay_method(st, result.strategy_stats[SlotPatternOverlayMethod], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotPatternOverlayMethod, ar); return ar; }
        
        ar = p8_theoretical::apply_forcing_chains(st, result.strategy_stats[SlotForcingChains], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotForcingChains, ar); return ar; }
        ar = p8_theoretical::apply_dynamic_forcing_chains(st, result.strategy_stats[SlotDynamicForcingChains], result);
        if (ar != ApplyResult::NoProgress) { note_strategy_slot(result, SlotDynamicForcingChains, ar); return ar; }

        return ApplyResult::NoProgress;
    }

public:
    GenericLogicCertifyResult certify(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        core_engines::SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        return certify_up_to_level(puzzle, topo, 8, budget, capture_solution_grid);
    }

    GenericLogicCertifyResult certify_up_to_level(
        const std::vector<uint16_t>& puzzle,
        const GenericTopology& topo,
        int max_level,
        core_engines::SearchAbortControl* budget = nullptr,
        bool capture_solution_grid = false) const {
        
        GenericLogicCertifyResult result{};
        const bool has_budget = (budget != nullptr);
        const int level_limit = std::clamp(max_level, 1, 8);

        // Bufor dla planszy (zero-alloc miÄ™dzy wykonaniami poszczegĂłlnych komĂłrek)
        static thread_local GenericBoard board;
        board.topo = &topo;
        if (!board.init_from_puzzle(puzzle, false)) return result;

        // PĹ‚aski bufor dla tablicy masek (zero-alloc), wprowadzany do CandidateState
        // Zapas 4096 pozwala na bezproblemowÄ… operacjÄ™ na ekstremalnych siatkach (np. 64x64).
        static thread_local uint64_t tls_cands[4096];
        
        CandidateState st{};
        if (!st.init(board, topo, tls_cands)) return result;

        // GĹ‚Ăłwna pÄ™tla dyspozytora. KaĹĽdy powrĂłt "Progress" sprawia, ĹĽe zaczynamy 
        // przeczesywaÄ‡ strategie od najszybszych i najprostszych (P1).
        while (board.empty_cells != 0) {
            if (has_budget && !budget->step()) {
                result.timed_out = true;
                result.solved = false;
                return result;
            }
            
            const ApplyResult ar = apply_round_up_to_level(st, result, level_limit);
            if (ar == ApplyResult::Contradiction) return result;
            if (ar == ApplyResult::Progress) continue;
            
            // Ĺ»adna ze strategii na dozwolonym poziomie nie odnalazĹ‚a dedukcji (WÄ…skie gardĹ‚o nierozwiÄ…zane)
            break;
        }

        result.solved = (board.empty_cells == 0);
        if (capture_solution_grid) {
            result.solved_grid = board.values;
        }
        
        // Zapis flag dla testĂłw mikro-profilujÄ…cych
        result.naked_single_scanned = result.strategy_stats[SlotNakedSingle].use_count > 0;
        result.hidden_single_scanned = result.strategy_stats[SlotHiddenSingle].use_count > 0;
        
        return result;
    }
};

} // namespace sudoku_hpc::logic



