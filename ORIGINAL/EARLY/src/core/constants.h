#pragma once
// ============================================================================
// constants.h — Глобальные константы системы
// ============================================================================

#include <cstdint>

namespace rza {

// ============================================================================
// Версия проекта
// ============================================================================
constexpr const char* PROJECT_VERSION = "1.0.0-dev";
constexpr int         PROJECT_VERSION_MAJOR = 1;
constexpr int         PROJECT_VERSION_MINOR = 0;
constexpr int         PROJECT_VERSION_PATCH = 0;

// ============================================================================
// Пути по умолчанию
// ============================================================================
constexpr const char* DEFAULT_DATA_DIR    = "D:/AHexaTrader/1DataFiles/raw";
constexpr const char* DEFAULT_RESULTS_DIR = "D:/AHexaTrader/2026.08.18 RECTANGLE_ZONE_ANALYZER/results";

// ============================================================================
// Параметры производительности
// ============================================================================
constexpr int DEFAULT_NUM_THREADS        = 2;
constexpr int PROGRESS_UPDATE_MS         = 200;
constexpr int PROGRESS_BAR_WIDTH         = 40;
constexpr int PROGRESS_EVERY_BARS        = 5000;

// ============================================================================
// Размеры бакетов (должны совпадать с MQL4)
// ============================================================================
constexpr int AGE_BUCKETS        = 9;
constexpr int TOUCH_BUCKETS      = 5;
constexpr int DEPTH_BUCKETS      = 4;
constexpr int APPROACH_BUCKETS   = 4;
constexpr int HEIGHT_BUCKETS     = 5;
constexpr int SESSION_BUCKETS    = 4;
constexpr int ZONE_TYPES         = 2;
constexpr int PATTERN_TYPES      = 7;

// ============================================================================
// Общее число кандидатов паттернов
// ============================================================================
constexpr int TOTAL_PATTERN_CANDIDATES = 864;

// ============================================================================
// Границы типов таймфреймов
// ============================================================================
constexpr int TF_M1_SEC  = 60;
constexpr int TF_M5_SEC  = 300;
constexpr int TF_M15_SEC = 900;
constexpr int TF_M30_SEC = 1800;
constexpr int TF_H1_SEC  = 3600;
constexpr int TF_H4_SEC  = 14400;
constexpr int TF_D1_SEC  = 86400;
constexpr int TF_W1_SEC  = 604800;
constexpr int TF_MN1_SEC = 2592000;

// ============================================================================
// Формат XFBAR
// ============================================================================
constexpr const char* XFBAR_MAGIC       = "XFBAR001";
constexpr int         XFBAR_VERSION     = 1;
constexpr int         XFBAR_RECORD_SIZE = 60;
constexpr int         XFBAR_MAX_SYMBOL_LEN = 64;

// ============================================================================
// Торговые сессии (часы серверного времени)
// ============================================================================
constexpr int DEFAULT_ASIA_END_HOUR     = 7;
constexpr int DEFAULT_LONDON_END_HOUR   = 13;
constexpr int DEFAULT_NEWYORK_END_HOUR  = 21;

// ============================================================================
// Ограничения бэктеста
// ============================================================================
constexpr double DEFAULT_DEPOSIT            = 100000.0;
constexpr double DEFAULT_RISK_PCT           = 1.0;
constexpr double DEFAULT_COMMISSION_PER_LOT = 4.0;   // 4$ на сторону (откр+закр = 8$)
constexpr double DEFAULT_MIN_LOT            = 0.01;
constexpr double DEFAULT_LOT_STEP           = 0.01;
constexpr double DEFAULT_SLIPPAGE_POINTS    = 0.0;
constexpr int    DEFAULT_MAX_CONCURRENT_POS = 1;

// ============================================================================
// Математические константы
// ============================================================================
constexpr double WILSON_Z_95      = 1.959963984540054;
constexpr double EPSILON          = 1.0e-10;
constexpr double INFINITY_PRICE   = 1.0e100;
constexpr double INVALID_VALUE    = -1.0;

// ============================================================================
// Ограничения оптимизации
// ============================================================================
constexpr int MIN_HISTORY_BARS    = 100;
constexpr int MIN_SAMPLES_PATTERN = 100;
constexpr int STRONG_SAMPLES      = 300;
constexpr int MIN_OOS_SAMPLES     = 20;

} // namespace rza