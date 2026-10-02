#pragma once
// ============================================================================
// touch_types.h — Типы данных для поиска касаний (Блок 02)
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include "core/types.h"
#include "discovery/zones/zone_types.h"

namespace rza {

// ============================================================================
// Статистика бара касания
// ============================================================================
struct TouchBarStats {
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

    std::string direction;
};

// ============================================================================
// Запись одного касания
// ============================================================================
struct TouchRecord {
    std::string  touch_id;
    std::string  zone_id;
    std::string  symbol;
    std::string  timeframe;
    std::string  zone_type;

    int32_t      entry_number = 0;
    int32_t      touch_number = 0;
    std::string  touch_status;
    std::string  entry_direction;

    int64_t      touch_bar_time = 0;
    int64_t      touch_decision_time = 0;
    int32_t      touch_bar_idx = -1;
    int32_t      touch_decision_idx = -1;

    double       zone_age_calendar_minutes = 0.0;
    double       zone_age_calendar_hours = 0.0;
    int32_t      zone_age_bars = 0;
    double       zone_age_trading_hours = 0.0;

    double       zone_low = 0.0;
    double       zone_high = 0.0;
    double       zone_height_price = 0.0;
    double       zone_height_points = 0.0;
    double       zone_height_atr = -1.0;

    double       touch_atr = 0.0;
    double       touch_close = 0.0;

    double       touch_depth_price = 0.0;
    double       touch_depth_points = 0.0;
    double       touch_depth_percent = 0.0;

    TouchBarStats touch_bar;
    TouchBarStats previous_bar;

    double       approach_gap_price = 0.0;
    double       approach_gap_points = 0.0;
    double       approach_gap_atr = -1.0;
    double       approach_net_move_atr5 = -1.0;
    double       approach_range_atr5 = -1.0;

    int32_t      reversal_target_idx = -1;
    std::string  reversal_target_zone_id = "NONE";
    double       reversal_target_low = -1.0;
    double       reversal_target_high = -1.0;
    double       distance_to_reversal_target_price = -1.0;
    double       distance_to_reversal_target_atr = -1.0;
    std::string  reversal_target_status = "NONE";

    int32_t      breakout_target_idx = -1;
    std::string  breakout_target_zone_id = "NONE";
    double       breakout_target_low = -1.0;
    double       breakout_target_high = -1.0;
    double       distance_to_breakout_target_price = -1.0;
    double       distance_to_breakout_target_atr = -1.0;
    std::string  breakout_target_status = "NONE";

    double       primary_reaction_distance_price = 0.0;
    double       primary_reaction_distance_atr = -1.0;
    double       local_breakout_threshold_price = 0.0;
    double       local_breakout_threshold_atr = -1.0;

    std::string  local_first_result;
    int64_t      local_observation_end_time = 0;
    int32_t      local_observation_bars = -1;
    std::string  local_end_reason;

    std::string  structural_first_result;
    int64_t      structural_observation_end_time = 0;
    int32_t      structural_observation_bars = -1;
    std::string  structural_end_reason;

    double       mfe_close_atr = -1.0;
    double       mae_close_atr = -1.0;

    int64_t      local_obs_end_time = 0;
};

// ============================================================================
// Результат работы Блока 02
// ============================================================================
struct TouchesResult {
    bool                      success = false;
    std::string               error_message;

    std::vector<TouchRecord>  touches;

    int32_t  total_touches = 0;
    int32_t  expected_touches = 0;
    int32_t  wrong_side_entries = 0;
    int32_t  local_reversal_first = 0;
    int32_t  local_breakout_first = 0;
    int32_t  local_timeouts = 0;
    int32_t  structural_opposite_first = 0;
    int32_t  structural_same_type_first = 0;
    int32_t  structural_no_targets = 0;
    int32_t  structural_targets_broken = 0;
    int32_t  structural_timeouts = 0;
    int32_t  total_bars_processed = 0;
};

} // namespace rza