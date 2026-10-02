#pragma once
// ============================================================================
// buckets.h — Функции бакетизации
// ============================================================================
// Одна ответственность: преобразование числовых значений в бакеты
// и обратно (декодирование границ бакетов).
// ============================================================================

#include <cstdint>
#include <ctime>

namespace rza {

// ============================================================================
// ВОЗРАСТ ЗОНЫ (9 бакетов)
// ============================================================================

inline int get_age_bucket(double age_minutes) {
    if (age_minutes < 15.0)   return 0;
    if (age_minutes < 30.0)   return 1;
    if (age_minutes < 60.0)   return 2;
    if (age_minutes < 120.0)  return 3;
    if (age_minutes < 240.0)  return 4;
    if (age_minutes < 480.0)  return 5;
    if (age_minutes < 1440.0) return 6;
    if (age_minutes < 2880.0) return 7;
    return 8;
}

inline const char* age_bucket_name(int b) {
    static const char* names[] = {
        "00_0_15_MIN", "01_15_30_MIN", "02_30_60_MIN",
        "03_1_2_HOURS", "04_2_4_HOURS", "05_4_8_HOURS",
        "06_8_24_HOURS", "07_24_48_HOURS", "08_48_HOURS_PLUS"
    };
    return (b >= 0 && b < 9) ? names[b] : "UNKNOWN";
}

inline bool get_age_bounds(int b, double& min_min, double& max_min) {
    static const double mins[] = {0, 15, 30, 60, 120, 240, 480, 1440, 2880};
    static const double maxs[] = {15, 30, 60, 120, 240, 480, 1440, 2880, -1.0};
    if (b < 0 || b > 8) return false;
    min_min = mins[b];
    max_min = maxs[b];
    return true;
}

// ============================================================================
// НОМЕР КАСАНИЯ (5 бакетов)
// ============================================================================

inline int get_touch_bucket(int touch_number) {
    if (touch_number <= 1) return 0;
    if (touch_number == 2) return 1;
    if (touch_number == 3) return 2;
    if (touch_number == 4) return 3;
    return 4;
}

inline const char* touch_bucket_name(int b) {
    static const char* names[] = {
        "TOUCH_1", "TOUCH_2", "TOUCH_3", "TOUCH_4", "TOUCH_5_PLUS"
    };
    return (b >= 0 && b < 5) ? names[b] : "ALL";
}

inline bool get_touch_bounds(int b, int& min_val, int& max_val) {
    static const int mins[] = {1, 2, 3, 4, 5};
    static const int maxs[] = {1, 2, 3, 4, -1};
    if (b < 0 || b > 4) return false;
    min_val = mins[b];
    max_val = maxs[b];
    return true;
}

// ============================================================================
// ГЛУБИНА ВХОДА (4 бакета)
// ============================================================================

inline int get_depth_bucket(double depth_pct) {
    if (depth_pct < 25.0) return 0;
    if (depth_pct < 50.0) return 1;
    if (depth_pct < 75.0) return 2;
    return 3;
}

inline const char* depth_bucket_name(int b) {
    static const char* names[] = {
        "DEPTH_0_25_PCT", "DEPTH_25_50_PCT", "DEPTH_50_75_PCT", "DEPTH_75_100_PCT"
    };
    return (b >= 0 && b < 4) ? names[b] : "ALL";
}

inline bool get_depth_bounds(int b, double& min_pct, double& max_pct) {
    static const double mins[] = {0.0, 25.0, 50.0, 75.0};
    static const double maxs[] = {25.0, 50.0, 75.0, 100.0};
    if (b < 0 || b > 3) return false;
    min_pct = mins[b];
    max_pct = maxs[b];
    return true;
}

// ============================================================================
// СКОРОСТЬ ПОДХОДА (4 бакета)
// ============================================================================

inline int get_approach_bucket(double approach_atr5) {
    if (approach_atr5 <= 0.0) return 0;
    if (approach_atr5 < 0.5)  return 1;
    if (approach_atr5 < 1.0)  return 2;
    return 3;
}

inline const char* approach_bucket_name(int b) {
    static const char* names[] = {
        "APPROACH_FLAT_OR_AWAY", "APPROACH_SLOW_LT_0_5_ATR",
        "APPROACH_MEDIUM_0_5_1_ATR", "APPROACH_FAST_GE_1_ATR"
    };
    return (b >= 0 && b < 4) ? names[b] : "ALL";
}

inline bool get_approach_bounds(int b, double& min_atr, double& max_atr) {
    static const double mins[] = {-1.0, 0.0, 0.5, 1.0};
    static const double maxs[] = {0.0, 0.5, 1.0, -1.0};
    if (b < 0 || b > 3) return false;
    min_atr = mins[b];
    max_atr = maxs[b];
    return true;
}

// ============================================================================
// ВЫСОТА ЗОНЫ (5 бакетов)
// ============================================================================

inline int get_height_bucket(double height_atr) {
    if (height_atr < 0.25) return 0;
    if (height_atr < 0.50) return 1;
    if (height_atr < 0.75) return 2;
    if (height_atr < 1.00) return 3;
    return 4;
}

inline const char* height_bucket_name(int b) {
    static const char* names[] = {
        "HEIGHT_LT_0_25_ATR", "HEIGHT_0_25_0_50_ATR",
        "HEIGHT_0_50_0_75_ATR", "HEIGHT_0_75_1_00_ATR",
        "HEIGHT_GE_1_00_ATR"
    };
    return (b >= 0 && b < 5) ? names[b] : "ALL";
}

inline bool get_height_bounds(int b, double& min_atr, double& max_atr) {
    static const double mins[] = {0.0, 0.25, 0.50, 0.75, 1.00};
    static const double maxs[] = {0.25, 0.50, 0.75, 1.00, -1.0};
    if (b < 0 || b > 4) return false;
    min_atr = mins[b];
    max_atr = maxs[b];
    return true;
}

// ============================================================================
// ТОРГОВАЯ СЕССИЯ (4 бакета)
// ============================================================================

inline int get_session_bucket(int64_t unix_time,
                              int asia_end = 7,
                              int london_end = 13,
                              int newyork_end = 21) {
    time_t t = static_cast<time_t>(unix_time);
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    int hour = tm_buf.tm_hour;
    if (hour < asia_end)      return 0;
    if (hour < london_end)    return 1;
    if (hour < newyork_end)   return 2;
    return 3;
}

inline const char* session_bucket_name(int b) {
    static const char* names[] = {
        "ASIA_SERVER_TIME", "LONDON_SERVER_TIME",
        "NEW_YORK_SERVER_TIME", "OTHER_SERVER_TIME"
    };
    return (b >= 0 && b < 4) ? names[b] : "ALL";
}

inline bool get_session_bounds(int b, int asia_end, int london_end, int newyork_end,
                               int& start_hour, int& end_hour) {
    if (b < 0 || b > 3) return false;
    int starts[] = {0, asia_end, london_end, newyork_end};
    int ends[]   = {asia_end, london_end, newyork_end, 24};
    start_hour = starts[b];
    end_hour = ends[b];
    return true;
}

} // namespace rza