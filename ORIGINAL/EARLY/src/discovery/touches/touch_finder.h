#pragma once
// ============================================================================
// touch_finder.h — Класс для поиска касаний (Блок 02)
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "core/params.h"
#include "discovery/zones/zone_types.h"
#include "discovery/touches/touch_types.h"

namespace rza {

class TouchFinder {
public:
    static TouchesResult find_touches(
        const std::vector<MqlRates>& bars,
        const std::vector<double>& atr,
        const std::vector<ZoneRecord>& zones,
        const TouchSearchParams& params,
        int period_seconds,
        double point,
        int digits,
        const std::string& symbol,
        const std::string& timeframe
    );
};

} // namespace rza