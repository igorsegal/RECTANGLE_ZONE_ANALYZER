#pragma once
// ============================================================================
// timer.h — Утилиты для замеров времени
// ============================================================================
// Одна ответственность: измерение времени выполнения.
// ============================================================================

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace rza {

class Timer {
public:
    Timer() { reset(); }

    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    double elapsed_sec() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - start_).count();
    }

    double elapsed_ms() const {
        return elapsed_sec() * 1000.0;
    }

    std::string elapsed_str() const {
        double sec = elapsed_sec();
        std::ostringstream oss;
        if (sec >= 60.0) {
            int minutes = static_cast<int>(sec) / 60;
            double seconds = sec - minutes * 60;
            oss << minutes << "m " << std::fixed << std::setprecision(1) << seconds << "s";
        } else if (sec >= 1.0) {
            oss << std::fixed << std::setprecision(2) << sec << "s";
        } else {
            oss << static_cast<int>(sec * 1000) << "ms";
        }
        return oss.str();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace rza