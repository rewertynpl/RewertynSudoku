#pragma once

#include "strategieSource/strategie_main.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace sudoku_hpc::geometria {

using sudoku_geometrie::GeometriaInfo;

inline int liczba_geometrii() {
    return sudoku_geometrie::liczba_geometrii();
}

struct GeometrySpec {
    int n = 0;
    int box_rows = 0;
    int box_cols = 0;
    bool is_symmetric = false;
    std::string label;
};

enum class GeometryClass {
    Symmetric,
    Asymmetric
};

inline void zainicjalizuj_geometrie() {
    sudoku_geometrie::zainicjalizuj_geometrie();
}

inline bool czy_obslugiwana(int n, int box_rows, int box_cols) {
    return sudoku_geometrie::czy_obslugiwana(n, box_rows, box_cols);
}

inline bool czy_obslugiwana(int box_rows, int box_cols) {
    if (box_rows <= 0 || box_cols <= 0) {
        return false;
    }
    return czy_obslugiwana(box_rows * box_cols, box_rows, box_cols);
}

inline const GeometriaInfo* znajdz_geometrie(int n, int box_rows, int box_cols) {
    return sudoku_geometrie::znajdz_geometrie(n, box_rows, box_cols);
}

inline const std::vector<GeometrySpec>& all_geometries() {
    zainicjalizuj_geometrie();
    static const std::vector<GeometrySpec> cache = [] {
        std::vector<GeometrySpec> out;
        const int count = sudoku_geometrie::liczba_geometrii();
        out.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            const auto& g = sudoku_geometrie::get_geometria(i);
            out.push_back(
                {g.N,
                 g.box_rows,
                 g.box_cols,
                 g.box_rows == g.box_cols,
                 g.desc == nullptr ? "" : std::string(g.desc)});
        }
        std::sort(out.begin(), out.end(), [](const GeometrySpec& a, const GeometrySpec& b) {
            if (a.n != b.n) {
                return a.n < b.n;
            }
            if (a.box_rows != b.box_rows) {
                return a.box_rows < b.box_rows;
            }
            return a.box_cols < b.box_cols;
        });
        return out;
    }();
    return cache;
}

inline bool is_symmetric_geometry(int box_rows, int box_cols) {
    return box_rows > 0 && box_cols > 0 && box_rows == box_cols;
}

inline GeometryClass classify_geometry(int box_rows, int box_cols) {
    return is_symmetric_geometry(box_rows, box_cols) ? GeometryClass::Symmetric : GeometryClass::Asymmetric;
}

inline const char* geometry_class_name(GeometryClass c) {
    return c == GeometryClass::Symmetric ? "symmetric" : "asymmetric";
}

inline bool is_supported_geometry(int box_rows, int box_cols) {
    if (box_rows <= 0 || box_cols <= 0) {
        return false;
    }
    return czy_obslugiwana(box_rows * box_cols, box_rows, box_cols);
}

inline std::vector<GeometrySpec> geometries_for_n(int n) {
    std::vector<GeometrySpec> out;
    for (const auto& g : all_geometries()) {
        if (g.n == n) {
            out.push_back(g);
        }
    }
    std::sort(out.begin(), out.end(), [](const GeometrySpec& a, const GeometrySpec& b) {
        if (a.box_rows != b.box_rows) {
            return a.box_rows < b.box_rows;
        }
        return a.box_cols < b.box_cols;
    });
    return out;
}

inline bool validate_geometry_catalog(std::string* details_out = nullptr) {
    const auto& catalog = all_geometries();
    std::set<std::tuple<int, int, int>> seen;
    std::ostringstream details;
    bool ok = true;
    for (const auto& g : catalog) {
        if (g.n != g.box_rows * g.box_cols) {
            ok = false;
            details << "invalid_product " << g.n << " (" << g.box_rows << "x" << g.box_cols << ")\n";
        }
        const auto key = std::make_tuple(g.n, g.box_rows, g.box_cols);
        if (!seen.insert(key).second) {
            ok = false;
            details << "duplicate " << g.n << " (" << g.box_rows << "x" << g.box_cols << ")\n";
        }
        if (!czy_obslugiwana(g.n, g.box_rows, g.box_cols)) {
            ok = false;
            details << "not_recognized " << g.n << " (" << g.box_rows << "x" << g.box_cols << ")\n";
        }
    }
    if (details_out != nullptr) {
        *details_out = details.str();
    }
    return ok;
}

} // namespace sudoku_hpc::geometria
