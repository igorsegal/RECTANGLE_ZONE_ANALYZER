#pragma once
// ============================================================================
// touch_utils.h — Вспомогательные функции для поиска касаний
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "discovery/zones/zone_types.h"
#include "discovery/touches/touch_types.h"

namespace rza {

class TouchUtils {
public:
    static void fill_bar_stats(const MqlRates& bar, double atr_value, TouchBarStats& stats);

    static void calculate_approach_window(
        const std::vector<MqlRates>& bars,
        int touch_idx,
        int window_bars,
        bool bullish_zone,
        double atr_value,
        double& net_move_atr,
        double& range_atr
    );

    static void select_structural_targets(
        const std::vector<ZoneRecord>& zones,
        const std::vector<int>& active_zone_indexes,
        int source_zone_idx,
        int64_t touch_decision_time,
        int& reversal_target_idx,
        int& breakout_target_idx
    );

    static void update_mfe_mae(
        TouchRecord& touch,
        const MqlRates& bar
    );
};

} // namespace rza