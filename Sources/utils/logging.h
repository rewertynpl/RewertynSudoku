#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace sudoku_hpc {

class DebugLogger {
public:
    DebugLogger() {
        try {
            const auto now = std::chrono::system_clock::now();
            const auto t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            std::ostringstream name;
            name << "sudoku_debug_"
                 << std::put_time(&tm, "%Y%m%d_%H%M%S")
                 << ".log";
            path_ = (std::filesystem::current_path() / name.str()).string();
            stream_.open(path_, std::ios::out | std::ios::app);
        } catch (...) {
            path_ = "sudoku_debug.log";
            stream_.open(path_, std::ios::out | std::ios::app);
        }
    }

    const std::string& path() const {
        return path_;
    }

    void write(const char* level, const std::string& scope, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mu_);
        if (!stream_) {
            return;
        }
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        stream_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
                << " [" << level << "] "
                << "(" << scope << ") "
                << msg
                << "\n";
        stream_.flush();
    }

private:
    std::string path_;
    std::ofstream stream_;
    std::mutex mu_;
};

inline DebugLogger& debug_logger() {
    static DebugLogger logger;
    return logger;
}

inline void log_info(const std::string& scope, const std::string& msg) {
    debug_logger().write("INFO", scope, msg);
}

inline void log_warn(const std::string& scope, const std::string& msg) {
    debug_logger().write("WARN", scope, msg);
}

inline void log_error(const std::string& scope, const std::string& msg) {
    debug_logger().write("ERROR", scope, msg);
}

} // namespace sudoku_hpc
