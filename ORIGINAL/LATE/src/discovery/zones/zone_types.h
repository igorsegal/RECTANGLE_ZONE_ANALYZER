#pragma once
// ============================================================================
// zone_types.h — Типы данных для поиска зон (Блок 01)
// ============================================================================
// Одна ответственность: объявление структур ZoneRecord, BarStats, ZonesResult.
// Параметры поиска (ZoneSearchParams) находятся в core/params.h.
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include "core/types.h"

namespace rza {

// ============================================================================
// Статистика одного бара
// ============================================================================
struct BarStats {
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;

    double range_price = 0.0;
    double body_price = 0.0;
    double upper_wick_price = 0.0;
    double lower_wick_price = 0.0;

    double range_atr = -1.0;
    double body_atr = -1.0;
    double body_percent = 0.0;
    double upper_wick_percent = 0.0;
    double lower_wick_percent = 0.0;
};

// ============================================================================
// Запись одной зоны
// ============================================================================
struct ZoneRecord {
    int32_t      id = -1;
    std::string  zone_id;
    ZoneType     type = ZoneType::BULL;

    std::string  candidate_status;
    std::string  reject_reason;
    std::string  blocking_zone_id;
    std::string  final_status;

    int64_t      source_time = 0;
    int64_t      confirm_bar_time = 0;
    int64_t      confirmation_time = 0;
    int64_t      raw_breakout_time = 0;
    int64_t      broken_time = 0;
    int64_t      end_time = 0;

    double       zone_low = 0.0;
    double       zone_high = 0.0;
    double       zone_height_price = 0.0;
    double       zone_height_points = 0.0;
    double       zone_height_atr = -1.0;

    double       atr_value = 0.0;

    double       required_gap_price = 0.0;
    double       actual_gap_price = -1.0;

    double       breakout_threshold_price = 0.0;
    bool         raw_breakout = false;
    double       raw_breakout_close = 0.0;
    bool         threshold_breakout = false;
    double       breakout_close = 0.0;
    double       breakout_distance_price = 0.0;
    double       breakout_distance_atr = -1.0;

    double       lifetime_minutes = -1.0;
    double       lifetime_hours = -1.0;
    int32_t      lifetime_bars = -1;

    int32_t      source_idx = -1;
    int32_t      confirm_idx = -1;
    int32_t      confirmation_decision_shift = -1;

    BarStats     source_bar;
    BarStats     confirm_bar;
};

// ============================================================================
// Результат работы Блока 01
// ============================================================================
struct ZonesResult {
    bool                      success = false;
    std::string               error_message;

    std::vector<ZoneRecord>   zones;

    int32_t  total_candidates = 0;
    int32_t  accepted_zones = 0;
    int32_t  rejected_zones = 0;
    int32_t  accepted_bull = 0;
    int32_t  accepted_bear = 0;
    int32_t  broken_zones = 0;
    int32_t  end_active_zones = 0;
    int32_t  total_bars_processed = 0;
};

} // namespace rza