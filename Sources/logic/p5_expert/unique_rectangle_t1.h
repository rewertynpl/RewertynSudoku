// ============================================================================
// SUDOKU HPC - LOGIC ENGINE
// Moduł: unique_rectangle_t1.h (Poziom 5 - Expert)
// Opis: Wykrywa podstawowy typ Unique Rectangle (Type 1). 
//       Zapobiega powstaniu Dead Patternu (4 komórki bivalue w 2 blokach), 
//       eliminując kandydatów tworzących ten wzorzec z komórki celującej.
//       Zero-allocation - pętle rygorystycznie oparte na bitboardach.
// ============================================================================

#pragma once

#include <cstdint>
#include <algorithm>
#include <array>

#include "../../core/candidate_state.h"
#include "../../config/bit_utils.h"
#include "../logic_result.h"

namespace sudoku_hpc::logic::p5_expert {

inline ApplyResult apply_unique_rectangle(CandidateState& st, StrategyStats& s, GenericLogicCertifyResult& r) {
    const uint64_t t0 = st.now_ns();
    ++s.use_count;
    
    // Oparta o geometrię bloku, zatem odrzuca proste rury i wektory 1D
    if (st.topo->box_rows <= 1 || st.topo->box_cols <= 1) {
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::NoProgress;
    }
    
    const int n = st.topo->n;
    bool progress = false;
    
    // Szukamy potencjalnych par komórek, by stworzyć narożniki (corners)
    for (int r1 = 0; r1 < n; ++r1) {
        for (int r2 = r1 + 1; r2 < n; ++r2) {
            for (int c1 = 0; c1 < n; ++c1) {
                for (int c2 = c1 + 1; c2 < n; ++c2) {
                    const int a = r1 * n + c1;
                    const int b = r1 * n + c2;
                    const int c = r2 * n + c1;
                    const int d = r2 * n + c2;
                    
                    // Wszystkie narożniki muszą być nieodkryte
                    if (st.board->values[a] != 0 ||
                        st.board->values[b] != 0 ||
                        st.board->values[c] != 0 ||
                        st.board->values[d] != 0) {
                        continue;
                    }
                    
                    const std::array<int, 4> cells = {a, b, c, d};
                    const std::array<uint64_t, 4> masks = {
                        st.cands[a],
                        st.cands[b],
                        st.cands[c],
                        st.cands[d],
                    };
                    
                    // Muszą współdzielić tę samą parę jako bazę UR
                    const uint64_t pair = masks[0] & masks[1] & masks[2] & masks[3];
                    if (std::popcount(pair) != 2) continue;
                    
                    // Upewnienie się, że rdzeń cyfr bivalue znajduje się w każdym narożniku
                    bool valid_mask = true;
                    for (int i = 0; i < 4; ++i) {
                        if ((masks[i] & pair) != pair) {
                            valid_mask = false;
                            break;
                        }
                    }
                    if (!valid_mask) continue;
                    
                    // Aby był to poprawny Deadly Pattern zagrażający logice, 
                    // całe 4 komórki muszą dzielić DOKŁADNIE DWA BLOKI (2 boxes).
                    std::array<int, 4> boxes = {
                        st.topo->cell_box[a],
                        st.topo->cell_box[b],
                        st.topo->cell_box[c],
                        st.topo->cell_box[d],
                    };
                    
                    // Prosty trick na usunięcie duplikatów ze stacka 
                    // (zamiast std::set bez lokowania w 2 operacjach)
                    if (boxes[0] > boxes[1]) std::swap(boxes[0], boxes[1]);
                    if (boxes[1] > boxes[2]) std::swap(boxes[1], boxes[2]);
                    if (boxes[2] > boxes[3]) std::swap(boxes[2], boxes[3]);
                    if (boxes[0] > boxes[1]) std::swap(boxes[0], boxes[1]);
                    if (boxes[1] > boxes[2]) std::swap(boxes[1], boxes[2]);
                    if (boxes[0] > boxes[1]) std::swap(boxes[0], boxes[1]);
                    
                    int unique_boxes = 1;
                    for (int i = 1; i < 4; ++i) {
                        if (boxes[i] != boxes[i - 1]) ++unique_boxes;
                    }
                    if (unique_boxes != 2) continue;
                    
                    // Szukamy Type 1 (3 komórki to CZYSTE bivalue dla tej pary) 
                    // 4-ta komórka (target) posiada coś więcej. Z tej celującej komórki
                    // odcinamy `pair`, zapobiegając wytworzeniu martwego prostokąta.
                    int exact_pair_count = 0;
                    int target_idx = -1;
                    
                    for (int i = 0; i < 4; ++i) {
                        if (masks[i] == pair) {
                            ++exact_pair_count;
                        } else if (std::popcount(masks[i]) > 2) {
                            target_idx = cells[i];
                        }
                    }
                    
                    if (exact_pair_count == 3 && target_idx >= 0) {
                        const ApplyResult er = st.eliminate(target_idx, pair);
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
    
    if (progress) {
        ++s.hit_count;
        r.used_unique_rectangle = true;
        s.elapsed_ns += st.now_ns() - t0;
        return ApplyResult::Progress;
    }
    
    s.elapsed_ns += st.now_ns() - t0;
    return ApplyResult::NoProgress;
}

} // namespace sudoku_hpc::logic::p5_expert