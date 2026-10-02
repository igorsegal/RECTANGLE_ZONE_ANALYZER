// ============================================================================
// strict_monitor.cpp — Реализация монитора рынка
// ============================================================================

#include "backtest/strict/strict_monitor.h"
#include <cmath>

namespace rza {

std::vector<ZoneState> StrictMonitor::init_zones(
    const std::vector<ZoneRecord>& discovery_zones)
{
    std::vector<ZoneState> zones;
    zones.reserve(discovery_zones.size());

    for (size_t i = 0; i < discovery_zones.size(); ++i) {
        const ZoneRecord& zr = discovery_zones[i];

        ZoneState zs;
        zs.zone_id = zr.id;
        zs.zone_type = zr.type;
        zs.zone_high = zr.zone_high;
        zs.zone_low = zr.zone_low;
        zs.zone_height = zr.zone_height_price;
        zs.confirmation_time = zr.confirmation_time;
        zs.is_active = true;
        zs.is_broken = false;
        zs.touch_count = 0;
        zs.price_left_zone = false;
        zones.push_back(zs);
    }

    return zones;
}

bool StrictMonitor::price_touches_zone(double close_price, double zone_high, double zone_low) {
    return close_price >= zone_low && close_price <= zone_high;
}

bool StrictMonitor::zone_is_broken(const MqlRates& bar, const ZoneState& zone) {
    if (zone.zone_type == ZoneType::BULL) {
        return bar.close < zone.zone_low;
    } else {
        return bar.close > zone.zone_high;
    }
}

void StrictMonitor::update_touch_count(ZoneState& zone, bool touching, bool was_touching_prev) {
    if (touching && !was_touching_prev) {
        zone.touch_count++;
        zone.price_left_zone = false;
    } else if (!touching && was_touching_prev) {
        zone.price_left_zone = true;
    }
}

std::vector<TouchEvent> StrictMonitor::scan_bar(
    const MqlRates& bar,
    int bar_idx,
    std::vector<ZoneState>& zones,
    double atr_value)
{
    (void)bar_idx;
    (void)atr_value;

    std::vector<TouchEvent> events;

    for (auto& zone : zones) {
        if (!zone.is_active) continue;

        if (zone_is_broken(bar, zone)) {
            zone.is_active = false;
            zone.is_broken = true;
            continue;
        }

        bool touching = price_touches_zone(bar.close, zone.zone_high, zone.zone_low);
        bool was_touching = (zone.touch_count > 0 && !zone.price_left_zone);
        
        update_touch_count(zone, touching, was_touching);

        if (touching) {
            TouchEvent event;
            event.zone_id = zone.zone_id;
            event.zone_type = zone.zone_type;
            event.zone_high = zone.zone_high;
            event.zone_low = zone.zone_low;
            event.zone_height = zone.zone_height;
            event.close_price = bar.close;
            event.confirmation_time = zone.confirmation_time;
            event.touch_count = zone.touch_count;
            events.push_back(event);
        }
    }

    return events;
}

} // namespace rza