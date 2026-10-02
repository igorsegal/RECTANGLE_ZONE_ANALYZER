// ============================================================================
// touch_finder.cpp — Главный метод поиска касаний (Блок 02)
// ============================================================================

#include "discovery/touches/touch_finder.h"
#include "discovery/touches/touch_utils.h"
#include "discovery/touches/touch_results.h"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace rza {

TouchesResult TouchFinder::find_touches(
    const std::vector<MqlRates>& bars,
    const std::vector<double>& atr,
    const std::vector<ZoneRecord>& zones,
    const TouchSearchParams& params,
    int period_seconds,
    double point,
    int digits,
    const std::string& symbol,
    const std::string& timeframe)
{
    TouchesResult result;
    result.success = false;

    const int total_bars = static_cast<int>(bars.size());
    if (total_bars < 4) {
        result.error_message = "Not enough bars: " + std::to_string(total_bars);
        return result;
    }

    std::vector<int> active_touch_indexes;
    active_touch_indexes.reserve(100);

    int touch_counter = 0;

    for (int bar_idx = 0; bar_idx < total_bars; ++bar_idx) {
        const MqlRates& bar = bars[bar_idx];
        int64_t decision_time = bar.time + period_seconds;

        // 1. Проверяем касания для активных зон
        for (size_t z_idx = 0; z_idx < zones.size(); ++z_idx) {
            const ZoneRecord& zone = zones[z_idx];

            if (zone.final_status != "ACTIVE" && zone.final_status != "BROKEN" && zone.final_status != "END_OF_HISTORY")
                continue;

            if (bar.time < zone.confirmation_time) continue;
            if (zone.broken_time > 0 && bar.time >= zone.broken_time) continue;

            bool inside = (bar.close >= zone.zone_low && bar.close <= zone.zone_high);
            bool prev_inside = (bar_idx > 0) &&
                               (bars[bar_idx-1].close >= zone.zone_low &&
                                bars[bar_idx-1].close <= zone.zone_high);

            if (inside && !prev_inside) {
                TouchRecord touch;
                touch_counter++;
                touch.touch_id = symbol + "_" + timeframe + "_TOUCH_" + std::to_string(touch_counter);
                touch.zone_id = zone.zone_id;
                touch.symbol = symbol;
                touch.timeframe = timeframe;
                touch.zone_type = (zone.type == ZoneType::BULL) ? "BULL" : "BEAR";

                touch.entry_number = touch_counter;
                touch.touch_number = 1;
                touch.touch_status = "TOUCH";
                touch.entry_direction = "EXPECTED_SIDE";

                touch.touch_bar_time = bar.time;
                touch.touch_decision_time = decision_time;
                touch.touch_bar_idx = bar_idx;
                touch.touch_decision_idx = bar_idx;

                double age_seconds = static_cast<double>(bar.time - zone.confirmation_time);
                touch.zone_age_calendar_minutes = age_seconds / 60.0;
                touch.zone_age_calendar_hours = age_seconds / 3600.0;
                touch.zone_age_bars = bar_idx - zone.confirm_idx;

                touch.zone_low = zone.zone_low;
                touch.zone_high = zone.zone_high;
                touch.zone_height_price = zone.zone_height_price;
                touch.zone_height_points = zone.zone_height_points;
                touch.zone_height_atr = zone.zone_height_atr;

                double touch_atr = (bar_idx < static_cast<int>(atr.size())) ? atr[bar_idx] : 0.0;
                touch.touch_atr = touch_atr;
                touch.touch_close = bar.close;

                double zone_height = zone.zone_high - zone.zone_low;
                if (zone.type == ZoneType::BULL) {
                    touch.touch_depth_price = zone.zone_high - bar.close;
                } else {
                    touch.touch_depth_price = bar.close - zone.zone_low;
                }
                if (touch.touch_depth_price < 0.0) touch.touch_depth_price = 0.0;
                touch.touch_depth_points = (point > 0.0) ? touch.touch_depth_price / point : 0.0;
                if (zone_height > 0.0) {
                    touch.touch_depth_percent = 100.0 * touch.touch_depth_price / zone_height;
                }

                TouchUtils::fill_bar_stats(bar, touch_atr, touch.touch_bar);
                if (bar_idx > 0) {
                    TouchUtils::fill_bar_stats(bars[bar_idx-1], touch_atr, touch.previous_bar);
                }

                bool bullish_zone = (zone.type == ZoneType::BULL);
                TouchUtils::calculate_approach_window(bars, bar_idx, 5, bullish_zone, touch_atr,
                                         touch.approach_net_move_atr5, touch.approach_range_atr5);

                std::vector<int> all_active_zones;
                for (size_t i = 0; i < zones.size(); ++i) {
                    if (zones[i].final_status == "ACTIVE" || zones[i].final_status == "END_OF_HISTORY") {
                        all_active_zones.push_back(static_cast<int>(i));
                    }
                }
                TouchUtils::select_structural_targets(zones, all_active_zones, static_cast<int>(z_idx),
                                         decision_time, touch.reversal_target_idx, touch.breakout_target_idx);

                if (touch.reversal_target_idx >= 0) {
                    touch.reversal_target_zone_id = zones[touch.reversal_target_idx].zone_id;
                    touch.reversal_target_low = zones[touch.reversal_target_idx].zone_low;
                    touch.reversal_target_high = zones[touch.reversal_target_idx].zone_high;
                    touch.reversal_target_status = "ACTIVE";

                    if (zone.type == ZoneType::BULL) {
                        touch.distance_to_reversal_target_price = zones[touch.reversal_target_idx].zone_low - zone.zone_high;
                    } else {
                        touch.distance_to_reversal_target_price = zone.zone_low - zones[touch.reversal_target_idx].zone_high;
                    }
                    if (touch_atr > 0.0) {
                        touch.distance_to_reversal_target_atr = touch.distance_to_reversal_target_price / touch_atr;
                    }
                }

                if (touch.breakout_target_idx >= 0) {
                    touch.breakout_target_zone_id = zones[touch.breakout_target_idx].zone_id;
                    touch.breakout_target_low = zones[touch.breakout_target_idx].zone_low;
                    touch.breakout_target_high = zones[touch.breakout_target_idx].zone_high;
                    touch.breakout_target_status = "ACTIVE";

                    if (zone.type == ZoneType::BULL) {
                        touch.distance_to_breakout_target_price = zone.zone_low - zones[touch.breakout_target_idx].zone_high;
                    } else {
                        touch.distance_to_breakout_target_price = zones[touch.breakout_target_idx].zone_low - zone.zone_high;
                    }
                    if (touch_atr > 0.0) {
                        touch.distance_to_breakout_target_atr = touch.distance_to_breakout_target_price / touch_atr;
                    }
                }

                double reaction_distance = std::max(params.reaction_points * point,
                                                    params.reaction_atr * touch_atr);
                touch.primary_reaction_distance_price = reaction_distance;
                if (touch_atr > 0.0) {
                    touch.primary_reaction_distance_atr = reaction_distance / touch_atr;
                }

                double local_breakout_threshold = std::max(params.local_breakout_points * point,
                                                           params.local_breakout_atr * touch_atr);
                touch.local_breakout_threshold_price = local_breakout_threshold;
                if (touch_atr > 0.0) {
                    touch.local_breakout_threshold_atr = local_breakout_threshold / touch_atr;
                }

                result.touches.push_back(touch);
                active_touch_indexes.push_back(static_cast<int>(result.touches.size() - 1));
                result.total_touches++;
                result.expected_touches++;
            }
        }

        // 2. Обновляем MFE/MAE и результаты для активных касаний
        std::vector<int> next_active;
        for (int t_idx : active_touch_indexes) {
            TouchRecord& touch = result.touches[t_idx];

            TouchUtils::update_mfe_mae(touch, bar);

            int obs_bars = bar_idx - touch.touch_bar_idx;

            TouchResults::update_local_result(touch, bar, decision_time, obs_bars, params,
                               result.local_reversal_first, result.local_breakout_first,
                               result.local_timeouts);

            TouchResults::update_structural_result(touch, zones, bar, decision_time, obs_bars, params,
                                    result.structural_opposite_first, result.structural_same_type_first,
                                    result.structural_targets_broken, result.structural_timeouts);

            bool local_done = !touch.local_first_result.empty();
            bool struct_done = !touch.structural_first_result.empty();

            if (!local_done || !struct_done) {
                next_active.push_back(t_idx);
            }
        }
        active_touch_indexes = std::move(next_active);
    }

    result.total_bars_processed = total_bars;
    result.success = true;
    return result;
}

} // namespace rza