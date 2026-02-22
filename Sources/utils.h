#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <random>
#include <sstream>
#include <thread>
#include <optional>

namespace sudoku_hpc {
uint64_t splitmix64(uint64_t& state) {
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31u);
}

inline int popcnt64(uint64_t v) {
    return std::popcount(v);
}

inline int single_digit_from_mask(uint64_t mask) {
    if (mask == 0 || (mask & (mask - 1ULL)) != 0) {
        return 0;
    }
    return std::countr_zero(mask) + 1;
}

inline int cell_row(int idx) {
    return idx / kN;
}

inline int cell_col(int idx) {
    return idx % kN;
}

inline int box_index(int row, int col) {
    return (row / kBoxRows) * kBoxRows + (col / kBoxCols);
}

// VIP Premium Feature: Generate a rich HTML visualizer of the generated Sudoku grid
inline void export_to_html(const std::string& filename, const std::string& puzzle, const std::string& solution) {
    std::ofstream out(filename);
    if(!out) return;
    out << "<!DOCTYPE html>\n<html>\n<head>\n<title>Sudoku Premium Viewer</title>\n";
    out << "<style>\nbody { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #1e1e24; color: #f4f4f9; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }\n";
    out << ".grid { display: grid; grid-template-columns: repeat(9, 60px); grid-template-rows: repeat(9, 60px); gap: 2px; background-color: #f4f4f9; padding: 4px; border-radius: 8px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }\n";
    out << ".cell { display: flex; align-items: center; justify-content: center; background-color: #2b2b36; font-size: 24px; font-weight: bold; color: #4dcad6; }\n";
    out << ".cell.clue { color: #f4f4f9; background-color: #3b3b4d; }\n";
    out << ".cell:nth-child(3n) { margin-right: 4px; }\n.cell.row-3, .cell.row-6 { margin-bottom: 4px; }\n";
    out << "h1 { color: #4dcad6; font-size: 3em; margin-bottom: 0.5em; text-transform: uppercase; letter-spacing: 2px; }\n";
    out << "</style>\n</head>\n<body>\n<h1>Premium Sudoku</h1>\n<div class='grid'>\n";
    
    for (int i=0; i<81; ++i) {
        std::string extra_cls = "";
        int r = i / 9;
        if (r == 2 || r == 5) extra_cls += " row-3"; // simple trick for bold bottom border
        
        char p = (i < puzzle.size()) ? puzzle[i] : '0';
        char s = (i < solution.size()) ? solution[i] : '0';
        
        if (p != '0' && p != '.') {
            out << "<div class='cell clue" << extra_cls << "'>" << p << "</div>\n";
        } else {
            out << "<div class='cell" << extra_cls << "'>" << s << "</div>\n";
        }
    }
    out << "</div>\n</body>\n</html>\n";
}


} // namespace sudoku_hpc