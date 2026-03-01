//Author copyright Marcin Matysek (Rewertyn)
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sudoku_hpc {

struct GenericTopology {
    int box_rows = 0;
    int box_cols = 0;
    int n = 0;
    int nn = 0;

    int box_rows_count = 0;
    int box_cols_count = 0;

    std::vector<int> cell_row;
    std::vector<int> cell_col;
    std::vector<int> cell_box;
    std::vector<int> cell_center_sym;
    std::vector<uint32_t> cell_rcb_packed;

    std::vector<int> house_offsets;
    std::vector<int> houses_flat;

    std::vector<int> peer_offsets;
    std::vector<int> peers_flat;
};

inline uint32_t pack_rcb(int r, int c, int b) {
    return static_cast<uint32_t>((r & 63) | ((c & 63) << 6) | ((b & 63) << 12));
}

inline bool build_generic_topology(int box_rows, int box_cols, GenericTopology& topo, std::string* err = nullptr) {
    if (box_rows <= 0 || box_cols <= 0) {
        if (err != nullptr) *err = "box_rows and box_cols must be > 0";
        return false;
    }

    const int n = box_rows * box_cols;
    if (n < 4 || n > 64) {
        if (err != nullptr) *err = "n=box_rows*box_cols must be in [4,64]";
        return false;
    }

    const int nn = n * n;
    topo = {};
    topo.box_rows = box_rows;
    topo.box_cols = box_cols;
    topo.n = n;
    topo.nn = nn;
    topo.box_rows_count = n / box_rows;
    topo.box_cols_count = n / box_cols;

    topo.cell_row.resize(static_cast<size_t>(nn));
    topo.cell_col.resize(static_cast<size_t>(nn));
    topo.cell_box.resize(static_cast<size_t>(nn));
    topo.cell_center_sym.resize(static_cast<size_t>(nn));
    topo.cell_rcb_packed.resize(static_cast<size_t>(nn));

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            const int idx = r * n + c;
            const int br = r / box_rows;
            const int bc = c / box_cols;
            const int box = br * topo.box_cols_count + bc;

            topo.cell_row[static_cast<size_t>(idx)] = r;
            topo.cell_col[static_cast<size_t>(idx)] = c;
            topo.cell_box[static_cast<size_t>(idx)] = box;
            topo.cell_center_sym[static_cast<size_t>(idx)] = (n - 1 - r) * n + (n - 1 - c);
            topo.cell_rcb_packed[static_cast<size_t>(idx)] = pack_rcb(r, c, box);
        }
    }

    const int house_count = 3 * n;
    topo.house_offsets.resize(static_cast<size_t>(house_count + 1), 0);
    topo.houses_flat.reserve(static_cast<size_t>(house_count * n));

    int cursor = 0;
    for (int h = 0; h < n; ++h) {
        topo.house_offsets[static_cast<size_t>(h)] = cursor;
        for (int c = 0; c < n; ++c) {
            topo.houses_flat.push_back(h * n + c);
            ++cursor;
        }
    }
    for (int h = 0; h < n; ++h) {
        topo.house_offsets[static_cast<size_t>(n + h)] = cursor;
        for (int r = 0; r < n; ++r) {
            topo.houses_flat.push_back(r * n + h);
            ++cursor;
        }
    }
    for (int b = 0; b < n; ++b) {
        topo.house_offsets[static_cast<size_t>(2 * n + b)] = cursor;
        const int br = b / topo.box_cols_count;
        const int bc = b % topo.box_cols_count;
        const int r0 = br * box_rows;
        const int c0 = bc * box_cols;
        for (int dr = 0; dr < box_rows; ++dr) {
            for (int dc = 0; dc < box_cols; ++dc) {
                topo.houses_flat.push_back((r0 + dr) * n + (c0 + dc));
                ++cursor;
            }
        }
    }
    topo.house_offsets[static_cast<size_t>(house_count)] = cursor;

    topo.peer_offsets.resize(static_cast<size_t>(nn + 1), 0);
    topo.peers_flat.clear();
    topo.peers_flat.reserve(static_cast<size_t>(nn * (3 * n)));

    std::array<uint8_t, 4096> seen{};
    for (int idx = 0; idx < nn; ++idx) {
        std::fill(seen.begin(), seen.end(), static_cast<uint8_t>(0));

        const int r = topo.cell_row[static_cast<size_t>(idx)];
        const int c = topo.cell_col[static_cast<size_t>(idx)];
        const int b = topo.cell_box[static_cast<size_t>(idx)];

        topo.peer_offsets[static_cast<size_t>(idx)] = static_cast<int>(topo.peers_flat.size());

        for (int cc = 0; cc < n; ++cc) {
            const int p = r * n + cc;
            if (p == idx || seen[static_cast<size_t>(p)] != 0) continue;
            seen[static_cast<size_t>(p)] = 1;
            topo.peers_flat.push_back(p);
        }
        for (int rr = 0; rr < n; ++rr) {
            const int p = rr * n + c;
            if (p == idx || seen[static_cast<size_t>(p)] != 0) continue;
            seen[static_cast<size_t>(p)] = 1;
            topo.peers_flat.push_back(p);
        }

        const int br = b / topo.box_cols_count;
        const int bc = b % topo.box_cols_count;
        const int r0 = br * box_rows;
        const int c0 = bc * box_cols;
        for (int dr = 0; dr < box_rows; ++dr) {
            for (int dc = 0; dc < box_cols; ++dc) {
                const int p = (r0 + dr) * n + (c0 + dc);
                if (p == idx || seen[static_cast<size_t>(p)] != 0) continue;
                seen[static_cast<size_t>(p)] = 1;
                topo.peers_flat.push_back(p);
            }
        }
    }
    topo.peer_offsets[static_cast<size_t>(nn)] = static_cast<int>(topo.peers_flat.size());

    return true;
}

inline const std::vector<std::pair<int, int>>& supported_geometry_pairs() {
    static const std::vector<std::pair<int, int>> pairs = [] {
        std::vector<std::pair<int, int>> out;
        for (int n = 4; n <= 64; ++n) {
            for (int br = 1; br <= n; ++br) {
                if (n % br != 0) continue;
                const int bc = n / br;
                out.emplace_back(br, bc);
            }
        }
        return out;
    }();
    return pairs;
}

inline std::string supported_geometries_text() {
    std::ostringstream out;
    out << "Supported geometries (asymmetric included), n in [4,64]\n";
    for (int n = 4; n <= 64; ++n) {
        out << "n=" << n << ": ";
        bool first = true;
        for (const auto& [br, bc] : supported_geometry_pairs()) {
            if (br * bc != n) continue;
            if (!first) out << ", ";
            first = false;
            out << br << "x" << bc;
        }
        out << "\n";
    }
    return out.str();
}

inline bool print_geometry_validation(int box_rows, int box_cols, std::ostream& out) {
    GenericTopology topo;
    std::string err;
    const bool ok = build_generic_topology(box_rows, box_cols, topo, &err);
    out << "geometry " << box_rows << "x" << box_cols << ": " << (ok ? "ok" : "fail") << "\n";
    if (!ok) {
        out << "reason: " << err << "\n";
        return false;
    }
    out << "n=" << topo.n << " nn=" << topo.nn
        << " box_rows_count=" << topo.box_rows_count
        << " box_cols_count=" << topo.box_cols_count << "\n";
    out << "houses=" << (static_cast<int>(topo.house_offsets.size()) - 1)
        << " peers_flat=" << topo.peers_flat.size() << "\n";
    return true;
}

inline bool print_geometry_catalog_validation(std::ostream& out) {
    int ok_count = 0;
    for (const auto& [br, bc] : supported_geometry_pairs()) {
        GenericTopology topo;
        std::string err;
        const bool ok = build_generic_topology(br, bc, topo, &err);
        if (!ok) {
            out << "FAIL " << br << "x" << bc << ": " << err << "\n";
            return false;
        }

        const int house_count = static_cast<int>(topo.house_offsets.size()) - 1;
        if (house_count != 3 * topo.n) {
            out << "FAIL " << br << "x" << bc << ": house_count mismatch\n";
            return false;
        }
        if (static_cast<int>(topo.cell_row.size()) != topo.nn ||
            static_cast<int>(topo.cell_col.size()) != topo.nn ||
            static_cast<int>(topo.cell_box.size()) != topo.nn ||
            static_cast<int>(topo.cell_center_sym.size()) != topo.nn) {
            out << "FAIL " << br << "x" << bc << ": cell arrays size mismatch\n";
            return false;
        }
        ++ok_count;
    }
    out << "Geometry catalog validation OK. cases=" << ok_count << "\n";
    return true;
}

} // namespace sudoku_hpc
