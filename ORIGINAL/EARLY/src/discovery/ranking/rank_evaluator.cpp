// ============================================================================
// rank_evaluator.cpp — Реализация оценки паттернов
// ============================================================================

#include "discovery/ranking/rank_evaluator.h"
#include "utils/math_utils.h"

namespace rza {

// ============================================================================
// Поиск родительского паттерна
// ============================================================================
bool RankEvaluator::find_parent(
    const PatternKey& key,
    const StatAccumulator& acc,
    PatternKey& parent_key,
    PatternStats& parent_stats)
{
    // AGE_ONLY — базовый паттерн, родителя нет
    if (key.pattern_type == PatternType::AGE_ONLY) {
        return false;
    }

    // Для всех остальных типов родитель — AGE_ONLY с теми же zone + age
    parent_key.pattern_type = PatternType::AGE_ONLY;
    parent_key.zone_type = key.zone_type;
    parent_key.age_bucket = key.age_bucket;
    parent_key.touch_bucket = -1;
    parent_key.depth_bucket = -1;
    parent_key.approach_bucket = -1;
    parent_key.height_bucket = -1;
    parent_key.session_bucket = -1;

    const PatternStats* ps = acc.get_stats(parent_key);
    if (ps == nullptr) {
        return false;
    }

    parent_stats = *ps;
    return true;
}

// ============================================================================
// Wilson lower bound для локального разворота
// ============================================================================
double RankEvaluator::compute_local_wilson(const PatternStats& stats) {
    int decisive = stats.decisive_local();
    if (decisive <= 0) return -1.0;
    return wilson_lower_95(stats.local_reversal, decisive);
}

// ============================================================================
// Wilson lower bound для структурного разворота
// ============================================================================
double RankEvaluator::compute_struct_wilson(const PatternStats& stats) {
    int decisive = stats.decisive_struct();
    if (decisive <= 0) return -1.0;
    return wilson_lower_95(stats.struct_opposite, decisive);
}

// ============================================================================
// Improvement в процентных пунктах
// ============================================================================
double RankEvaluator::compute_improvement(double child_pct, double parent_pct) {
    if (child_pct < 0.0 || parent_pct < 0.0) return 0.0;
    return child_pct - parent_pct;
}

// ============================================================================
// Оценка одного паттерна
// ============================================================================
RankedPattern RankEvaluator::evaluate(
    const PatternKey& key,
    const PatternStats& stats,
    const StatAccumulator& acc)
{
    RankedPattern rp;
    rp.key = key;
    rp.stats = stats;

    // Wilson для текущего паттерна
    rp.local_wilson = compute_local_wilson(stats);
    rp.struct_wilson = compute_struct_wilson(stats);

    // Поиск родителя
    rp.has_parent = find_parent(key, acc, rp.parent_key, rp.parent_stats);

    if (rp.has_parent) {
        rp.parent_local_wilson = compute_local_wilson(rp.parent_stats);
        rp.parent_struct_wilson = compute_struct_wilson(rp.parent_stats);

        // Improvement
        double child_local_pct = percent(stats.local_reversal, stats.decisive_local());
        double parent_local_pct = percent(rp.parent_stats.local_reversal,
                                          rp.parent_stats.decisive_local());
        rp.local_improvement_pp = compute_improvement(child_local_pct, parent_local_pct);

        double child_struct_pct = percent(stats.struct_opposite, stats.decisive_struct());
        double parent_struct_pct = percent(rp.parent_stats.struct_opposite,
                                           rp.parent_stats.decisive_struct());
        rp.struct_improvement_pp = compute_improvement(child_struct_pct, parent_struct_pct);
    }

    // MFE/MAE
    if (stats.count_mfe > 0) {
        rp.avg_mfe = stats.sum_mfe / stats.count_mfe;
    }
    if (stats.count_mae > 0) {
        rp.avg_mae = stats.sum_mae / stats.count_mae;
    }
    if (rp.avg_mfe >= 0.0 && rp.avg_mae > 0.0) {
        rp.mfe_mae_ratio = rp.avg_mfe / rp.avg_mae;
    }

    return rp;
}

// ============================================================================
// Оценка всех паттернов
// ============================================================================
std::vector<RankedPattern> RankEvaluator::evaluate_all(
    const StatAccumulator& acc)
{
    std::vector<RankedPattern> result;
    result.reserve(acc.pattern_count());

    for (const auto& pair : acc.all_stats()) {
        result.push_back(evaluate(pair.first, pair.second, acc));
    }

    return result;
}

} // namespace rza