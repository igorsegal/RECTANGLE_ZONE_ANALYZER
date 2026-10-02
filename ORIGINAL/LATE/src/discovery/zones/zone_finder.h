#pragma once
// ============================================================================
// zone_finder.h — Класс для поиска зон (Блок 01)
// ============================================================================
// Одна ответственность: поиск прямоугольных зон по массиву баров.
// Параметры поиска (ZoneSearchParams) находятся в core/params.h.
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "core/params.h"                    // ← ИСПРАВЛЕНИЕ: для ZoneSearchParams
#include "discovery/zones/zone_types.h"

namespace rza {

class ZoneFinder {
public:
    static ZonesResult find_zones(
        const std::vector<MqlRates>& bars,
        const std::vector<double>& atr,
        const ZoneSearchParams& params,
        int period_seconds,
        double point,
        int digits,
        const std::string& symbol,
        const std::string& timeframe
    );

    static void fill_bar_stats(const MqlRates& bar, double atr_value, BarStats& stats);
    static double get_zones_gap(double first_low, double first_high,
                                double second_low, double second_high);

private:
    static void process_breakouts_on_bar(
        std::vector<ZoneRecord>& zones,
        std::vector<int>& active_zone_indexes,
        int closed_bar_idx,
        int64_t decision_time,
        const std::vector<MqlRates>& bars,
        int& broken_count
    );

    static int add_candidate(
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
        const std::string& timeframe
    );

    static void evaluate_candidate_distance(
        std::vector<ZoneRecord>& zones,
        const std::vector<int>& active_zone_indexes,
        int candidate_idx,
        int& rejected_count
    );
};

} // namespace rza