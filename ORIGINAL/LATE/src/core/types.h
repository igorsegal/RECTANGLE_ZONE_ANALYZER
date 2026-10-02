#pragma once
// ============================================================================
// types.h — Все структуры данных системы RectangleZoneAnalyzer
// ============================================================================
// Одна ответственность: объявление всех enum и структур данных,
// которые используются во всём проекте.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace rza {

// ============================================================================
// 1. СТРУКТУРЫ БИНАРНОГО ФОРМАТА XFBAR (байт-в-байт)
// ============================================================================

#pragma pack(push, 1)

struct FileHeader {
    char     magic[8];
    int32_t  version;
    int32_t  record_size;
    int32_t  period_seconds;
    int32_t  digits;
    double   point;
    int64_t  bar_count;
    int64_t  first_time;
    int64_t  last_time;
    int32_t  symbol_len;
};

struct MqlRates {
    int64_t  time;
    double   open;
    double   high;
    double   low;
    double   close;
    int64_t  tick_volume;
    int32_t  spread;
    int64_t  real_volume;
};

#pragma pack(pop)

static_assert(sizeof(MqlRates) == 60, "MqlRates must be 60 bytes");

// ============================================================================
// 2. БАЗОВЫЕ ENUM
// ============================================================================

enum class ZoneType : int {
    BULL = 0,
    BEAR = 1,
    COUNT = 2
};

enum class ZoneStatus : int {
    ACTIVE = 0,
    BROKEN = 1,
    END_OF_HISTORY = 2,
    REJECTED = 3
};

enum class TouchStatus : int {
    TOUCH = 0,
    REENTRY = 1
};

enum class LocalResult : int {
    UNDECIDED = 0,
    REVERSAL_FIRST = 1,
    BREAKOUT_FIRST = 2,
    TIMEOUT = 3,
    END_OF_HISTORY = 4
};

enum class StructuralResult : int {
    UNDECIDED = 0,
    OPPOSITE_ZONE_FIRST = 1,
    SAME_TYPE_ZONE_FIRST = 2,
    TARGETS_BROKEN = 3,
    TIMEOUT = 4,
    NO_TARGETS = 5,
    END_OF_HISTORY = 6
};

enum class PatternType : int {
    AGE_ONLY = 0,
    AGE_TOUCH = 1,
    AGE_DEPTH = 2,
    AGE_TOUCH_DEPTH = 3,
    AGE_APPROACH = 4,
    AGE_HEIGHT = 5,
    AGE_SESSION = 6,
    COUNT = 7
};

enum class SignalDirection : int {
    BUY = 0,
    SELL = 1
};

enum class TargetMode : int {
    PRIMARY_REACTION_DISTANCE = 0,
    NEXT_OPPOSITE_ZONE = 1
};

enum class RuleType : int {
    LOCAL_REVERSAL = 0,
    STRUCTURAL_REVERSAL = 1
};

enum class ExitReason : int {
    TP_REACTION = 0,
    TP_STRUCTURAL = 1,
    SL_BREAKOUT = 2,
    TIMEOUT = 3,
    END_OF_HISTORY = 4
};

enum class OOSGrade : int {
    CONFIRMED_TWO_FOLDS = 0,
    REPEATED_MIXED = 1,
    SINGLE_FOLD_PASS = 2,
    OOS_FAILED = 3,
    INSUFFICIENT_OOS = 4
};

enum class RuleGrade : int {
    A_STRONG_CONFIRMED = 0,
    B_CONFIRMED = 1,
    C_CONFIRMED_SMALL = 2,
    D_CONFIRMED = 3
};

enum class LogLevel : int {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

// ============================================================================
// 3. ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

inline const char* zone_type_str(ZoneType t) {
    return t == ZoneType::BULL ? "BULL" : "BEAR";
}

inline const char* timeframe_str(int period_seconds) {
    switch (period_seconds) {
        case 60:     return "M1";
        case 300:    return "M5";
        case 900:    return "M15";
        case 1800:   return "M30";
        case 3600:   return "H1";
        case 14400:  return "H4";
        case 86400:  return "D1";
        case 604800: return "W1";
        case 2592000:return "MN1";
        default:     return "??";
    }
}

inline int timeframe_to_seconds(const std::string& tf) {
    if (tf == "M1")  return 60;
    if (tf == "M5")  return 300;
    if (tf == "M15") return 900;
    if (tf == "M30") return 1800;
    if (tf == "H1")  return 3600;
    if (tf == "H4")  return 14400;
    if (tf == "D1")  return 86400;
    if (tf == "W1")  return 604800;
    if (tf == "MN1") return 2592000;
    return 0;
}

inline const char* log_level_str(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "?????";
    }
}

} // namespace rza