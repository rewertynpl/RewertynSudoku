// ============================================================================
// SUDOKU HPC - GEOMETRIE PLANSZ
// Plik: strategie_main.cpp
// ============================================================================

#ifndef STRATEGIE_MAIN_H
#define STRATEGIE_MAIN_H

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

namespace sudoku_geometrie {

struct GeometriaInfo {
    int N;              
    int box_rows;       
    int box_cols;       
    const char* desc;   
};

static constexpr int LICZBA_GEOMETRII = 69;
static GeometriaInfo g_wszystkie_geometrie[LICZBA_GEOMETRII];
static bool g_geometrie_zainicjalizowane = false;

inline void zainicjalizuj_geometrie() {
    if (g_geometrie_zainicjalizowane) return;
    int idx = 0;
    g_wszystkie_geometrie[idx++] = {4, 2, 2, "4x4 (2x2)"};
    g_wszystkie_geometrie[idx++] = {6, 2, 3, "6x6 (2x3)"};
    g_wszystkie_geometrie[idx++] = {6, 3, 2, "6x6 (3x2)"};
    g_wszystkie_geometrie[idx++] = {8, 2, 4, "8x8 (2x4)"};
    g_wszystkie_geometrie[idx++] = {8, 4, 2, "8x8 (4x2)"};
    g_wszystkie_geometrie[idx++] = {9, 3, 3, "9x9 (3x3)"};
    g_wszystkie_geometrie[idx++] = {10, 2, 5, "10x10 (2x5)"};
    g_wszystkie_geometrie[idx++] = {10, 5, 2, "10x10 (5x2)"};
    g_wszystkie_geometrie[idx++] = {12, 2, 6, "12x12 (2x6)"};
    g_wszystkie_geometrie[idx++] = {12, 6, 2, "12x12 (6x2)"};
    g_wszystkie_geometrie[idx++] = {12, 3, 4, "12x12 (3x4)"};
    g_wszystkie_geometrie[idx++] = {12, 4, 3, "12x12 (4x3)"};
    g_wszystkie_geometrie[idx++] = {14, 2, 7, "14x14 (2x7)"};
    g_wszystkie_geometrie[idx++] = {14, 7, 2, "14x14 (7x2)"};
    g_wszystkie_geometrie[idx++] = {15, 3, 5, "15x15 (3x5)"};
    g_wszystkie_geometrie[idx++] = {15, 5, 3, "15x15 (5x3)"};
    g_wszystkie_geometrie[idx++] = {16, 2, 8, "16x16 (2x8)"};
    g_wszystkie_geometrie[idx++] = {16, 8, 2, "16x16 (8x2)"};
    g_wszystkie_geometrie[idx++] = {16, 4, 4, "16x16 (4x4)"};
    g_wszystkie_geometrie[idx++] = {18, 2, 9, "18x18 (2x9)"};
    g_wszystkie_geometrie[idx++] = {18, 9, 2, "18x18 (9x2)"};
    g_wszystkie_geometrie[idx++] = {18, 3, 6, "18x18 (3x6)"};
    g_wszystkie_geometrie[idx++] = {18, 6, 3, "18x18 (6x3)"};
    g_wszystkie_geometrie[idx++] = {20, 2, 10, "20x20 (2x10)"};
    g_wszystkie_geometrie[idx++] = {20, 10, 2, "20x20 (10x2)"};
    g_wszystkie_geometrie[idx++] = {20, 4, 5, "20x20 (4x5)"};
    g_wszystkie_geometrie[idx++] = {20, 5, 4, "20x20 (5x4)"};
    g_wszystkie_geometrie[idx++] = {21, 3, 7, "21x21 (3x7)"};
    g_wszystkie_geometrie[idx++] = {21, 7, 3, "21x21 (7x3)"};
    g_wszystkie_geometrie[idx++] = {22, 2, 11, "22x22 (2x11)"};
    g_wszystkie_geometrie[idx++] = {22, 11, 2, "22x22 (11x2)"};
    g_wszystkie_geometrie[idx++] = {24, 2, 12, "24x24 (2x12)"};
    g_wszystkie_geometrie[idx++] = {24, 12, 2, "24x24 (12x2)"};
    g_wszystkie_geometrie[idx++] = {24, 3, 8, "24x24 (3x8)"};
    g_wszystkie_geometrie[idx++] = {24, 8, 3, "24x24 (8x3)"};
    g_wszystkie_geometrie[idx++] = {24, 4, 6, "24x24 (4x6)"};
    g_wszystkie_geometrie[idx++] = {24, 6, 4, "24x24 (6x4)"};
    g_wszystkie_geometrie[idx++] = {25, 5, 5, "25x25 (5x5)"};
    g_wszystkie_geometrie[idx++] = {26, 2, 13, "26x26 (2x13)"};
    g_wszystkie_geometrie[idx++] = {26, 13, 2, "26x26 (13x2)"};
    g_wszystkie_geometrie[idx++] = {27, 3, 9, "27x27 (3x9)"};
    g_wszystkie_geometrie[idx++] = {27, 9, 3, "27x27 (9x3)"};
    g_wszystkie_geometrie[idx++] = {28, 2, 14, "28x28 (2x14)"};
    g_wszystkie_geometrie[idx++] = {28, 14, 2, "28x28 (14x2)"};
    g_wszystkie_geometrie[idx++] = {28, 4, 7, "28x28 (4x7)"};
    g_wszystkie_geometrie[idx++] = {28, 7, 4, "28x28 (7x4)"};
    g_wszystkie_geometrie[idx++] = {30, 2, 15, "30x30 (2x15)"};
    g_wszystkie_geometrie[idx++] = {30, 15, 2, "30x30 (15x2)"};
    g_wszystkie_geometrie[idx++] = {30, 3, 10, "30x30 (3x10)"};
    g_wszystkie_geometrie[idx++] = {30, 10, 3, "30x30 (10x3)"};
    g_wszystkie_geometrie[idx++] = {30, 5, 6, "30x30 (5x6)"};
    g_wszystkie_geometrie[idx++] = {30, 6, 5, "30x30 (6x5)"};
    g_wszystkie_geometrie[idx++] = {32, 2, 16, "32x32 (2x16)"};
    g_wszystkie_geometrie[idx++] = {32, 16, 2, "32x32 (16x2)"};
    g_wszystkie_geometrie[idx++] = {32, 4, 8, "32x32 (4x8)"};
    g_wszystkie_geometrie[idx++] = {32, 8, 4, "32x32 (8x4)"};
    g_wszystkie_geometrie[idx++] = {33, 3, 11, "33x33 (3x11)"};
    g_wszystkie_geometrie[idx++] = {33, 11, 3, "33x33 (11x3)"};
    g_wszystkie_geometrie[idx++] = {34, 2, 17, "34x34 (2x17)"};
    g_wszystkie_geometrie[idx++] = {34, 17, 2, "34x34 (17x2)"};
    g_wszystkie_geometrie[idx++] = {35, 5, 7, "35x35 (5x7)"};
    g_wszystkie_geometrie[idx++] = {35, 7, 5, "35x35 (7x5)"};
    g_wszystkie_geometrie[idx++] = {36, 2, 18, "36x36 (2x18)"};
    g_wszystkie_geometrie[idx++] = {36, 18, 2, "36x36 (18x2)"};
    g_wszystkie_geometrie[idx++] = {36, 3, 12, "36x36 (3x12)"};
    g_wszystkie_geometrie[idx++] = {36, 12, 3, "36x36 (12x3)"};
    g_wszystkie_geometrie[idx++] = {36, 4, 9, "36x36 (4x9)"};
    g_wszystkie_geometrie[idx++] = {36, 9, 4, "36x36 (9x4)"};
    g_wszystkie_geometrie[idx++] = {36, 6, 6, "36x36 (6x6)"};
    g_geometrie_zainicjalizowane = true;
}

inline const GeometriaInfo* get_wszystkie_geometrie_ptr() { zainicjalizuj_geometrie(); return g_wszystkie_geometrie; }
inline const GeometriaInfo& get_geometria(int index) { zainicjalizuj_geometrie(); return g_wszystkie_geometrie[index]; }

inline const GeometriaInfo* znajdz_geometrie(int N, int box_rows, int box_cols) {
    zainicjalizuj_geometrie();
    for (int i = 0; i < LICZBA_GEOMETRII; ++i) {
        const auto& g = g_wszystkie_geometrie[i];
        if (g.N == N && g.box_rows == box_rows && g.box_cols == box_cols) return &g;
    }
    return nullptr;
}

inline std::vector<const GeometriaInfo*> znajdz_geometrie_dla_N(int N) {
    zainicjalizuj_geometrie();
    std::vector<const GeometriaInfo*> wynik;
    for (int i = 0; i < LICZBA_GEOMETRII; ++i) {
        if (g_wszystkie_geometrie[i].N == N) wynik.push_back(&g_wszystkie_geometrie[i]);
    }
    return wynik;
}

inline bool czy_obslugiwana(int N, int box_rows, int box_cols) { return znajdz_geometrie(N, box_rows, box_cols) != nullptr; }

inline const GeometriaInfo* domyslna_geometria(int N) {
    auto geometrie = znajdz_geometrie_dla_N(N);
    if (geometrie.empty()) return nullptr;
    const GeometriaInfo* najlepsza = geometrie[0];
    int min_roznica = std::abs(najlepsza->box_rows - najlepsza->box_cols);
    for (const auto* g : geometrie) {
        int roznica = std::abs(g->box_rows - g->box_cols);
        if (roznica < min_roznica) { min_roznica = roznica; najlepsza = g; }
    }
    return najlepsza;
}

inline std::vector<int> wszystkie_rozmiary() {
    zainicjalizuj_geometrie();
    std::vector<int> wynik;
    int ostatni_N = 0;
    for (int i = 0; i < LICZBA_GEOMETRII; ++i) {
        int N = g_wszystkie_geometrie[i].N;
        if (N != ostatni_N) { wynik.push_back(N); ostatni_N = N; }
    }
    return wynik;
}

inline std::string format_geometria(const GeometriaInfo& g) {
    return std::to_string(g.N) + "x" + std::to_string(g.N) + " (" + std::to_string(g.box_rows) + "x" + std::to_string(g.box_cols) + ")";
}

inline void wypisz_wszystkie_geometrie() {
    zainicjalizuj_geometrie();
    // (Oryginalnie wydruk w konsoli - zachowujemy pustą strukturę by zachować sygnaturę)
}

} // namespace sudoku_geometrie
#endif // STRATEGIE_MAIN_H
