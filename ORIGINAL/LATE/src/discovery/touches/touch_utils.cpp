// ============================================================================
// touch_utils.cpp — Реализация вспомогательных функций
// ============================================================================

#include "discovery/touches/touch_utils.h"
#include <cmath>
#include <algorithm>

namespace rza {

void TouchUtils::fill_bar_stats(const MqlRates& bar, double atr_value, TouchBarStats& stats) {
    stats.open = bar.open;
    stats.high = bar.high;
    stats.low = bar.low;
    stats.close = bar.close;

    stats.range_price = stats.high - stats.low;
    stats.body_price = std::abs(stats.close - stats.open);
    stats.upper_wick_price = stats.high - std::max(stats.open, stats.close);
    stats.lower_wick_price = std::min(stats.open, stats.close) - stats.low;

    if (stats.upper_wick_price < 0.0) stats.upper_wick_price = 0.0;
    if (stats.lower_wick_price < 0.0) stats.lower_wick_price = 0.0;

    if (atr_value > 0.0) {
        stats.range_atr = stats.range_price / atr_value;
        stats.body_atr = stats.body_price / atr_value;
    } else {
        stats.range_atr = -1.0;
        stats.body_atr = -1.0;
    }

    if (stats.range_price > 0.0) {
        stats.body_percent = 100.0 * stats.body_price / stats.range_price;
        stats.upper_wick_percent = 100.0 * stats.upper_wick_price / stats.range_price;
        stats.lower_wick_percent = 100.0 * stats.lower_wick_price / stats.range_price;
    } else {
        stats.body_percent = 0.0;
        stats.upper_wick_percent = 0.0;
        stats.lower_wick_percent = 0.0;
    }

    double difference = stats.close - stats.open;
    if (std::abs(difference) < 0.00001)
        stats.direction = "DOJI";
    else if (difference > 0.0)
        stats.direction = "BULLISH";
    else
        stats.direction = "BEARISH";
}

void TouchUtils::calculate_approach_window(
    const std::vector<MqlRates>& bars,
    int touch_idx,
    int window_bars,
    bool bullish_zone,
    double atr_value,
    double& net_move_atr,
    double& range_atr)
{
    net_move_atr = -1.0;
    range_atr = -1.0;

    if (window_bars <= 0 || touch_idx + window_bars >= static_cast<int>(bars.size()))
        return;

    double touch_close = bars[touch_idx].close;
    double start_close = bars[touch_idx + window_bars].close;

    double net_move_price;
    if (bullish_zone)
        net_move_price = start_close - touch_close;
    else
        net_move_price = touch_close - start_close;

    if (atr_value > 0.0)
        net_move_atr = net_move_price / atr_value;

    double highest = -1.0e100;
    double lowest = 1.0e100;

    for (int j = 0; j < window_bars; ++j) {
        int idx = touch_idx + j;
        if (bars[idx].high > highest) highest = bars[idx].high;
        if (bars[idx].low < lowest) lowest = bars[idx].low;
    }

    if (atr_value > 0.0)
        range_atr = (highest - lowest) / atr_value;
}

void TouchUtils::select_structural_targets(
    const std::vector<ZoneRecord>& zones,
    const std::vector<int>& active_zone_indexes,
    int source_zone_idx,
    int64_t touch_decision_time,
    int& reversal_target_idx,
    int& breakout_target_idx)
{
    reversal_target_idx = -1;
    breakout_target_idx = -1;

    double nearest_reversal_distance = 1.0e100;
    double nearest_breakout_distance = 1.0e100;

    bool source_bull = (zones[source_zone_idx].type == ZoneType::BULL);

    for (int candidate_idx : active_zone_indexes) {
        if (candidate_idx == source_zone_idx) continue;

        const ZoneRecord& candidate = zones[candidate_idx];

        if (candidate.confirmation_time >= touch_decision_time) continue;
        if (candidate.broken_time > 0 && candidate.broken_time <= touch_decision_time) continue;

        bool candidate_bull = (candidate.type == ZoneType::BULL);
        double distance = 0.0;

        if (source_bull) {
            if (!candidate_bull && candidate.zone_low >= zones[source_zone_idx].zone_high) {
                distance = candidate.zone_low - zones[source_zone_idx].zone_high;
                if (distance < nearest_reversal_distance) {
                    nearest_reversal_distance = distance;
                    reversal_target_idx = candidate_idx;
                }
            }

            if (candidate_bull && candidate.zone_high <= zones[source_zone_idx].zone_low) {
                distance = zones[source_zone_idx].zone_low - candidate.zone_high;
                if (distance < nearest_breakout_distance) {
                    nearest_breakout_distance = distance;
                    breakout_target_idx = candidate_idx;
                }
            }
        } else {
            if (candidate_bull && candidate.zone_high <= zones[source_zone_idx].zone_low) {
                distance = zones[source_zone_idx].zone_low - candidate.zone_high;
                if (distance < nearest_reversal_distance) {
                    nearest_reversal_distance = distance;
                    reversal_target_idx = candidate_idx;
                }
            }

            if (!candidate_bull && candidate.zone_low >= zones[source_zone_idx].zone_high) {
                distance = candidate.zone_low - zones[source_zone_idx].zone_high;
                if (distance < nearest_breakout_distance) {
                    nearest_breakout_distance = distance;
                    breakout_target_idx = candidate_idx;
                }
            }
        }
    }
}

void TouchUtils::update_mfe_mae(TouchRecord& touch, const MqlRates& bar) {
    double close_price = bar.close;

    double favorable_close;
    double adverse_close;

    if (touch.zone_type == "BULL") {
        favorable_close = close_price - touch.touch_close;
        adverse_close = touch.touch_close - close_price;
    } else {
        favorable_close = touch.touch_close - close_price;
        adverse_close = close_price - touch.touch_close;
    }

    if (favorable_close < 0.0) favorable_close = 0.0;
    if (adverse_close < 0.0) adverse_close = 0.0;

    double current_mfe = touch.mfe_close_atr < 0.0 ? 0.0 : touch.mfe_close_atr * touch.touch_atr;
    double current_mae = touch.mae_close_atr < 0.0 ? 0.0 : touch.mae_close_atr * touch.touch_atr;

    if (favorable_close > current_mfe) {
        if (touch.touch_atr > 0.0)
            touch.mfe_close_atr = favorable_close / touch.touch_atr;
    }

    if (adverse_close > current_mae) {
        if (touch.touch_atr > 0.0)
            touch.mae_close_atr = adverse_close / touch.touch_atr;
    }
}

} // namespace rza