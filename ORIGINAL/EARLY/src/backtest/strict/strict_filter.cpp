// ============================================================================
// strict_filter.cpp — Реализация фильтра правил
// ============================================================================

#include "backtest/strict/strict_filter.h"
#include <cmath>
#include <ctime>

namespace rza {

int StrictFilter::extract_hour(int64_t unix_time) {
    time_t t = static_cast<time_t>(unix_time);
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    return tm_buf.tm_hour;
}

double StrictFilter::calculate_approach_atr(const StrictContext& ctx) {
    if (ctx.bars == nullptr || ctx.atr == nullptr) return 0.0;
    int bar_idx = ctx.current_bar_idx;
    if (bar_idx < 5) return 0.0;
    double atr_val = ctx.current_atr;
    if (atr_val <= 0.0) return 0.0;

    double price_now = (*ctx.bars)[bar_idx].close;
    double price_5ago = (*ctx.bars)[bar_idx - 5].close;
    double net_move = std::abs(price_now - price_5ago);
    return net_move / atr_val;
}

bool StrictFilter::check_age(const TouchEvent& event, const FinalRule& rule, int64_t current_time) {
    double age_minutes = static_cast<double>(current_time - event.confirmation_time) / 60.0;
    if (age_minutes < 0.0) age_minutes = 0.0;
    if (age_minutes < rule.age_min_minutes) return false;
    if (rule.age_max_minutes >= 0.0 && age_minutes >= rule.age_max_minutes) return false;
    return true;
}

bool StrictFilter::check_touch(const TouchEvent& event, const FinalRule& rule) {
    if (rule.touch_min < 0 && rule.touch_max < 0) return true;
    int touch_num = event.touch_count;
    if (rule.touch_min >= 0 && touch_num < rule.touch_min) return false;
    if (rule.touch_max >= 0 && touch_num > rule.touch_max) return false;
    return true;
}

bool StrictFilter::check_depth(const TouchEvent& event, const FinalRule& rule) {
    if (rule.depth_min_pct < 0.0 && rule.depth_max_pct < 0.0) return true;
    if (event.zone_height <= 0.0) return false;

    double depth_pct = 0.0;
    if (event.zone_type == ZoneType::BULL) {
        depth_pct = (event.zone_high - event.close_price) / event.zone_height * 100.0;
    } else {
        depth_pct = (event.close_price - event.zone_low) / event.zone_height * 100.0;
    }

    if (rule.depth_min_pct >= 0.0 && depth_pct < rule.depth_min_pct) return false;
    if (rule.depth_max_pct >= 0.0 && depth_pct > rule.depth_max_pct) return false;
    return true;
}

bool StrictFilter::check_approach(const StrictContext& ctx, const FinalRule& rule) {
    if (rule.approach_min_atr < 0.0 && rule.approach_max_atr < 0.0) return true;
    double approach = calculate_approach_atr(ctx);
    if (rule.approach_min_atr >= 0.0 && approach < rule.approach_min_atr) return false;
    if (rule.approach_max_atr >= 0.0 && approach >= rule.approach_max_atr) return false;
    return true;
}

bool StrictFilter::check_height(const TouchEvent& event, const FinalRule& rule, double atr_value) {
    if (rule.height_min_atr < 0.0 && rule.height_max_atr < 0.0) return true;
    if (atr_value <= 0.0) return false;
    double height_atr = event.zone_height / atr_value;
    if (rule.height_min_atr >= 0.0 && height_atr < rule.height_min_atr) return false;
    if (rule.height_max_atr >= 0.0 && height_atr >= rule.height_max_atr) return false;
    return true;
}

bool StrictFilter::check_session(int64_t current_time, const FinalRule& rule) {
    if (rule.session_start_hour < 0 && rule.session_end_hour < 0) return true;
    int hour = extract_hour(current_time);
    if (rule.session_start_hour >= 0 && hour < rule.session_start_hour) return false;
    if (rule.session_end_hour >= 0 && hour >= rule.session_end_hour) return false;
    return true;
}

void StrictFilter::calculate_sl_tp(
    const TouchEvent& event,
    const FinalRule& rule,
    double atr_value,
    double& sl_price,
    double& tp_price,
    double& tp_distance)
{
    tp_distance = 0.0;
    if (rule.direction == "BUY") {
        sl_price = event.zone_low;
        if (rule.target_mode == "PRIMARY_REACTION_DISTANCE") {
            tp_distance = atr_value * 1.0;
            tp_price = event.close_price + tp_distance;
        } else {
            tp_distance = atr_value * 2.0;
            tp_price = event.close_price + tp_distance;
        }
    } else {
        sl_price = event.zone_high;
        if (rule.target_mode == "PRIMARY_REACTION_DISTANCE") {
            tp_distance = atr_value * 1.0;
            tp_price = event.close_price - tp_distance;
        } else {
            tp_distance = atr_value * 2.0;
            tp_price = event.close_price - tp_distance;
        }
    }
}

RuleCheckResult StrictFilter::check_single_rule(
    const TouchEvent& event,
    const FinalRule& rule,
    const StrictContext& ctx)
{
    RuleCheckResult result;
    bool zone_type_match = false;
    if (rule.zone_type_str == "BULL" && event.zone_type == ZoneType::BULL) zone_type_match = true;
    if (rule.zone_type_str == "BEAR" && event.zone_type == ZoneType::BEAR) zone_type_match = true;
    
    if (!zone_type_match) {
        result.fail_reason = "zone_type_mismatch";
        return result;
    }

    int64_t current_time = (*ctx.bars)[ctx.current_bar_idx].time;

    result.age_ok = check_age(event, rule, current_time);
    if (!result.age_ok) { result.fail_reason = "age"; return result; }

    result.touch_ok = check_touch(event, rule);
    if (!result.touch_ok) { result.fail_reason = "touch"; return result; }

    result.depth_ok = check_depth(event, rule);
    if (!result.depth_ok) { result.fail_reason = "depth"; return result; }

    result.approach_ok = check_approach(ctx, rule);
    if (!result.approach_ok) { result.fail_reason = "approach"; return result; }

    result.height_ok = check_height(event, rule, ctx.current_atr);
    if (!result.height_ok) { result.fail_reason = "height"; return result; }

    result.session_ok = check_session(current_time, rule);
    if (!result.session_ok) { result.fail_reason = "session"; return result; }

    result.passed = true;
    return result;
}

std::vector<EntrySignal> StrictFilter::check_rules(
    const TouchEvent& event,
    const std::vector<FinalRule>& rules,
    const StrictContext& ctx)
{
    std::vector<EntrySignal> signals;
    for (const auto& rule : rules) {
        RuleCheckResult check = check_single_rule(event, rule, ctx);
        if (check.passed) {
            EntrySignal signal;
            signal.valid = true;
            signal.rule_id = rule.rule_id;
            signal.zone_id = event.zone_id;
            signal.direction = (rule.direction == "BUY") ? TradeDirection::BUY : TradeDirection::SELL;
            signal.entry_price = event.close_price;
            signal.signal_time = (*ctx.bars)[ctx.current_bar_idx].time;
            signal.signal_bar_idx = ctx.current_bar_idx;
            signal.atr_at_entry = ctx.current_atr;

            calculate_sl_tp(event, rule, ctx.current_atr,
                           signal.stop_loss_price, signal.take_profit_price, signal.tp_distance);
            signals.push_back(signal);
        }
    }
    return signals;
}

} // namespace rza