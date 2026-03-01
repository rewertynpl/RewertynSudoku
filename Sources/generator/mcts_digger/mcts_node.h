// ============================================================================
// SUDOKU HPC - MCTS DIGGER
// Moduł: mcts_node.h
// Opis: Reprezentuje węzeł drzewa Monte Carlo. Prealokowany w TLS.
//       Służy do oceny "wartości" usunięcia konkretnej komórki (reward).
// ============================================================================

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace sudoku_hpc::mcts_digger {

// Struktura bufora przetrzymująca statystyki na danym etapie poszukiwań
// Utrzymuje do MAX_NN komórek jako dostępne akcje (usunięcia).
struct MctsNodeScratch {
    int prepared_nn = 0;
    
    // Tablice równoległe (SoA) zastępujące alokowane węzły
    std::vector<double> reward_sum;
    std::vector<uint32_t> visits;
    std::vector<int> active_cells;
    std::vector<int> active_pos; // Mapowanie: O(1) index w active_cells dla danej komórki
    
    int active_count = 0;
    uint64_t total_visits = 0;

    void ensure(int nn) {
        if (prepared_nn == nn) {
            return;
        }
        reward_sum.assign(static_cast<size_t>(nn), 0.0);
        visits.assign(static_cast<size_t>(nn), 0U);
        active_cells.assign(static_cast<size_t>(nn), -1);
        active_pos.assign(static_cast<size_t>(nn), -1);
        prepared_nn = nn;
        active_count = 0;
        total_visits = 0;
    }

    void reset(int nn) {
        ensure(nn);
        std::fill(reward_sum.begin(), reward_sum.end(), 0.0);
        std::fill(visits.begin(), visits.end(), 0U);
        std::fill(active_pos.begin(), active_pos.end(), -1);
        active_count = 0;
        total_visits = 0;
    }

    // Aktywuje komórkę jako możliwy cel usunięcia ("odnoga" w drzewie)
    void activate(int cell) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        if (active_pos[static_cast<size_t>(cell)] >= 0) {
            return; // Już aktywna
        }
        
        const int pos = active_count++;
        active_cells[static_cast<size_t>(pos)] = cell;
        active_pos[static_cast<size_t>(cell)] = pos;
    }

    // Usuwa komórkę z dostępnych odnóg w O(1) swap_and_pop
    void disable(int cell) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        const int pos = active_pos[static_cast<size_t>(cell)];
        if (pos < 0 || pos >= active_count) {
            return;
        }
        
        const int last_pos = active_count - 1;
        const int last_cell = active_cells[static_cast<size_t>(last_pos)];
        
        // Zastąpienie usuwanego elementu ostatnim elementem
        active_cells[static_cast<size_t>(pos)] = last_cell;
        if (last_cell >= 0) {
            active_pos[static_cast<size_t>(last_cell)] = pos;
        }
        
        // Wyczyszczenie ogona
        active_cells[static_cast<size_t>(last_pos)] = -1;
        active_pos[static_cast<size_t>(cell)] = -1;
        --active_count;
    }

    // Aktualizacja statystyk węzła na podstawie sygnału z Backpropagation
    void update(int cell, double reward) {
        if (cell < 0 || cell >= prepared_nn) {
            return;
        }
        reward_sum[static_cast<size_t>(cell)] += reward;
        visits[static_cast<size_t>(cell)] += 1U;
        ++total_visits;
    }
};

// TLS dostęp (Gwarancja uniknięcia alokacji w głównej pętli MCTS Diggera)
inline MctsNodeScratch& tls_mcts_node_scratch() {
    thread_local MctsNodeScratch s{};
    return s;
}

} // namespace sudoku_hpc::mcts_digger