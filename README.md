# Sudoku Level Generator RewertynPL 🚀

An ultra-fast, highly optimized, multithreaded Sudoku puzzle generator written in modern **C++20**. Designed with High-Performance Computing (HPC) in mind, this engine can generate and validate massive amounts of Sudoku grids ranging from basic 4x4 up to complex 36x36 asymmetric geometries.

Sudoku Level Generator Master Prompt Included in files as txt.

## ⚠️ Current Project Status

*   **GUI Language:** Please note that the Graphical User Interface (built with native WinAPI) is currently available **only in Polish**. CLI commands and internal logs are mostly in English/mixed.
*   **Strategy Implementation:** At this moment, **only Level 1 logical strategies** (e.g., *Naked Single*, *Hidden Single*) are fully implemented and active in the logic certifier. The codebase contains structural skeletons for advanced techniques (Levels 2-8, like *X-Wing*, *Swordfish*, *Exocet*, *MSLS*), but they are work-in-progress and not fully operational yet. In some strategies (chain to goal), with limited depth.

## ✨ Key Features

*   **Extreme Performance:** Utilizes hardware vectorization (`AVX2`, `AVX512`) and bitwise operations.
*   **Advanced Threading:** Features an extreme low-latency, lock-free thread pool with MPSC (Multi-Producer Single-Consumer) rings and zero `shared_ptr` overhead.
*   **Zero-Allocation Serialization:** Direct buffer writing using `std::to_chars` to completely avoid memory reallocation bottlenecks during grid output.
*   **Uniqueness Validation:** Includes a highly optimized DLX (Dancing Links / Algorithm X) solver adapted for 64-bit masks.
*   **Flexible Geometries:** Supports standard symmetric grids (e.g., 9x9 as 3x3 boxes) as well as asymmetric variants (e.g., 12x12 as 4x3 boxes) up to 36x36.
*   **Quality Gates & Telemetry:** Built-in benchmarking, difficulty profiling, and extensive microprofiling tools.

## 🛠️ Technology Stack

*   **Language:** C++20
*   **Platform:** Windows (Native WinAPI for GUI)
*   **Dependencies:** **None.** This project relies purely on the C++ Standard Library and Windows API. No external libraries (like Qt, Boost, etc.) are required.

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details. 

You are completely free to use this software for **commercial and private purposes**, modify it, and distribute it for free. 

*Note regarding third-party code: Currently, this project does **not** include any third-party libraries or dependencies that would require separate licensing or purchase. Everything is built from scratch using standard C++ and native OS APIs.*

## 🚀 Getting Started

### Prerequisites
*   A C++20 compatible compiler (e.g., MSVC, GCC, Clang).
*   Windows OS (for the GUI version).
*   A CPU supporting AVX2/AVX512 is highly recommended for maximum performance.

### Usage (CLI mode)
You can run the generator via the command line with various parameters:
```bash
# Generate 100 classic 9x9 Sudoku puzzles at difficulty level 1 using 8 threads
sudoku.exe --box-rows 3 --box-cols 3 --target 100 --difficulty 1 --threads 8

# List all supported grid geometries
sudoku.exe --list-geometries
