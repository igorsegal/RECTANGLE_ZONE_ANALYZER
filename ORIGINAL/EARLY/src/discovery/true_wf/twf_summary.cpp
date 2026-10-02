// ============================================================================
// twf_summary.cpp — Реализация итоговой сводки
// ============================================================================

#include "discovery/true_wf/twf_summary.h"
#include <unordered_set>
#include <unordered_map>

namespace rza {

// ============================================================================
// Сборка сводки по одному паттерну
// ============================================================================
TWFPatternSummary TWFSummary::build_pattern_summary(
    const PatternKey& key,
    const std::vector<TWFWindowPatternResult>& window_results)
{
    TWFPatternSummary summary;
    summary.pattern_key = key;
    summary.total_windows = 0;

    double sum_opt_local_wilson = 0.0;
    int count_opt_local_wilson = 0;
    double sum_opt_struct_wilson = 0.0;
    int count_opt_struct_wilson = 0;
    double sum_val_local_pct = 0.0;
    int count_val_local_pct = 0;
    double sum_val_struct_pct = 0.0;
    int count_val_struct_pct = 0;

    for (const auto& wr : window_results) {
        if (!(wr.pattern_key == key)) continue;

        summary.total_windows++;

        if (wr.opt_eligible) {
            summary.opt_eligible_windows++;
            if (wr.opt_local_wilson >= 0.0) {
                sum_opt_local_wilson += wr.opt_local_wilson;
                count_opt_local_wilson++;
            }
            if (wr.opt_struct_wilson >= 0.0) {
                sum_opt_struct_wilson += wr.opt_struct_wilson;
                count_opt_struct_wilson++;
            }
        }

        if (wr.val_passed) {
            summary.val_passed_windows++;
            if (wr.val_local_pct >= 0.0) {
                sum_val_local_pct += wr.val_local_pct;
                count_val_local_pct++;
            }
            if (wr.val_struct_pct >= 0.0) {
                sum_val_struct_pct += wr.val_struct_pct;
                count_val_struct_pct++;
            }
        }

        // Pooled VAL статистика
        summary.pooled_val_stats.samples += wr.val_stats.samples;
        summary.pooled_val_stats.local_reversal += wr.val_stats.local_reversal;
        summary.pooled_val_stats.local_breakout += wr.val_stats.local_breakout;
        summary.pooled_val_stats.struct_opposite += wr.val_stats.struct_opposite;
        summary.pooled_val_stats.struct_same_type += wr.val_stats.struct_same_type;
        summary.pooled_val_stats.struct_targets_broken += wr.val_stats.struct_targets_broken;
        summary.pooled_val_stats.struct_timeout += wr.val_stats.struct_timeout;
        summary.pooled_val_stats.local_timeout += wr.val_stats.local_timeout;
        summary.pooled_val_stats.sum_mfe += wr.val_stats.sum_mfe;
        summary.pooled_val_stats.count_mfe += wr.val_stats.count_mfe;
        summary.pooled_val_stats.sum_mae += wr.val_stats.sum_mae;
        summary.pooled_val_stats.count_mae += wr.val_stats.count_mae;
    }

    if (count_opt_local_wilson > 0) {
        summary.avg_opt_local_wilson = sum_opt_local_wilson / count_opt_local_wilson;
    }
    if (count_opt_struct_wilson > 0) {
        summary.avg_opt_struct_wilson = sum_opt_struct_wilson / count_opt_struct_wilson;
    }
    if (count_val_local_pct > 0) {
        summary.avg_val_local_pct = sum_val_local_pct / count_val_local_pct;
    }
    if (count_val_struct_pct > 0) {
        summary.avg_val_struct_pct = sum_val_struct_pct / count_val_struct_pct;
    }

    return summary;
}

// ============================================================================
// Определение grade
// ============================================================================
std::string TWFSummary::determine_grade(
    const TWFPatternSummary& summary,
    const FinalRulesParams& params,
    bool& accepted)
{
    accepted = false;

    // Проверка соответствия требуемому grade
    if (summary.val_passed_windows >= 2) {
        accepted = true;
        return "CONFIRMED_TWO_FOLDS";
    }

    if (summary.val_passed_windows == 1 && summary.opt_eligible_windows >= 2) {
        accepted = true;
        return "SINGLE_FOLD_PASS";
    }

    if (summary.opt_eligible_windows > 0 && summary.val_passed_windows == 0) {
        return "OOS_FAILED";
    }

    if (summary.opt_eligible_windows == 0) {
        return "INSUFFICIENT_OPT";
    }

    return "REPEATED_MIXED";
}

// ============================================================================
// Главная функция сборки сводки
// ============================================================================
TWFResult TWFSummary::build_summary(
    const std::vector<TWFWindowPatternResult>& window_results,
    const FinalRulesParams& final_params)
{
    TWFResult result;
    result.success = false;

    // Собираем уникальные ключи паттернов
    std::unordered_set<uint64_t> all_keys;
    std::unordered_map<uint64_t, PatternKey> key_map;

    for (const auto& wr : window_results) {
        uint64_t k = wr.pattern_key.to_uint64();
        if (all_keys.find(k) == all_keys.end()) {
            all_keys.insert(k);
            key_map[k] = wr.pattern_key;
        }
    }

    result.total_patterns_evaluated = static_cast<int>(all_keys.size());

    // Собираем сводки по паттернам
    for (const auto& key : all_keys) {
        const PatternKey& pk = key_map[key];
        TWFPatternSummary summary = build_pattern_summary(pk, window_results);

        summary.grade = determine_grade(summary, final_params, summary.accepted);

        result.pattern_summaries.push_back(summary);
        if (summary.accepted) {
            result.accepted_patterns++;
        }
    }

    // Определяем total_windows из первого результата
    if (!window_results.empty()) {
        int max_window_idx = 0;
        for (const auto& wr : window_results) {
            if (wr.window_index > max_window_idx) {
                max_window_idx = wr.window_index;
            }
        }
        result.total_windows = max_window_idx + 1;
    }

    result.success = true;
    return result;
}

} // namespace rza