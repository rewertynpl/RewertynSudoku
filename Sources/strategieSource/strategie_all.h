// ============================================================================
// SUDOKU HPC - WSZYSTKIE STRATEGIE
// Plik: strategie_all.h
// ============================================================================

#ifndef STRATEGIE_ALL_H
#define STRATEGIE_ALL_H

#include <memory>
#include "strategie_types.h"
#include "strategie_scratch.h"
#include "strategie_main.h"
#include "strategylevels1.h"
#include "strategylevels2.h"
#include "strategylevels3.h"
#include "strategylevels4.h"
#include "strategylevels5.h"
#include "strategylevels6.h"
#include "strategylevels7.h"
#include "strategylevels8.h"

namespace sudoku_strategie {

template<int N>
class UnifiedLogicCertify {
public:
    const TopologyCache<N>& topo;
    
    level1::StrategyLevel1<N> level1;
    level2::StrategyLevel2<N> level2;
    level3::StrategyLevel3<N> level3;
    level4::StrategyLevel4<N> level4;
    level5::StrategyLevel5<N> level5;
    level6::StrategyLevel6<N> level6;
    level7::StrategyLevel7<N> level7;
    level8::StrategyLevel8<N> level8;
    
    UnifiedLogicCertify(const TopologyCache<N>& t) 
        : topo(t), level1(t), level2(t), level3(t), level4(t), level5(t), level6(t), level7(t), level8(t)
    {}
    
    LogicCertifyResult certify(BoardSoA<N>& board, int difficulty_level, StrategyId required_strategy = static_cast<StrategyId>(0)) {
        LogicCertifyResult result{};
        
        if (difficulty_level >= 1) {
            while (!board.is_full()) {
                ApplyResult r = level1.apply_all(board, result);
                if (r == ApplyResult::Contradiction) { result.solved = false; return result; }
                if (r == ApplyResult::NoProgress) break;
            }
        }
        
        if (difficulty_level >= 2 && !board.is_full()) { ApplyResult r = level2.apply_all(board, result); (void)r; }
        if (difficulty_level >= 3 && !board.is_full()) { ApplyResult r = level3.apply_all(board, result); (void)r; }
        if (difficulty_level >= 4 && !board.is_full()) { ApplyResult r = level4.apply_all(board, result); (void)r; }
        if (difficulty_level >= 5 && !board.is_full()) { ApplyResult r = level5.apply_all(board, result); (void)r; }
        if (difficulty_level >= 6 && !board.is_full()) { ApplyResult r = level6.apply_all(board, result); (void)r; }
        if (difficulty_level >= 7 && !board.is_full()) { ApplyResult r = level7.apply_all(board, result); (void)r; }
        if (difficulty_level >= 8 && !board.is_full()) { ApplyResult r = level8.apply_all(board, result); (void)r; }
        
        result.solved = board.is_full();
        
        if (required_strategy != static_cast<StrategyId>(0)) {
            bool strategy_found = check_strategy_used(result, required_strategy);
            if (!strategy_found) result.solved = false;
        }
        
        return result;
    }
    
    bool check_strategy_used(const LogicCertifyResult& result, StrategyId strategy) const {
        switch (strategy) {
            case StrategyId::NakedSingle: return result.used_naked_single;
            case StrategyId::HiddenSingle: return result.used_hidden_single;
            case StrategyId::PointingPairs:
            case StrategyId::PointingTriples:
            case StrategyId::BoxLineReduction: return result.used_box_line;
            case StrategyId::NakedPair: return result.used_naked_pair;
            case StrategyId::HiddenPair: return result.used_hidden_pair;
            case StrategyId::NakedTriple: return result.used_naked_triple;
            case StrategyId::HiddenTriple: return result.used_hidden_triple;
            case StrategyId::XWing: return result.used_x_wing;
            case StrategyId::Swordfish: return result.used_swordfish;
            default: return false;
        }
    }
};

template<int N>
std::unique_ptr<UnifiedLogicCertify<N>> create_certifier(const TopologyCache<N>& topo) {
    return std::make_unique<UnifiedLogicCertify<N>>(topo);
}

} // namespace sudoku_strategie

#endif // STRATEGIE_ALL_H