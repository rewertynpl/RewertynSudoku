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
enum class Align {
    Left,
    Right,
    Center
};

struct ColumnSpec {
    std::string header;
    Align align = Align::Right;
};

std::string pad_cell(const std::string& text, size_t width, Align align) {
    if (text.size() >= width) {
        return text;
    }
    const size_t gap = width - text.size();
    if (align == Align::Left) {
        return text + std::string(gap, ' ');
    }
    if (align == Align::Right) {
        return std::string(gap, ' ') + text;
    }
    const size_t left_pad = gap / 2;
    const size_t right_pad = gap - left_pad;
    return std::string(left_pad, ' ') + text + std::string(right_pad, ' ');
}

std::string render_table(const std::vector<ColumnSpec>& cols, const std::vector<std::vector<std::string>>& rows) {
    std::vector<size_t> widths(cols.size(), 0);
    for (size_t i = 0; i < cols.size(); ++i) {
        widths[i] = cols[i].header.size();
    }
    for (const auto& row : rows) {
        for (size_t i = 0; i < cols.size() && i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }

    auto separator = [&]() {
        std::ostringstream oss;
        oss << '+';
        for (size_t i = 0; i < cols.size(); ++i) {
            oss << std::string(widths[i] + 2, '-') << '+';
        }
        return oss.str();
    };

    std::ostringstream out;
    out << separator() << '\n';
    out << '|';
    for (size_t i = 0; i < cols.size(); ++i) {
        out << ' ' << pad_cell(cols[i].header, widths[i], Align::Center) << ' ' << '|';
    }
    out << '\n' << separator() << '\n';
    for (const auto& row : rows) {
        out << '|';
        for (size_t i = 0; i < cols.size(); ++i) {
            const std::string cell = (i < row.size()) ? row[i] : "";
            out << ' ' << pad_cell(cell, widths[i], cols[i].align) << ' ' << '|';
        }
        out << '\n';
    }
    out << separator();
    return out.str();
}

std::string format_fixed(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string now_local_string() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string now_local_compact_string() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string ascii_sanitize(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char ch : in) {
        if (ch >= 32 && ch <= 126) {
            out.push_back(static_cast<char>(ch));
        } else if (ch == '\t') {
            out.push_back('\t');
        } else {
            out.push_back('?');
        }
    }
    return out;
}

class DebugFileLogger {
public:
    DebugFileLogger() {
        std::lock_guard<std::mutex> lock(mu_);
        init_locked();
    }

    void log(std::string_view level, std::string_view scope, std::string_view message) {
        std::lock_guard<std::mutex> lock(mu_);
        if (!initialized_) {
            init_locked();
        }
        if (!out_) {
            return;
        }
        const auto tid_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
        out_ << now_local_string()
             << " [" << ascii_sanitize(level) << "]"
             << " [" << ascii_sanitize(scope) << "]"
             << " [tid=" << tid_hash << "] "
             << ascii_sanitize(message)
             << "\n";
        out_.flush();
    }

    std::string path() const {
        std::lock_guard<std::mutex> lock(mu_);
        return path_;
    }

private:
    mutable std::mutex mu_;
    bool initialized_ = false;
    std::ofstream out_;
    std::string path_;

    void init_locked() {
        initialized_ = true;
        std::error_code ec;
        std::filesystem::create_directories("debug", ec);
        std::ostringstream name;
        name << "sudoku_debug_" << now_local_compact_string();
#ifdef _WIN32
        name << "_pid" << GetCurrentProcessId();
#endif
        name << ".txt";
        std::filesystem::path p = std::filesystem::path("debug") / name.str();
        path_ = p.string();
        out_.open(path_, std::ios::out | std::ios::app);
        if (!out_) {
            return;
        }
        out_ << "=== Sudoku HPC Debug Log ===\n";
        out_ << "Started: " << now_local_string() << "\n";
        out_ << "Path: " << path_ << "\n";
        out_ << "----------------------------------------\n";
        out_.flush();
    }
};

DebugFileLogger& debug_logger() {
    static DebugFileLogger logger;
    return logger;
}

void log_info(std::string_view scope, std::string_view message) {
    debug_logger().log("INFO", scope, message);
}

void log_warn(std::string_view scope, std::string_view message) {
    debug_logger().log("WARN", scope, message);
}

void log_error(std::string_view scope, std::string_view message) {
    debug_logger().log("ERROR", scope, message);
}

struct BenchmarkTableARow {
    int lvl = 0;
    int solved_ok = 0;
    int analyzed = 0;
    uint64_t required_use = 0;
    uint64_t required_hit = 0;
    uint64_t reject_strategy = 0;
    double avg_solved_gen_ms = 0.0;
    double avg_dig_ms = 0.0;
    double avg_analyze_ms = 0.0;
    uint64_t backtracks = 0;
    int timeouts = 0;
    double success_rate = 0.0;
};

struct BenchmarkTableA2Row {
    int lvl = 0;
    int analyzed = 0;
    uint64_t medusa_hit = 0;
    uint64_t medusa_use = 0;
    uint64_t sue_hit = 0;
    uint64_t sue_use = 0;
    uint64_t msls_hit = 0;
    uint64_t msls_use = 0;
};

struct BenchmarkTableA3Row {
    std::string strategy;
    int lvl = 0;
    uint64_t max_attempts = 0;
    uint64_t analyzed = 0;
    uint64_t required_strategy_hits = 0;
    double analyzed_per_s = 0.0;
    uint64_t est_5min = 0;
    uint64_t written = 0;
};

struct BenchmarkTableBRow {
    std::string size;
    std::array<std::string, 8> levels{};
};

struct BenchmarkTableCRow {
    std::string size;
    int lvl = 0;
    double est_analyze_s = 0.0;
    double budget_s = 0.0;
    double peak_ram_mb = 0.0;
    std::string decision;
};

struct BenchmarkTableMicroprofilingRow {
    std::string stage;  // SolvedKernel, DigKernel, etc.
    int lvl = 0;
    uint64_t call_count = 0;
    double avg_elapsed_ms = 0.0;
    double total_elapsed_ms = 0.0;
    uint64_t min_elapsed_ns = 0;
    uint64_t max_elapsed_ns = 0;
    double pct_of_total = 0.0;  // % całkowitego czasu
};

std::string format_hhmmss(uint64_t total_s);  // Forward declaration

struct BenchmarkReportData {
    std::string title = "Porownanie strategii Sudoku (poziomy 1-8)";
    std::string probe_per_level = "0";
    std::string benchmark_mode = "manual";
    std::string cpu_model = "unknown";
    std::string ram_info = "unknown";
    std::string os_info = "unknown";
    std::string runtime_info = "C++20";
    std::string threads_info = "1";
    std::vector<BenchmarkTableARow> table_a;
    std::vector<BenchmarkTableA2Row> table_a2;
    std::vector<BenchmarkTableA3Row> table_a3;
    std::vector<BenchmarkTableBRow> table_b;
    std::vector<BenchmarkTableCRow> table_c;
    std::vector<BenchmarkTableMicroprofilingRow> table_microprofiling;  // NOWE: mikroprofiling
    std::vector<std::string> rules;
    uint64_t total_execution_s = 0;

    // Quality Gate - progi regresji
    bool quality_gate_enabled = false;
    double quality_gate_min_throughput_pct = 90.0;  // minimalny % throughput względem baseline
    double quality_gate_max_reject_strategy_pct = 20.0;  // maksymalny % reject_strategy
    bool quality_gate_passed = true;
    std::string quality_gate_message;

    // Metoda serializująca do tekstu
    std::string to_text() const {
        std::ostringstream oss;
        oss << "============================================================================\n";
        oss << title << "\n";
        oss << "============================================================================\n";
        oss << "Benchmark mode: " << benchmark_mode << " | Probe: " << probe_per_level << "\n";
        oss << "Threads: " << threads_info << " | Runtime: " << runtime_info << "\n";
        oss << "CPU: " << cpu_model << "\n";
        oss << "RAM: " << ram_info << " | OS: " << os_info << "\n";
        oss << "Total execution time: " << format_hhmmss(total_execution_s) << "\n\n";
        
        oss << "TABLE A: Strategy Performance\n";
        oss << "----------------------------\n";
        for (const auto& row : table_a) {
            oss << "Lvl " << row.lvl << ": solved=" << row.solved_ok 
                << ", analyzed=" << row.analyzed 
                << ", success_rate=" << std::fixed << std::setprecision(1) << row.success_rate << "%\n";
        }
        oss << "\n";
        
        oss << "TABLE A3: Required Strategy\n";
        oss << "---------------------------\n";
        for (const auto& row : table_a3) {
            oss << row.strategy << " (L" << row.lvl << "): analyzed=" << row.analyzed 
                << ", written=" << row.written << "\n";
        }
        oss << "\n";
        
        if (!table_microprofiling.empty()) {
            oss << "MICROPROFILING\n";
            oss << "--------------\n";
            for (const auto& row : table_microprofiling) {
                oss << row.stage << " (L" << row.lvl << "): avg=" << std::fixed << std::setprecision(3) 
                    << row.avg_elapsed_ms << "ms, total=" << row.total_elapsed_ms << "ms\n";
            }
            oss << "\n";
        }
        
        if (!rules.empty()) {
            oss << "RULES:\n";
            for (const auto& rule : rules) {
                oss << "  - " << rule << "\n";
            }
        }
        
        return oss.str();
    }
};

std::string format_hhmmss(uint64_t total_s) {
    const uint64_t h = total_s / 3600;
    const uint64_t m = (total_s % 3600) / 60;
    const uint64_t s = total_s % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << "h "
        << std::setw(2) << std::setfill('0') << m << "m "
        << std::setw(2) << std::setfill('0') << s << "s";
    return oss.str();
}

double process_current_ram_mb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

double process_peak_ram_mb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

std::string format_mb(double mb) {
    return format_fixed(mb, 1) + " MB";
}

std::string detect_cpu_model() {
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0,
            KEY_QUERY_VALUE,
            &key) == ERROR_SUCCESS) {
        wchar_t value[256]{};
        DWORD type = 0;
        DWORD size = sizeof(value);
        const LONG rc = RegQueryValueExW(
            key,
            L"ProcessorNameString",
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(value),
            &size);
        RegCloseKey(key);
        if (rc == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
            const int len = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                std::string out(static_cast<size_t>(len), '\0');
                WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), len, nullptr, nullptr);
                out.resize(static_cast<size_t>(len - 1));
                return out;
            }
        }
    }
#endif
    return "unknown";
}

std::string detect_ram_info() {
#ifdef _WIN32
    MEMORYSTATUSEX msx{};
    msx.dwLength = sizeof(msx);
    if (GlobalMemoryStatusEx(&msx)) {
        const double total_mb = static_cast<double>(msx.ullTotalPhys) / (1024.0 * 1024.0);
        const double avail_mb = static_cast<double>(msx.ullAvailPhys) / (1024.0 * 1024.0);
        return "total=" + format_mb(total_mb) + ", avail=" + format_mb(avail_mb);
    }
#endif
    return "unknown";
}

std::string detect_os_info() {
#ifdef _WIN32
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll != nullptr) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
        auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        if (rtl_get_version != nullptr) {
            RTL_OSVERSIONINFOW osvi{};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (rtl_get_version(&osvi) == 0) {
                std::ostringstream oss;
                oss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << " build " << osvi.dwBuildNumber;
                return oss.str();
            }
        }
    }

    OSVERSIONINFOW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    if (GetVersionExW(&osvi)) {
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        std::ostringstream oss;
        oss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << " build " << osvi.dwBuildNumber;
        return oss.str();
    }
#endif
    return "unknown";
}

std::string detect_runtime_info() {
#if defined(_MSC_VER)
    return std::string("C++20 / MSVC ") + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::string("C++20 / clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("C++20 / GCC ") + __VERSION__;
#else
    return "C++20";
#endif
}

class BenchmarkReportWriter {
public:
    static bool write_text_report(const BenchmarkReportData& data, const std::string& path, bool append = false) {
        std::ofstream out(path, append ? (std::ios::out | std::ios::app) : std::ios::out);
        if (!out) {
            log_error("BenchmarkReportWriter", "cannot open report file: " + path);
            return false;
        }

        out << data.title << "\n";
        out << "DateTime: " << now_local_string() << "\n";
        out << "Probe na poziom: " << data.probe_per_level << "\n";
        out << "Tryb benchmarku: " << data.benchmark_mode << "\n";
        out << "CPU: " << data.cpu_model << "\n";
        out << "RAM: " << data.ram_info << "\n";
        out << "OS: " << data.os_info << "\n";
        out << "Runtime: " << data.runtime_info << "\n";
        out << "Threads: " << data.threads_info << "\n\n";

        out << "Tabela A: Czas benchmarku (geometria aktualna)\n";
        out << render_table(
                   {
                       {"lvl", Align::Right},
                       {"solved_ok", Align::Right},
                       {"analyzed", Align::Right},
                       {"required_use", Align::Right},
                       {"required_hit/use", Align::Right},
                       {"reject_strategy", Align::Right},
                       {"avg_solved_gen_ms", Align::Right},
                       {"avg_dig_ms", Align::Right},
                       {"avg_analyze_ms", Align::Right},
                       {"backtracks", Align::Right},
                       {"timeouts", Align::Right},
                       {"success_rate", Align::Right},
                   },
                   to_rows_a(data.table_a))
            << "\n\n";

        out << "Tabela A2: Ciezkie strategie (geometria aktualna)\n";
        out << render_table(
                   {
                       {"lvl", Align::Right},
                       {"analyzed", Align::Right},
                       {"3DMedusa hit/use", Align::Right},
                       {"SueDeCoq hit/use", Align::Right},
                       {"MSLS hit/use", Align::Right},
                   },
                   to_rows_a2(data.table_a2))
            << "\n\n";

        out << "Tabela A3: Wymuszony test ciezkich strategii (required_strategy)\n";
        out << render_table(
                   {
                       {"strategy", Align::Left},
                       {"lvl", Align::Right},
                       {"max_attempts", Align::Right},
                       {"analyzed", Align::Right},
                       {"hit/use", Align::Right},
                       {"analyzed/s", Align::Right},
                       {"est_5min", Align::Right},
                       {"written", Align::Right},
                   },
                   to_rows_a3(data.table_a3))
            << "\n\n";

        out << "Tabela B: Min/Max clues (4x4..36x36, poziom 1-8)\n";
        out << render_table(
                   {
                       {"size", Align::Right},
                       {"L1", Align::Right},
                       {"L2", Align::Right},
                       {"L3", Align::Right},
                       {"L4", Align::Right},
                       {"L5", Align::Right},
                       {"L6", Align::Right},
                       {"L7", Align::Right},
                       {"L8", Align::Right},
                   },
                   to_rows_b(data.table_b))
            << "\n\n";

        out << "Tabela C: Szacowane czasy analyzera i RUN/SKIP dla testow\n";
        out << render_table(
                   {
                       {"size", Align::Right},
                       {"lvl", Align::Right},
                       {"est_analyze_s", Align::Right},
                       {"budget_s", Align::Right},
                       {"peak_ram_mb", Align::Right},
                       {"decyzja", Align::Left},
                   },
                   to_rows_c(data.table_c))
            << "\n\n";

        out << "Tabela D: Mikroprofiling etapow generatora\n";
        out << render_table(
                   {
                       {"stage", Align::Left},
                       {"lvl", Align::Right},
                       {"calls", Align::Right},
                       {"avg_ms", Align::Right},
                       {"total_ms", Align::Right},
                       {"min_ns", Align::Right},
                       {"max_ns", Align::Right},
                       {"pct_total", Align::Right},
                   },
                   to_rows_microprofiling(data.table_microprofiling))
            << "\n\n";

        out << "Regula testow (zaimplementowana):\n";
        for (const std::string& rule : data.rules) {
            out << "- " << rule << "\n";
        }
        out << "\n";
        out << "Total execution time: " << format_hhmmss(data.total_execution_s) << "\n";
        if (!out) {
            log_error("BenchmarkReportWriter", "write failed for report file: " + path);
            return false;
        }
        log_info("BenchmarkReportWriter", "report written: " + path);
        return true;
    }

private:
    static std::vector<std::vector<std::string>> to_rows_a(const std::vector<BenchmarkTableARow>& rows) {
        std::vector<std::vector<std::string>> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back(
                {
                    std::to_string(r.lvl),
                    std::to_string(r.solved_ok),
                    std::to_string(r.analyzed),
                    std::to_string(r.required_use),
                    std::to_string(r.required_hit) + "/" + std::to_string(r.required_use),
                    std::to_string(r.reject_strategy),
                    format_fixed(r.avg_solved_gen_ms, 3),
                    format_fixed(r.avg_dig_ms, 3),
                    format_fixed(r.avg_analyze_ms, 3),
                    std::to_string(r.backtracks),
                    std::to_string(r.timeouts),
                    format_fixed(r.success_rate, 2),
                });
        }
        return out;
    }

    static std::vector<std::vector<std::string>> to_rows_a2(const std::vector<BenchmarkTableA2Row>& rows) {
        std::vector<std::vector<std::string>> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back(
                {
                    std::to_string(r.lvl),
                    std::to_string(r.analyzed),
                    std::to_string(r.medusa_hit) + "/" + std::to_string(r.medusa_use),
                    std::to_string(r.sue_hit) + "/" + std::to_string(r.sue_use),
                    std::to_string(r.msls_hit) + "/" + std::to_string(r.msls_use),
                });
        }
        return out;
    }

    static std::vector<std::vector<std::string>> to_rows_a3(const std::vector<BenchmarkTableA3Row>& rows) {
        std::vector<std::vector<std::string>> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back(
                {
                    r.strategy,
                    std::to_string(r.lvl),
                    std::to_string(r.max_attempts),
                    std::to_string(r.analyzed),
                    std::to_string(r.required_strategy_hits) + "/" + std::to_string(r.analyzed),
                    format_fixed(r.analyzed_per_s, 2),
                    std::to_string(r.est_5min),
                    std::to_string(r.written),
                });
        }
        return out;
    }

    static std::vector<std::vector<std::string>> to_rows_b(const std::vector<BenchmarkTableBRow>& rows) {
        std::vector<std::vector<std::string>> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            std::vector<std::string> row;
            row.reserve(9);
            row.push_back(r.size);
            for (const std::string& lv : r.levels) {
                row.push_back(lv);
            }
            out.push_back(std::move(row));
        }
        return out;
    }

    static std::vector<std::vector<std::string>> to_rows_c(const std::vector<BenchmarkTableCRow>& rows) {
        std::vector<std::vector<std::string>> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back(
                {
                    r.size,
                    std::to_string(r.lvl),
                    format_fixed(r.est_analyze_s, 4),
                    format_fixed(r.budget_s, 4),
                    format_fixed(r.peak_ram_mb, 2),
                    r.decision,
                });
        }
        return out;
    }

    static std::vector<std::vector<std::string>> to_rows_microprofiling(const std::vector<BenchmarkTableMicroprofilingRow>& rows) {
        std::vector<std::vector<std::string>> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
            out.push_back(
                {
                    r.stage,
                    std::to_string(r.lvl),
                    std::to_string(r.call_count),
                    format_fixed(r.avg_elapsed_ms, 4),
                    format_fixed(r.total_elapsed_ms, 2),
                    std::to_string(r.min_elapsed_ns),
                    std::to_string(r.max_elapsed_ns),
                    format_fixed(r.pct_of_total, 2),
                });
        }
        return out;
    }
};

// ============================================================================
// MIKROPROFILING - Metryki wydajności per etap i per strategia
// ============================================================================

} // namespace sudoku_hpc
