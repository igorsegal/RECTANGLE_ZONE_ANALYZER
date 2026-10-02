#pragma once
// ============================================================================
// strict_monitor.h — Блок 1: Монитор рынка
// ============================================================================

#include <vector>
#include "core/types.h"
#include "backtest/strict/strict_types.h"
#include "discovery/zones/zone_types.h"

namespace rza {

class StrictMonitor {
public:
    static std::vector<ZoneState> init_zones(const std::vector<ZoneRecord>& discovery_zones);
    
    static std::vector<TouchEvent> scan_bar(
        const MqlRates& bar,
        int bar_idx,
        std::vector<ZoneState>& zones,
        double atr_value);

private:
    static bool price_touches_zone(double close_price, double zone_high, double zone_low);
    static bool zone_is_broken(const MqlRates& bar, const ZoneState& zone);
    static void update_touch_count(ZoneState& zone, bool touching, bool was_touching_prev);
};

} // namespace rza