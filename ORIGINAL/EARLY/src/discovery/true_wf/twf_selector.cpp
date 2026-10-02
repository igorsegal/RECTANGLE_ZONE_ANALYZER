// ============================================================================
// twf_selector.cpp — Реализация отбора паттернов
// ============================================================================

#include "discovery/true_wf/twf_selector.h"
#include "discovery/statistics/stat_aggregator.h"
#include "utils/math_utils.h"
#include <sstream>

namespace rza {

// ============================================================================
// Проверка opt-условий
// ============================================================================
bool TWFSelector::passes_opt(
    const PatternStats& stats,
    const WalkForwardParams& params,
    std::string& reason)
{
    std::ostringstream oss;

    if (stats.samples < params.min_train_samples) {
        oss << "samples=" << stats.samples << " < " << params.min_train_samples;
        reason = oss.str();
        return false;
    }

    int decisive_local = stats.decisive_local();
    if (decisive_local < params.min_train_local_decisive) {
        oss << "decisive_local=" << decisive_local
            << " < " << params.min_train_local_decisive;
        reason = oss.str();
        return false;
    }

    int decisive_struct = stats.decisive_struct();
    if (decisive_struct < params.min_train_structural_decisive) {
        oss << "decisive_struct=" << decisive_struct
            << " < " << params.min_train_structural_decisive;
        reason = oss.str();
        return false;
    }

    double local_wilson = wilson_lower_95(stats.local_reversal, decisive_local);
    if (local_wilson < params.min_train_local_wilson) {
        oss << "local_wilson=" << local_wilson
            << " < " << params.min_train_local_wilson;
        reason = oss.str();
        return false;
    }

    double struct_wilson = wilson_lower_95(stats.struct_opposite, decisive_struct);
    if (struct_wilson < params.min_train_structural_wilson) {
        oss << "struct_wilson=" << struct_wilson
            << " < " << params.min_train_structural_wilson;
        reason = oss.str();
        return false;
    }

    reason = "OK";
    return true;
}

// ============================================================================
// Проверка val-условий
// ============================================================================
bool TWFSelector::passes_val(
    const PatternStats& opt_stats,
    const PatternStats& val_stats,
    const WalkForwardParams& params,
    std::string& reason)
{
    std::ostringstream oss;

    if (val_stats.samples < params.min_oos_samples) {
        oss << "val_samples=" << val_stats.samples
            << " < " << params.min_oos_samples;
        reason = oss.str();
        return false;
    }

    int val_decisive_local = val_stats.decisive_local();
    if (val_decisive_local < params.min_oos_local_decisive) {
        oss << "val_decisive_local=" << val_decisive_local
            << " < " << params.min_oos_local_decisive;
        reason = oss.str();
        return false;
    }

    int val_decisive_struct = val_stats.decisive_struct();
    if (val_decisive_struct < params.min_oos_structural_decisive) {
        oss << "val_decisive_struct=" << val_decisive_struct
            << " < " << params.min_oos_structural_decisive;
        reason = oss.str();
        return false;
    }

    // Val improvement
    double opt_local_pct = percent(opt_stats.local_reversal, opt_stats.decisive_local());
    double val_local_pct = percent(val_stats.local_reversal, val_stats.decisive_local());
    double local_improvement = val_local_pct - opt_local_pct;

    if (local_improvement < params.min_oos_improvement_pp) {
        oss << "local_improvement=" << local_improvement
            << " < " << params.min_oos_improvement_pp;
        reason = oss.str();
        return false;
    }

    reason = "OK";
    return true;
}

// ============================================================================
// Оценка одного паттерна в одном окне
// ============================================================================
TWFWindowPatternResult TWFSelector::evaluate_pattern_in_window(
    const PatternKey& pattern_key,
    const std::vector<TouchRecord>& opt_touches,
    const std::vector<TouchRecord>& val_touches,
    int window_index,
    const WalkForwardParams& params)
{
    TWFWindowPatternResult result;
    result.window_index = window_index;
    result.pattern_key = pattern_key;

    // OPT статистика
    auto opt_acc = StatAggregator::aggregate(opt_touches);
    const PatternStats* opt_ps = opt_acc.get_stats(pattern_key);
    if (opt_ps != nullptr) {
        result.opt_stats = *opt_ps;
    }

    // VAL статистика
    auto val_acc = StatAggregator::aggregate(val_touches);
    const PatternStats* val_ps = val_acc.get_stats(pattern_key);
    if (val_ps != nullptr) {
        result.val_stats = *val_ps;
    }

    // OPT метрики
    int opt_decisive_local = result.opt_stats.decisive_local();
    int opt_decisive_struct = result.opt_stats.decisive_struct();
    if (opt_decisive_local > 0) {
        result.opt_local_wilson = wilson_lower_95(
            result.opt_stats.local_reversal, opt_decisive_local);
        result.opt_local_pct = percent(
            result.opt_stats.local_reversal, opt_decisive_local);
    }
    if (opt_decisive_struct > 0) {
        result.opt_struct_wilson = wilson_lower_95(
            result.opt_stats.struct_opposite, opt_decisive_struct);
        result.opt_struct_pct = percent(
            result.opt_stats.struct_opposite, opt_decisive_struct);
    }

    // VAL метрики
    int val_decisive_local = result.val_stats.decisive_local();
    int val_decisive_struct = result.val_stats.decisive_struct();
    if (val_decisive_local > 0) {
        result.val_local_wilson = wilson_lower_95(
            result.val_stats.local_reversal, val_decisive_local);
        result.val_local_pct = percent(
            result.val_stats.local_reversal, val_decisive_local);
    }
    if (val_decisive_struct > 0) {
        result.val_struct_wilson = wilson_lower_95(
            result.val_stats.struct_opposite, val_decisive_struct);
        result.val_struct_pct = percent(
            result.val_stats.struct_opposite, val_decisive_struct);
    }

    // Проверка opt
    result.opt_eligible = passes_opt(result.opt_stats, params, result.opt_fail_reason);

    // Проверка val (только если opt прошёл)
    if (result.opt_eligible) {
        result.val_passed = passes_val(result.opt_stats, result.val_stats,
                                        params, result.val_fail_reason);
        result.window_pass = result.val_passed;
    } else {
        result.val_passed = false;
        result.val_fail_reason = "opt_not_eligible";
        result.window_pass = false;
    }

    return result;
}

} // namespace rza