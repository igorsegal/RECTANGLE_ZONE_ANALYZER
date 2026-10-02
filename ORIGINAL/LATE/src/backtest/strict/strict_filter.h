#pragma once
// ============================================================================
// strict_filter.h — Блок 2: Фильтр правил
// ============================================================================

#include <vector>
#include "backtest/strict/strict_types.h"

namespace rza {

class StrictFilter {
public:
    static std::vector<EntrySignal> check_rules(
        const TouchEvent& event,
        const std::vector<FinalRule>& rules,
        const StrictContext& ctx);

private:
    static RuleCheckResult check_single_rule(
        const TouchEvent& event,
        const FinalRule& rule,
        const StrictContext& ctx);

    static bool check_age(const TouchEvent& event, const FinalRule& rule, int64_t current_time);
    static bool check_touch(const TouchEvent& event, const FinalRule& rule);
    static bool check_depth(const TouchEvent& event, const FinalRule& rule);
    static bool check_approach(const StrictContext& ctx, const FinalRule& rule);
    static bool check_height(const TouchEvent& event, const FinalRule& rule, double atr_value);
    static bool check_session(int64_t current_time, const FinalRule& rule);
    
    static void calculate_sl_tp(
        const TouchEvent& event,
        const FinalRule& rule,
        double atr_value,
        double& sl_price,
        double& tp_price,
        double& tp_distance);

    static double calculate_approach_atr(const StrictContext& ctx);
    static int extract_hour(int64_t unix_time);
};

} // namespace rza