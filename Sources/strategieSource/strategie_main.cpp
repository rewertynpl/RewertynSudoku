// ============================================================================
// SUDOKU HPC - GEOMETRIE PLANSZ
// Plik: strategie_main.cpp
// ============================================================================

#ifndef STRATEGIE_MAIN_H
#define STRATEGIE_MAIN_H

#include <algorithm>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace sudoku_geometrie {

struct GeometriaInfo {
    int N;              
    int box_rows;       
    int box_cols;       
    const char* desc;   
};

static constexpr int MIN_N = 4;
static constexpr int MAX_N = 64;
static std::vector<GeometriaInfo> g_wszystkie_geometrie;
static std::vector<std::string> g_opisy_geometrii;
static bool g_geometrie_zainicjalizowane = false;

inline void zainicjalizuj_geometrie() {
    if (g_geometrie_zainicjalizowane) return;

    int total = 0;
    for (int n = MIN_N; n <= MAX_N; ++n) {
        for (int box_rows = 1; box_rows <= n; ++box_rows) {
            if (n % box_rows != 0) continue;
            ++total;
        }
    }
    g_wszystkie_geometrie.clear();
    g_opisy_geometrii.clear();
    g_wszystkie_geometrie.reserve(static_cast<size_t>(total));
    g_opisy_geometrii.reserve(static_cast<size_t>(total));

    for (int n = MIN_N; n <= MAX_N; ++n) {
        for (int box_rows = 1; box_rows <= n; ++box_rows) {
            if (n % box_rows != 0) continue;
            const int box_cols = n / box_rows;
            g_opisy_geometrii.push_back(
                std::to_string(n) + "x" + std::to_string(n) + " (" +
                std::to_string(box_rows) + "x" + std::to_string(box_cols) + ")");
            GeometriaInfo info{};
            info.N = n;
            info.box_rows = box_rows;
            info.box_cols = box_cols;
            info.desc = g_opisy_geometrii.back().c_str();
            g_wszystkie_geometrie.push_back(info);
        }
    }
    g_geometrie_zainicjalizowane = true;
}

inline int liczba_geometrii() {
    zainicjalizuj_geometrie();
    return static_cast<int>(g_wszystkie_geometrie.size());
}

inline const GeometriaInfo* get_wszystkie_geometrie_ptr() {
    zainicjalizuj_geometrie();
    return g_wszystkie_geometrie.empty() ? nullptr : g_wszystkie_geometrie.data();
}

inline const GeometriaInfo& get_geometria(int index) {
    zainicjalizuj_geometrie();
    return g_wszystkie_geometrie[static_cast<size_t>(index)];
}

inline const GeometriaInfo* znajdz_geometrie(int N, int box_rows, int box_cols) {
    zainicjalizuj_geometrie();
    for (const auto& g : g_wszystkie_geometrie) {
        if (g.N == N && g.box_rows == box_rows && g.box_cols == box_cols) return &g;
    }
    return nullptr;
}

inline std::vector<const GeometriaInfo*> znajdz_geometrie_dla_N(int N) {
    zainicjalizuj_geometrie();
    std::vector<const GeometriaInfo*> wynik;
    for (const auto& g : g_wszystkie_geometrie) {
        if (g.N == N) wynik.push_back(&g);
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
    wynik.reserve(static_cast<size_t>(MAX_N - MIN_N + 1));
    int ostatni_N = std::numeric_limits<int>::min();
    for (const auto& g : g_wszystkie_geometrie) {
        const int N = g.N;
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
