// ============================================================================
// SUDOKU HPC - POST PROCESSING
// Moduł: replay_validator.h
// Opis: Walidacja rozwiązania po procesie "kopania" (Replay Validation)
//       oraz sprzętowo optymalizowane (64-bitowe) hashowanie FNV-1a.
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>

#include "../../core/board.h"
#include "../../logic/logic_result.h"
#include "../../logic/sudoku_logic_engine.h"

namespace sudoku_hpc::post_processing {

struct ReplayValidationResult {
    bool ok = false;
    bool solved = false;
    uint64_t puzzle_hash = 0;
    uint64_t expected_solution_hash = 0;
    uint64_t replay_solution_hash = 0;
    uint64_t trace_hash = 0;
};

// ============================================================================
// OPTYMALIZACJA WEKTORYZACJI HASHOWANIA
// Procesor trawi pamięć paczkami 64-bitowymi zamiast bajtami.
// Algorytm bazuje na wariancie FNV-1a zaadaptowanym do bloków 8-bajtowych.
// ============================================================================
inline uint64_t fnv1a64_bytes(const void* data, size_t len, uint64_t seed = 1469598103934665603ULL) {
    uint64_t h = seed;
    const size_t blocks = len / 8;
    const size_t remainder = len % 8;
    const uint64_t* ptr64 = static_cast<const uint64_t*>(data);
    
    // Pętla odwijana i pipelinowana przez kompilator dla 64-bitowych słów
    for (size_t i = 0; i < blocks; ++i) {
        h ^= ptr64[i];
        h *= 1099511628211ULL;
    }
    
    // Ewentualna reszta (max 7 bajtów) - cast na standardowe przetworzenie
    if (remainder > 0) {
        const uint8_t* ptr8 = reinterpret_cast<const uint8_t*>(ptr64 + blocks);
        for (size_t i = 0; i < remainder; ++i) {
            h ^= static_cast<uint64_t>(ptr8[i]);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

// Hashowanie zoptymalizowane dla wektorów cyfr Sudoku (uint16_t)
inline uint64_t hash_u16_vector(const std::vector<uint16_t>& data, uint64_t seed = 1469598103934665603ULL) {
    if (data.empty()) return seed;
    return fnv1a64_bytes(data.data(), data.size() * sizeof(uint16_t), seed);
}

// Funkcja wykonująca pełne przejście kontrolne z weryfikacją poprawności
// oraz budująca "Trace Hash" ze statystyk użytych strategii.
inline ReplayValidationResult run_replay_validation(
    const std::vector<uint16_t>& puzzle,
    const std::vector<uint16_t>& expected_solution,
    const GenericTopology& topo,
    const logic::GenericLogicCertify& logic) {
    
    ReplayValidationResult out{};
    
    // 1. Hash wejścia i wzorca
    out.puzzle_hash = hash_u16_vector(puzzle);
    out.expected_solution_hash = hash_u16_vector(expected_solution);
    
    // 2. Symulacja logiczna na "czysto" (Replay)
    const logic::GenericLogicCertifyResult replay = logic.certify(puzzle, topo, nullptr, true);
    out.solved = replay.solved;
    out.replay_solution_hash = hash_u16_vector(replay.solved_grid);

    // 3. Budowa Trace Hash z poszczególnych kroków silnika (szybkie flush block memory)
    uint64_t trace_seed = 1469598103934665603ULL;
    trace_seed = fnv1a64_bytes(&replay.steps, sizeof(replay.steps), trace_seed);
    
    // Unikanie iteracji po arrayu za pomocą bezpośredniego zrzutu pamięci struktury
    trace_seed = fnv1a64_bytes(
        replay.strategy_stats.data(), 
        replay.strategy_stats.size() * sizeof(logic::StrategyStats), 
        trace_seed
    );
    
    out.trace_hash = trace_seed;
    
    // 4. Werdykt
    out.ok = replay.solved && (replay.solved_grid == expected_solution);
    
    return out;
}

} // namespace sudoku_hpc::post_processing