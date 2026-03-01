// ============================================================================
// SUDOKU HPC - LOGIC ENGINE SHARED
// Moduł: exact_pattern_scratchpad.h
// Opis: Mózg Zero-Allocation Policy dla całego silnika Certyfikatora.
//       Płaskie bufory Thread-Local dla wszystkich grafów, kolejek i tablic.
//       Rozmiary stałe dostosowane pod N=64. Unikamy false-sharingu cache.
// ============================================================================

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace sudoku_hpc::logic {

// ============================================================================
// STRUKTURY DANYCH WSPÓŁDZIELONE (np. Almost Locked Sets)
// ============================================================================
struct ALS {
    uint64_t cell_mask[64]{};
    uint64_t digit_mask = 0ULL;
    uint8_t size = 0;
    uint8_t degree = 0;
    uint16_t house = 0;
};

// Struktura na potrzeby POM (Pattern Overlay Method) w P8
struct PomFrame {
    int cell_idx = -1;
    uint64_t pending_mask = 0ULL;
    uint64_t tried_mask = 0ULL;
};

// ============================================================================
// GŁÓWNY BUFOR WORKERA
// Wyrównanie w pamięci na 64 bajty by zabezpieczyć linię pamięci Cache przed
// degradacją w wyniku pracy wielu workerów.
// ============================================================================
struct alignas(64) ExactPatternScratchpad {
    static constexpr int MAX_N = 64;
    static constexpr int MAX_NN = MAX_N * MAX_N;          // do 4096 dla 64x64
    static constexpr int MAX_HOUSES = MAX_N * 3;          // 64 rzędy, 64 kolumny, 64 bloki
    static constexpr int MAX_STRONG_LINKS_PER_DIGIT = MAX_HOUSES;
    static constexpr int MAX_EDGES = MAX_HOUSES;
    static constexpr int MAX_ADJ = MAX_NN * 128;
    static constexpr int MAX_BFS = MAX_NN * 4;
    static constexpr int MAX_CHAIN = MAX_NN * 8;
    static constexpr int MAX_LINK_EDGES = MAX_NN * 48;

    // ------------------------------------------------------------------------
    // Pamięć współdzielona (General BFS)
    // ------------------------------------------------------------------------
    int bfs_queue[MAX_BFS]{};
    int bfs_depth[MAX_BFS]{};
    int bfs_parent[MAX_BFS]{};
    int visited[MAX_NN]{};

    // ------------------------------------------------------------------------
    // Pamięć współdzielona dla Grafów (Mapowania węzłów dla Chains)
    // ------------------------------------------------------------------------
    int cell_to_node[MAX_NN]{};
    int node_to_cell[MAX_NN]{};
    int node_degree[MAX_NN]{};
    int adj_offsets[MAX_NN + 1]{};
    int adj_cursor[MAX_NN]{};
    int adj_flat[MAX_ADJ]{};
    int edge_u[MAX_EDGES]{};
    int edge_v[MAX_EDGES]{};
    int edge_count = 0;
    int node_count = 0;

    // ------------------------------------------------------------------------
    // Struktury specyficzne dla Łańcuchów (Chains i Cycles)
    // ------------------------------------------------------------------------
    int chain_cell[MAX_CHAIN]{};
    uint64_t chain_enter_bit[MAX_CHAIN]{};
    int chain_parent[MAX_CHAIN]{};
    uint8_t chain_depth[MAX_CHAIN]{};
    int chain_count = 0;

    // ------------------------------------------------------------------------
    // Bufor dla technik podstawowych P4-P5 (Ryby i Skrzydła)
    // ------------------------------------------------------------------------
    uint64_t fish_row_masks[MAX_N]{};
    uint64_t fish_col_masks[MAX_N]{};
    int active_rows[MAX_N]{};
    int active_cols[MAX_N]{};
    int wing_cells[MAX_NN]{};
    int wing_count = 0;

    // ------------------------------------------------------------------------
    // Tablice dedykowane strukturom ALS
    // ------------------------------------------------------------------------
    ALS als_list[MAX_NN]{};
    int als_count = 0;
    int als_cells[MAX_NN]{};
    int als_cell_count = 0;
    
    // Tablice powiązań silnych per-cyfra
    int strong_count[MAX_N + 1]{};
    int strong_a[MAX_N + 1][MAX_STRONG_LINKS_PER_DIGIT]{};
    int strong_b[MAX_N + 1][MAX_STRONG_LINKS_PER_DIGIT]{};

    // ------------------------------------------------------------------------
    // Grafy Dynamiczne dla silników połączonych P8 (Dynamic Forcing / MSLS)
    // Używane do budowania map powiązań słabych/silnych w locie.
    // ------------------------------------------------------------------------
    int dyn_cell_to_node[MAX_NN]{};
    int dyn_node_to_cell[MAX_NN]{};
    int dyn_node_count = 0;
    int dyn_digit_cells[MAX_NN]{};
    int dyn_digit_cell_count = 0;

    int dyn_strong_edge_u[MAX_HOUSES * 2]{};
    int dyn_strong_edge_v[MAX_HOUSES * 2]{};
    int dyn_strong_edge_count = 0;
    int dyn_weak_edge_u[MAX_LINK_EDGES]{};
    int dyn_weak_edge_v[MAX_LINK_EDGES]{};
    int dyn_weak_edge_count = 0;

    int dyn_strong_degree[MAX_NN]{};
    int dyn_weak_degree[MAX_NN]{};
    int dyn_strong_offsets[MAX_NN + 1]{};
    int dyn_weak_offsets[MAX_NN + 1]{};
    int dyn_strong_cursor[MAX_NN]{};
    int dyn_weak_cursor[MAX_NN]{};
    int dyn_strong_adj[MAX_ADJ]{};
    int dyn_weak_adj[MAX_ADJ]{};

    // ------------------------------------------------------------------------
    // Rozszerzenie P8: Bufor Stanu Zrzutów (Zastępuje Rekurencję Stosu)
    // ------------------------------------------------------------------------
    int dyn_state_depth[MAX_NN * 2]{};
    int dyn_state_parent[MAX_NN * 2]{};
    int dyn_state_queue[MAX_NN * 2]{};
    int dyn_prop_queue[MAX_NN]{};
    
    // Odtwarzanie układu mask kandydatów po nieudanej/udanej propagacji 
    // dla Forcing Chains. Gwarantuje stan czystości do 2-8 poziomów zagnieżdżenia.
    uint64_t dyn_cands_backup[MAX_NN]{};
    uint16_t dyn_values_backup[MAX_NN]{};
    int dyn_empty_backup = 0;

    // Struktura zrzutu dla Pattern Overlay Method (DFS fallback P8)
    std::array<PomFrame, MAX_N> pom_stack{};
    int pom_depth = 0;

    // ========================================================================
    // Metody narzędziowe ułatwiające czyszczenie pamięci przed weryfikacją
    // ========================================================================
    void reset_node_maps(int nn) {
        std::fill_n(cell_to_node, nn, -1);
        node_count = 0;
        edge_count = 0;
    }

    void reset_graph(int node_n) {
        std::fill_n(node_degree, node_n, 0);
        std::fill_n(adj_offsets, node_n + 1, 0);
        std::fill_n(adj_cursor, node_n, 0);
    }

    void reset_dynamic_graph(int nn) {
        std::fill_n(dyn_cell_to_node, nn, -1);
        dyn_node_count = 0;
        dyn_digit_cell_count = 0;
        dyn_strong_edge_count = 0;
        dyn_weak_edge_count = 0;
    }

    void reset_dynamic_adjacency(int node_n) {
        std::fill_n(dyn_strong_degree, node_n, 0);
        std::fill_n(dyn_weak_degree, node_n, 0);
        std::fill_n(dyn_strong_offsets, node_n + 1, 0);
        std::fill_n(dyn_weak_offsets, node_n + 1, 0);
        std::fill_n(dyn_strong_cursor, node_n, 0);
        std::fill_n(dyn_weak_cursor, node_n, 0);
    }
    
    void reset_pom() {
        pom_depth = 0;
        for (auto& f : pom_stack) {
            f.cell_idx = -1;
            f.pending_mask = 0ULL;
            f.tried_mask = 0ULL;
        }
    }
};

// Szybki dostęp globalny dla wątku 
inline ExactPatternScratchpad& exact_pattern_scratchpad() {
    thread_local ExactPatternScratchpad scratch;
    return scratch;
}

} // namespace sudoku_hpc::logic

namespace sudoku_hpc::logic::shared {

using ::sudoku_hpc::logic::ALS;
using ::sudoku_hpc::logic::PomFrame;
using ::sudoku_hpc::logic::ExactPatternScratchpad;

inline ExactPatternScratchpad& exact_pattern_scratchpad() {
    return ::sudoku_hpc::logic::exact_pattern_scratchpad();
}

} // namespace sudoku_hpc::logic::shared
