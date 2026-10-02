#pragma once
// ============================================================================
// strict_types.h — Типы данных для строгой логики исполнения сделок
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include <utility>

#include "core/types.h"
#include "core/params.h"
#include "discovery/final_rules/fr_decoder.h"
#include "backtest/bt_types.h"

namespace rza {

struct TouchEvent {
    int      zone_id = -1;
    ZoneType zone_type = ZoneType::BULL;
    double   zone_high = 0.0;
    double   zone_low = 0.0;
    double   zone_height = 0.0;
    double   close_price = 0.0;
    int64_t  confirmation_time = 0;
    int      touch_count = 0;
};

struct ZoneState {
    int          zone_id = -1;
    ZoneType     zone_type = ZoneType::BULL;
    double       zone_high = 0.0;
    double       zone_low = 0.0;
    double       zone_height = 0.0;
    int64_t      confirmation_time = 0;
    bool         is_active = true;
    bool         is_broken = false;
    int          touch_count = 0;
    bool         price_left_zone = false;
    std::vector<std::pair<std::string, int>> rule_usage;

    int get_usage_count(const std::string& rule_id) const {
        for (const auto& pair : rule_usage) {
            if (pair.first == rule_id) return pair.second;
        }
        return 0;
    }

    void increment_usage(const std::string& rule_id) {
        for (auto& pair : rule_usage) {
            if (pair.first == rule_id) {
                pair.second++;
                return;
            }
        }
        rule_usage.push_back({rule_id, 1});
    }
};

struct EntrySignal {
    bool           valid = false;
    std::string    rule_id;
    int            zone_id = -1;
    TradeDirection direction = TradeDirection::BUY;
    double         entry_price = 0.0;
    double         stop_loss_price = 0.0;
    double         take_profit_price = 0.0;
    int64_t        signal_time = 0;
    int32_t        signal_bar_idx = -1;
    double         atr_at_entry = 0.0;
    double         tp_distance = 0.0;
};

struct RuleCheckResult {
    bool        passed = false;
    std::string fail_reason;
    bool        age_ok = false;
    bool        touch_ok = false;
    bool        depth_ok = false;
    bool        approach_ok = false;
    bool        height_ok = false;
    bool        session_ok = false;
};

struct StrictContext {
    const std::vector<MqlRates>*  bars = nullptr;
    const std::vector<double>*    atr = nullptr;
    std::vector<ZoneState>*       zones = nullptr;
    const std::vector<FinalRule>* rules = nullptr;
    const BacktestParams*         bt_params = nullptr;

    int    current_bar_idx = 0;
    int    total_bars = 0;
    double current_atr = 0.0;
    double point = 0.0;
    int    period_seconds = 0;

    int asia_end_hour = 7;
    int london_end_hour = 13;
    int newyork_end_hour = 21;
};

} // namespace rza