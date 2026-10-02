// ============================================================================
// zone_finder.cpp — Реализация поиска зон (Блок 01)
// ============================================================================

#include "discovery/zones/zone_finder.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace rza {

// ============================================================================
// Вспомогательные функции
// ============================================================================

static std::string time_to_id(int64_t unix_time) {
    if (unix_time <= 0) return "";
    time_t t = static_cast<time_t>(unix_time);
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_buf.tm_year + 1900)
        << std::setw(2) << (tm_buf.tm_mon + 1)
        << std::setw(2) << tm_buf.tm_mday
        << "_"
        << std::setw(2) << tm_buf.tm_hour
        << std::setw(2) << tm_buf.tm_min;
    return oss.str();
}

static std::string clean_identifier(const std::string& value) {
    std::string result = value;
    for (char& c : result) {
        if (c == ' ' || c == ';' || c == '/' || c == '\\' || c == ':') {
            c = '_';
        }
    }
    return result;
}

static std::string build_zone_id(const std::string& symbol,
                                  const std::string& timeframe,
                                  ZoneType type,
                                  int64_t source_time,
                                  int64_t confirmation_time) {
    return clean_identifier(symbol) + "_" + timeframe + "_" +
           (type == ZoneType::BULL ? "BULL" : "BEAR") + "_" +
           time_to_id(source_time) + "_" + time_to_id(confirmation_time);
}

// ============================================================================
// Заполнение статистики бара
// ============================================================================
void ZoneFinder::fill_bar_stats(const MqlRates& bar, double atr_value, BarStats& stats) {
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
}

// ============================================================================
// Расстояние между зонами
// ============================================================================
double ZoneFinder::get_zones_gap(double first_low, double first_high,
                                  double second_low, double second_high) {
    if (first_low > second_high) return first_low - second_high;
    if (second_low > first_high) return second_low - first_high;
    return 0.0;
}

// ============================================================================
// Обработка пробитий активных зон
// ============================================================================
void ZoneFinder::process_breakouts_on_bar(
    std::vector<ZoneRecord>& zones,
    std::vector<int>& active_zone_indexes,
    int closed_bar_idx,
    int64_t decision_time,
    const std::vector<MqlRates>& bars,
    int& broken_count)
{
    double close_price = bars[closed_bar_idx].close;
    size_t position = 0;

    while (position < active_zone_indexes.size()) {
        int zone_idx = active_zone_indexes[position];
        ZoneRecord& z = zones[zone_idx];
        bool bullish = (z.type == ZoneType::BULL);

        bool raw_broken = false;
        bool threshold_broken = false;

        if (bullish) {
            raw_broken = (close_price < z.zone_low);
            threshold_broken = (close_price < z.zone_low - z.breakout_threshold_price);
        } else {
            raw_broken = (close_price > z.zone_high);
            threshold_broken = (close_price > z.zone_high + z.breakout_threshold_price);
        }

        if (raw_broken && !z.raw_breakout) {
            z.raw_breakout = true;
            z.raw_breakout_time = decision_time;
            z.raw_breakout_close = close_price;
        }

        if (threshold_broken) {
            z.threshold_breakout = true;
            z.broken_time = decision_time;
            z.breakout_close = close_price;

            double breakout_distance_price;
            if (bullish) {
                breakout_distance_price = z.zone_low - close_price;
            } else {
                breakout_distance_price = close_price - z.zone_high;
            }
            if (breakout_distance_price < 0.0) breakout_distance_price = 0.0;

            z.breakout_distance_price = breakout_distance_price;
            if (z.atr_value > 0.0) {
                z.breakout_distance_atr = breakout_distance_price / z.atr_value;
            }

            z.final_status = "BROKEN";
            z.end_time = decision_time;
            double seconds = static_cast<double>(decision_time - z.confirmation_time);
            if (seconds < 0.0) seconds = 0.0;
            z.lifetime_minutes = seconds / 60.0;
            z.lifetime_hours = seconds / 3600.0;
            z.lifetime_bars = closed_bar_idx - z.confirm_idx;
            if (z.lifetime_bars < 0) z.lifetime_bars = 0;

            broken_count++;

            for (size_t i = position; i + 1 < active_zone_indexes.size(); ++i) {
                active_zone_indexes[i] = active_zone_indexes[i + 1];
            }
            active_zone_indexes.pop_back();
            continue;
        }
        position++;
    }
}

// ============================================================================
// Добавление кандидата зоны
// ============================================================================
int ZoneFinder::add_candidate(
    std::vector<ZoneRecord>& zones,
    int source_idx,
    int confirm_idx,
    ZoneType type,
    const std::vector<MqlRates>& bars,
    const std::vector<double>& atr,
    const ZoneSearchParams& params,
    double point,
    int64_t confirmation_time,
    const std::string& symbol,
    const std::string& timeframe)
{
    int index = static_cast<int>(zones.size());
    zones.emplace_back();
    ZoneRecord& z = zones[index];

    z.id = index;
    z.type = type;
    z.source_idx = source_idx;
    z.confirm_idx = confirm_idx;

    z.source_time = bars[source_idx].time;
    z.confirm_bar_time = bars[confirm_idx].time;
    z.confirmation_time = confirmation_time;

    double atr_value = 0.0;
    if (confirm_idx >= 0 && confirm_idx < static_cast<int>(atr.size())) {
        atr_value = atr[confirm_idx];
    }
    z.atr_value = atr_value;

    fill_bar_stats(bars[source_idx], atr_value, z.source_bar);
    fill_bar_stats(bars[confirm_idx], atr_value, z.confirm_bar);

    z.zone_id = build_zone_id(symbol, timeframe, type, z.source_time, confirmation_time);

    z.zone_low = bars[source_idx].low;
    z.zone_high = bars[source_idx].high;
    z.zone_height_price = z.zone_high - z.zone_low;
    if (point > 1e-10) {
        z.zone_height_points = z.zone_height_price / point;
    } else {
        z.zone_height_points = z.zone_height_price / 0.0001;
    }
    if (atr_value > 0.0) {
        z.zone_height_atr = z.zone_height_price / atr_value;
    }

    double fixed_gap = params.min_gap_points * point;
    double atr_gap = atr_value * params.min_gap_atr;
    z.required_gap_price = std::max(fixed_gap, atr_gap);

    double fixed_breakout = params.breakout_points * point;
    double atr_breakout = atr_value * params.breakout_atr;
    z.breakout_threshold_price = std::max(fixed_breakout, atr_breakout);

    z.candidate_status = "";
    z.reject_reason = "";
    z.blocking_zone_id = "";
    z.final_status = "";

    z.confirmation_decision_shift = confirm_idx;

    return index;
}

// ============================================================================
// Проверка расстояния до активных зон
// ============================================================================
void ZoneFinder::evaluate_candidate_distance(
    std::vector<ZoneRecord>& zones,
    const std::vector<int>& active_zone_indexes,
    int candidate_idx,
    int& rejected_count)
{
    ZoneRecord& candidate = zones[candidate_idx];

    double minimum_gap = 1.0e100;
    int nearest_zone_idx = -1;

    for (int active_idx : active_zone_indexes) {
        const ZoneRecord& active = zones[active_idx];
        if (active.type != candidate.type) continue;

        double gap = get_zones_gap(
            candidate.zone_low, candidate.zone_high,
            active.zone_low, active.zone_high
        );

        if (gap < minimum_gap) {
            minimum_gap = gap;
            nearest_zone_idx = active_idx;
        }
    }

    if (nearest_zone_idx >= 0) {
        candidate.actual_gap_price = minimum_gap;
    }

    if (nearest_zone_idx >= 0 && minimum_gap < candidate.required_gap_price) {
        candidate.candidate_status = "REJECTED_DISTANCE";
        candidate.reject_reason = "MIN_GAP";
        candidate.blocking_zone_id = zones[nearest_zone_idx].zone_id;
        candidate.final_status = "REJECTED";
        rejected_count++;
        return;
    }

    candidate.candidate_status = "ACCEPTED";
    candidate.final_status = "ACTIVE";
}

// ============================================================================
// ГЛАВНЫЙ МЕТОД: поиск зон
// ============================================================================
ZonesResult ZoneFinder::find_zones(
    const std::vector<MqlRates>& bars,
    const std::vector<double>& atr,
    const ZoneSearchParams& params,
    int period_seconds,
    double point,
    int digits,
    const std::string& symbol,
    const std::string& timeframe)
{
    ZonesResult result;
    result.success = false;

    const int total_bars = static_cast<int>(bars.size());
    if (total_bars < 4) {
        result.error_message = "Not enough bars: " + std::to_string(total_bars);
        return result;
    }

    result.zones.reserve(total_bars / 10);
    std::vector<int> active_zone_indexes;
    active_zone_indexes.reserve(100);

    int processed = 0;
    for (int confirm_idx = 1; confirm_idx < total_bars - 1; ++confirm_idx) {
        int source_idx = confirm_idx - 1;

        int64_t decision_time = bars[confirm_idx].time + period_seconds;

        process_breakouts_on_bar(
            result.zones, active_zone_indexes,
            confirm_idx, decision_time, bars,
            result.broken_zones
        );

        double source_open = bars[source_idx].open;
        double source_close = bars[source_idx].close;
        double confirm_open = bars[confirm_idx].open;
        double confirm_close = bars[confirm_idx].close;

        bool bullish_candidate =
            (source_close < source_open) &&
            (confirm_close > confirm_open) &&
            (confirm_close > source_open);

        bool bearish_candidate =
            (source_close > source_open) &&
            (confirm_close < confirm_open) &&
            (confirm_close < source_open);

        if (bullish_candidate) {
            int candidate_idx = add_candidate(
                result.zones, source_idx, confirm_idx, ZoneType::BULL,
                bars, atr, params, point, decision_time, symbol, timeframe
            );
            result.total_candidates++;
            evaluate_candidate_distance(result.zones, active_zone_indexes,
                                        candidate_idx, result.rejected_zones);
            if (result.zones[candidate_idx].candidate_status == "ACCEPTED") {
                active_zone_indexes.push_back(candidate_idx);
                result.accepted_zones++;
                result.accepted_bull++;
            }
        }

        if (bearish_candidate) {
            int candidate_idx = add_candidate(
                result.zones, source_idx, confirm_idx, ZoneType::BEAR,
                bars, atr, params, point, decision_time, symbol, timeframe
            );
            result.total_candidates++;
            evaluate_candidate_distance(result.zones, active_zone_indexes,
                                        candidate_idx, result.rejected_zones);
            if (result.zones[candidate_idx].candidate_status == "ACCEPTED") {
                active_zone_indexes.push_back(candidate_idx);
                result.accepted_zones++;
                result.accepted_bear++;
            }
        }

        processed++;
    }

    int64_t end_time = bars[total_bars - 1].time + period_seconds;
    for (int active_idx : active_zone_indexes) {
        ZoneRecord& z = result.zones[active_idx];
        z.final_status = "END_OF_HISTORY";
        z.end_time = end_time;
        double seconds = static_cast<double>(end_time - z.confirmation_time);
        if (seconds < 0.0) seconds = 0.0;
        z.lifetime_minutes = seconds / 60.0;
        z.lifetime_hours = seconds / 3600.0;
        z.lifetime_bars = (total_bars - 1) - z.confirm_idx;
        if (z.lifetime_bars < 0) z.lifetime_bars = 0;
        result.end_active_zones++;
    }

    result.total_bars_processed = processed;
    result.success = true;
    return result;
}

} // namespace rza