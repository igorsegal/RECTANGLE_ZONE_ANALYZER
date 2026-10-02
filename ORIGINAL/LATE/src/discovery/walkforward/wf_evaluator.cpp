// ============================================================================
// wf_evaluator.cpp — Реализация Walk-Forward оценки
// ============================================================================

#include "discovery/walkforward/wf_evaluator.h"
#include "discovery/statistics/stat_aggregator.h"
#include "utils/math_utils.h"
#include <unordered_set>
#include <unordered_map>
#include <sstream>

namespace rza {

// ============================================================================
// Проверка train-условий
// ============================================================================
bool WFEvaluator::passes_train(
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
// Проверка OOS-условий
// ============================================================================
bool WFEvaluator::passes_oos(
    const PatternStats& train_stats,
    const PatternStats& oos_stats,
    const WalkForwardParams& params,
    std::string& reason)
{
    std::ostringstream oss;

    if (oos_stats.samples < params.min_oos_samples) {
        oss << "oos_samples=" << oos_stats.samples
            << " < " << params.min_oos_samples;
        reason = oss.str();
        return false;
    }

    int oos_decisive_local = oos_stats.decisive_local();
    if (oos_decisive_local < params.min_oos_local_decisive) {
        oss << "oos_decisive_local=" << oos_decisive_local
            << " < " << params.min_oos_local_decisive;
        reason = oss.str();
        return false;
    }

    int oos_decisive_struct = oos_stats.decisive_struct();
    if (oos_decisive_struct < params.min_oos_structural_decisive) {
        oss << "oos_decisive_struct=" << oos_decisive_struct
            << " < " << params.min_oos_structural_decisive;
        reason = oss.str();
        return false;
    }

    // OOS improvement: процент в OOS должен быть не меньше, чем в train
    double train_local_pct = percent(train_stats.local_reversal,
                                     train_stats.decisive_local());
    double oos_local_pct = percent(oos_stats.local_reversal,
                                   oos_stats.decisive_local());
    double local_improvement = oos_local_pct - train_local_pct;

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
// Оценка одного паттерна в одном fold
// ============================================================================
WFFoldResult WFEvaluator::evaluate_fold(
    const PatternKey& pattern_key,
    const WFSegment& train_segment,
    const WFSegment& oos_segment,
    const WalkForwardParams& params)
{
    WFFoldResult result;
    result.fold_index = train_segment.index;
    result.pattern_key = pattern_key;

    // Собираем train-статистику
    std::vector<TouchRecord> train_touches;
    train_touches.reserve(train_segment.touches.size());
    for (const TouchRecord* t : train_segment.touches) {
        train_touches.push_back(*t);
    }
    auto full_train_acc = StatAggregator::aggregate(train_touches);
    const PatternStats* train_ps = full_train_acc.get_stats(pattern_key);
    if (train_ps != nullptr) {
        result.train_stats = *train_ps;
    }

    // Собираем OOS-статистику
    std::vector<TouchRecord> oos_touches;
    oos_touches.reserve(oos_segment.touches.size());
    for (const TouchRecord* t : oos_segment.touches) {
        oos_touches.push_back(*t);
    }
    auto full_oos_acc = StatAggregator::aggregate(oos_touches);
    const PatternStats* oos_ps = full_oos_acc.get_stats(pattern_key);
    if (oos_ps != nullptr) {
        result.oos_stats = *oos_ps;
    }

    // Train метрики
    int train_decisive_local = result.train_stats.decisive_local();
    int train_decisive_struct = result.train_stats.decisive_struct();
    if (train_decisive_local > 0) {
        result.train_local_wilson = wilson_lower_95(
            result.train_stats.local_reversal, train_decisive_local);
        result.train_local_pct = percent(
            result.train_stats.local_reversal, train_decisive_local);
    }
    if (train_decisive_struct > 0) {
        result.train_struct_wilson = wilson_lower_95(
            result.train_stats.struct_opposite, train_decisive_struct);
        result.train_struct_pct = percent(
            result.train_stats.struct_opposite, train_decisive_struct);
    }

    // OOS метрики
    int oos_decisive_local = result.oos_stats.decisive_local();
    int oos_decisive_struct = result.oos_stats.decisive_struct();
    if (oos_decisive_local > 0) {
        result.oos_local_wilson = wilson_lower_95(
            result.oos_stats.local_reversal, oos_decisive_local);
        result.oos_local_pct = percent(
            result.oos_stats.local_reversal, oos_decisive_local);
    }
    if (oos_decisive_struct > 0) {
        result.oos_struct_wilson = wilson_lower_95(
            result.oos_stats.struct_opposite, oos_decisive_struct);
        result.oos_struct_pct = percent(
            result.oos_stats.struct_opposite, oos_decisive_struct);
    }

    // Проверка train
    result.train_eligible = passes_train(result.train_stats, params,
                                          result.train_fail_reason);

    // Проверка OOS (только если train прошёл)
    if (result.train_eligible) {
        result.oos_passed = passes_oos(result.train_stats, result.oos_stats,
                                        params, result.oos_fail_reason);
        result.fold_pass = result.oos_passed;
    } else {
        result.oos_passed = false;
        result.oos_fail_reason = "train_not_eligible";
        result.fold_pass = false;
    }

    return result;
}

// ============================================================================
// Сборка сводки по паттерну
// ============================================================================
WFPatternSummary WFEvaluator::build_summary(
    const PatternKey& key,
    const std::vector<WFFoldResult>& fold_results)
{
    WFPatternSummary summary;
    summary.pattern_key = key;
    summary.total_folds = 0;

    double sum_train_local_wilson = 0.0;
    int count_train_local_wilson = 0;
    double sum_train_struct_wilson = 0.0;
    int count_train_struct_wilson = 0;
    double sum_oos_local_pct = 0.0;
    int count_oos_local_pct = 0;
    double sum_oos_struct_pct = 0.0;
    int count_oos_struct_pct = 0;

    for (const auto& fr : fold_results) {
        if (!(fr.pattern_key == key)) continue;

        summary.total_folds++;

        if (fr.train_eligible) {
            summary.train_eligible_folds++;
            if (fr.train_local_wilson >= 0.0) {
                sum_train_local_wilson += fr.train_local_wilson;
                count_train_local_wilson++;
            }
            if (fr.train_struct_wilson >= 0.0) {
                sum_train_struct_wilson += fr.train_struct_wilson;
                count_train_struct_wilson++;
            }
        }

        if (fr.oos_passed) {
            summary.oos_passed_folds++;
            if (fr.oos_local_pct >= 0.0) {
                sum_oos_local_pct += fr.oos_local_pct;
                count_oos_local_pct++;
            }
            if (fr.oos_struct_pct >= 0.0) {
                sum_oos_struct_pct += fr.oos_struct_pct;
                count_oos_struct_pct++;
            }
        }

        // Pooled OOS статистика
        summary.pooled_oos_stats.samples += fr.oos_stats.samples;
        summary.pooled_oos_stats.local_reversal += fr.oos_stats.local_reversal;
        summary.pooled_oos_stats.local_breakout += fr.oos_stats.local_breakout;
        summary.pooled_oos_stats.struct_opposite += fr.oos_stats.struct_opposite;
        summary.pooled_oos_stats.struct_same_type += fr.oos_stats.struct_same_type;
        summary.pooled_oos_stats.struct_targets_broken += fr.oos_stats.struct_targets_broken;
        summary.pooled_oos_stats.struct_timeout += fr.oos_stats.struct_timeout;
        summary.pooled_oos_stats.local_timeout += fr.oos_stats.local_timeout;
        summary.pooled_oos_stats.sum_mfe += fr.oos_stats.sum_mfe;
        summary.pooled_oos_stats.count_mfe += fr.oos_stats.count_mfe;
        summary.pooled_oos_stats.sum_mae += fr.oos_stats.sum_mae;
        summary.pooled_oos_stats.count_mae += fr.oos_stats.count_mae;
    }

    if (count_train_local_wilson > 0) {
        summary.avg_train_local_wilson = sum_train_local_wilson / count_train_local_wilson;
    }
    if (count_train_struct_wilson > 0) {
        summary.avg_train_struct_wilson = sum_train_struct_wilson / count_train_struct_wilson;
    }
    if (count_oos_local_pct > 0) {
        summary.avg_oos_local_pct = sum_oos_local_pct / count_oos_local_pct;
    }
    if (count_oos_struct_pct > 0) {
        summary.avg_oos_struct_pct = sum_oos_struct_pct / count_oos_struct_pct;
    }

    return summary;
}

// ============================================================================
// Главный метод: Walk-Forward
// ============================================================================
WFResult WFEvaluator::evaluate(
    const WFSegmentation& segmentation,
    const WalkForwardParams& params)
{
    WFResult result;
    result.success = false;

    if (!segmentation.success) {
        result.error_message = "Segmentation failed: " + segmentation.error_message;
        return result;
    }

    if (segmentation.oos_segments <= 0) {
        result.error_message = "No OOS segments";
        return result;
    }

    // Собираем все уникальные ключи паттернов из всех сегментов
    std::unordered_set<uint64_t> all_keys;
    std::unordered_map<uint64_t, PatternKey> key_map;

    for (const auto& seg : segmentation.segments) {
        std::vector<TouchRecord> seg_touches;
        seg_touches.reserve(seg.touches.size());
        for (const TouchRecord* t : seg.touches) {
            seg_touches.push_back(*t);
        }
        auto acc = StatAggregator::aggregate(seg_touches);
        for (const auto& pair : acc.all_stats()) {
            uint64_t k = pair.first.to_uint64();
            if (all_keys.find(k) == all_keys.end()) {
                all_keys.insert(k);
                key_map[k] = pair.first;
            }
        }
    }

    result.total_patterns_evaluated = static_cast<int>(all_keys.size());

    // Rolling WF: fold i = train[i..i+training), oos[i+training]
    int num_folds = segmentation.total_segments - segmentation.training_segments;
    result.total_folds = num_folds;

    std::vector<WFFoldResult> all_fold_results;

    for (int fold_idx = 0; fold_idx < num_folds; ++fold_idx) {
        // Train: сегменты [fold_idx, fold_idx + training_segments)
        WFSegment train_combined;
        train_combined.index = fold_idx;
        train_combined.start_time = segmentation.segments[fold_idx].start_time;
        train_combined.end_time = segmentation.segments[fold_idx + segmentation.training_segments - 1].end_time;

        for (int i = fold_idx; i < fold_idx + segmentation.training_segments; ++i) {
            for (const TouchRecord* t : segmentation.segments[i].touches) {
                train_combined.touches.push_back(t);
            }
        }

        // OOS: сегмент [fold_idx + training_segments]
        const WFSegment& oos_segment = segmentation.segments[fold_idx + segmentation.training_segments];

        // Для каждого паттерна оцениваем этот fold
        for (const auto& key : all_keys) {
            const PatternKey& pk = key_map[key];
            WFFoldResult fr = evaluate_fold(pk, train_combined, oos_segment, params);
            fr.fold_index = fold_idx;
            all_fold_results.push_back(fr);
        }
    }

    result.fold_results = all_fold_results;

    // Собираем сводки по паттернам
    for (const auto& key : all_keys) {
        const PatternKey& pk = key_map[key];
        WFPatternSummary summary = build_summary(pk, all_fold_results);

        // Определяем grade
        if (summary.oos_passed_folds >= 2) {
            summary.grade = "CONFIRMED_TWO_FOLDS";
            summary.accepted = true;
        } else if (summary.oos_passed_folds == 1 && summary.train_eligible_folds >= 2) {
            summary.grade = "SINGLE_FOLD_PASS";
            summary.accepted = true;
        } else if (summary.train_eligible_folds > 0 && summary.oos_passed_folds == 0) {
            summary.grade = "OOS_FAILED";
            summary.accepted = false;
        } else if (summary.train_eligible_folds == 0) {
            summary.grade = "INSUFFICIENT_TRAIN";
            summary.accepted = false;
        } else {
            summary.grade = "REPEATED_MIXED";
            summary.accepted = false;
        }

        result.pattern_summaries.push_back(summary);
        if (summary.accepted) {
            result.accepted_patterns++;
        }
    }

    result.success = true;
    return result;
}

} // namespace rza