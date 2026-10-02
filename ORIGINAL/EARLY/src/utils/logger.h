#pragma once
// ============================================================================
// logger.h — Простой логгер
// ============================================================================
// Одна ответственность: вывод сообщений с уровнями важности.
// ============================================================================

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "core/types.h"

namespace rza {

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        level_ = level;
    }

    void set_enabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = enabled;
    }

    void log(LogLevel level, const std::string& message) {
        if (!enabled_) return;
        if (level < level_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[" << timestamp() << "] "
                  << "[" << log_level_str(level) << "] "
                  << message << "\n";
        std::cout.flush();
    }

    void trace(const std::string& msg) { log(LogLevel::TRACE, msg); }
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg)  { log(LogLevel::INFO, msg); }
    void warn(const std::string& msg)  { log(LogLevel::WARN, msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void fatal(const std::string& msg) { log(LogLevel::FATAL, msg); }

private:
    Logger() : level_(LogLevel::INFO), enabled_(true) {}

    std::string timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << tm_buf.tm_hour << ":"
            << std::setw(2) << tm_buf.tm_min << ":"
            << std::setw(2) << tm_buf.tm_sec;
        return oss.str();
    }

    LogLevel level_;
    bool enabled_;
    std::mutex mutex_;
};

// Удобные макросы
#define LOG_TRACE(msg) rza::Logger::instance().trace(msg)
#define LOG_DEBUG(msg) rza::Logger::instance().debug(msg)
#define LOG_INFO(msg)  rza::Logger::instance().info(msg)
#define LOG_WARN(msg)  rza::Logger::instance().warn(msg)
#define LOG_ERROR(msg) rza::Logger::instance().error(msg)
#define LOG_FATAL(msg) rza::Logger::instance().fatal(msg)

} // namespace rza