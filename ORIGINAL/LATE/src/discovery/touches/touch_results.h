#pragma once
// ============================================================================
// touch_results.h — Обновление результатов касаний
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "core/params.h"
#include "discovery/zones/zone_types.h"
#include "discovery/touches/touch_types.h"

namespace rza {

class TouchResults {
public:
    static void update_local_result(
        TouchRecord& touch,
        const MqlRates& bar,
        int64_t decision_time,
        int observation_bars,
        const TouchSearchParams& params,
        int& local_reversal_count,
        int& local_breakout_count,
        int& local_timeout_count
    );

    static void update_structural_result(
        TouchRecord& touch,
        const std::vector<ZoneRecord>& zones,
        const MqlRates& bar,
        int64_t decision_time,
        int observation_bars,
        const TouchSearchParams& params,
        int& struct_opposite_count,
        int& struct_same_type_count,
        int& struct_targets_broken_count,
        int& struct_timeout_count
    );
};

} // namespace rza