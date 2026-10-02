// ============================================================================
// touch_results.cpp — Реализация обновления результатов
// ============================================================================

#include "discovery/touches/touch_results.h"

namespace rza {

void TouchResults::update_local_result(
    TouchRecord& touch,
    const MqlRates& bar,
    int64_t decision_time,
    int observation_bars,
    const TouchSearchParams& params,
    int& local_reversal_count,
    int& local_breakout_count,
    int& local_timeout_count)
{
    double close_price = bar.close;
    bool bullish = (touch.zone_type == "BULL");

    double favorable_from_boundary;
    double adverse_beyond_boundary;

    if (bullish) {
        favorable_from_boundary = close_price - touch.zone_high;
        adverse_beyond_boundary = touch.zone_low - close_price;
    } else {
        favorable_from_boundary = touch.zone_low - close_price;
        adverse_beyond_boundary = close_price - touch.zone_high;
    }

    if (touch.local_first_result.empty() && favorable_from_boundary >= touch.primary_reaction_distance_price) {
        touch.local_first_result = "REVERSAL_FIRST";
        touch.local_observation_end_time = decision_time;
        touch.local_observation_bars = observation_bars;
        touch.local_end_reason = "PRIMARY_REVERSAL";
        local_reversal_count++;
    }

    if (touch.local_first_result.empty() && adverse_beyond_boundary >= touch.local_breakout_threshold_price) {
        touch.local_first_result = "BREAKOUT_FIRST";
        touch.local_observation_end_time = decision_time;
        touch.local_observation_bars = observation_bars;
        touch.local_end_reason = "LOCAL_BREAKOUT";
        local_breakout_count++;
    }

    if (touch.local_first_result.empty() && observation_bars >= params.max_local_bars) {
        touch.local_first_result = "TIMEOUT";
        touch.local_observation_end_time = decision_time;
        touch.local_observation_bars = observation_bars;
        touch.local_end_reason = "MAX_LOCAL_BARS";
        local_timeout_count++;
    }
}

void TouchResults::update_structural_result(
    TouchRecord& touch,
    const std::vector<ZoneRecord>& zones,
    const MqlRates& bar,
    int64_t decision_time,
    int observation_bars,
    const TouchSearchParams& params,
    int& struct_opposite_count,
    int& struct_same_type_count,
    int& struct_targets_broken_count,
    int& struct_timeout_count)
{
    if (!touch.structural_first_result.empty()) return;

    double close_price = bar.close;

    if (touch.reversal_target_idx >= 0 && touch.reversal_target_status == "ACTIVE") {
        if (close_price >= zones[touch.reversal_target_idx].zone_low &&
            close_price <= zones[touch.reversal_target_idx].zone_high) {
            touch.reversal_target_status = "REACHED";
            touch.structural_first_result = "OPPOSITE_ZONE_FIRST";
            touch.structural_observation_end_time = decision_time;
            touch.structural_observation_bars = observation_bars;
            touch.structural_end_reason = "OPPOSITE_ZONE_REACHED";
            struct_opposite_count++;
            return;
        }
    }

    if (touch.breakout_target_idx >= 0 && touch.breakout_target_status == "ACTIVE") {
        if (close_price >= zones[touch.breakout_target_idx].zone_low &&
            close_price <= zones[touch.breakout_target_idx].zone_high) {
            touch.breakout_target_status = "REACHED";
            touch.structural_first_result = "SAME_TYPE_ZONE_FIRST";
            touch.structural_observation_end_time = decision_time;
            touch.structural_observation_bars = observation_bars;
            touch.structural_end_reason = "SAME_TYPE_ZONE_REACHED";
            struct_same_type_count++;
            return;
        }
    }

    bool reversal_possible = (touch.reversal_target_idx >= 0 && touch.reversal_target_status == "ACTIVE");
    bool breakout_possible = (touch.breakout_target_idx >= 0 && touch.breakout_target_status == "ACTIVE");

    if (touch.reversal_target_idx >= 0 && touch.reversal_target_status == "ACTIVE") {
        if (zones[touch.reversal_target_idx].broken_time > 0 &&
            zones[touch.reversal_target_idx].broken_time <= decision_time) {
            touch.reversal_target_status = "BROKEN_BEFORE_REACH";
            reversal_possible = false;
        }
    }

    if (touch.breakout_target_idx >= 0 && touch.breakout_target_status == "ACTIVE") {
        if (zones[touch.breakout_target_idx].broken_time > 0 &&
            zones[touch.breakout_target_idx].broken_time <= decision_time) {
            touch.breakout_target_status = "BROKEN_BEFORE_REACH";
            breakout_possible = false;
        }
    }

    if (!reversal_possible && !breakout_possible) {
        touch.structural_first_result = "TARGETS_BROKEN";
        touch.structural_observation_end_time = decision_time;
        touch.structural_observation_bars = observation_bars;
        touch.structural_end_reason = "TARGETS_BROKEN";
        struct_targets_broken_count++;
        return;
    }

    if (observation_bars >= params.max_structural_bars) {
        touch.structural_first_result = "TIMEOUT";
        touch.structural_observation_end_time = decision_time;
        touch.structural_observation_bars = observation_bars;
        touch.structural_end_reason = "MAX_STRUCTURAL_BARS";
        struct_timeout_count++;
    }
}

} // namespace rza