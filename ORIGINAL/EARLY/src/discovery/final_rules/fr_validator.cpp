// ============================================================================
// fr_validator.cpp — Реализация валидации OOS
// ============================================================================

#include "discovery/final_rules/fr_validator.h"
#include "utils/math_utils.h"
#include <sstream>

namespace rza {

// ============================================================================
// Определение grade
// ============================================================================
std::string FRValidator::determine_grade(
    const TWFPatternSummary& summary,
    const FinalRulesParams& params)
{
    (void)params;

    if (summary.val_passed_windows >= 2) {
        return "CONFIRMED_TWO_FOLDS";
    }
    if (summary.val_passed_windows == 1 && summary.opt_eligible_windows >= 2) {
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
// Проверки отдельных условий
// ИСПРАВЛЕНИЕ: pooled_oos_stats -> pooled_val_stats
// ============================================================================

bool FRValidator::check_oos_samples(const TWFPatternSummary& summary,
                                     const FinalRulesParams& params) {
    return summary.pooled_val_stats.samples >= params.min_rule_oos_samples;
}

bool FRValidator::check_local_decisive(const TWFPatternSummary& summary,
                                        const FinalRulesParams& params) {
    return summary.pooled_val_stats.decisive_local() >= params.min_local_decisive;
}

bool FRValidator::check_local_improvement(const TWFPatternSummary& summary,
                                           const FinalRulesParams& params) {
    if (summary.avg_val_local_pct < 0.0) return false;
    double improvement = summary.avg_val_local_pct - 50.0;
    return improvement >= params.min_local_improvement_pp;
}

bool FRValidator::check_local_wilson(const TWFPatternSummary& summary,
                                      const FinalRulesParams& params) {
    if (summary.avg_val_local_pct < 0.0) return false;
    int decisive = summary.pooled_val_stats.decisive_local();
    if (decisive <= 0) return false;
    double wilson = wilson_lower_95(summary.pooled_val_stats.local_reversal, decisive);
    return wilson >= params.min_local_wilson_lower95;
}

bool FRValidator::check_local_mfe_mae(const TWFPatternSummary& summary,
                                       const FinalRulesParams& params) {
    if (summary.pooled_val_stats.count_mfe <= 0 ||
        summary.pooled_val_stats.count_mae <= 0) {
        return false;
    }
    double avg_mfe = summary.pooled_val_stats.sum_mfe / summary.pooled_val_stats.count_mfe;
    double avg_mae = summary.pooled_val_stats.sum_mae / summary.pooled_val_stats.count_mae;
    if (avg_mae <= 0.0) return false;
    return (avg_mfe / avg_mae) >= params.min_local_mfe_mae_ratio;
}

bool FRValidator::check_structural_decisive(const TWFPatternSummary& summary,
                                             const FinalRulesParams& params) {
    return summary.pooled_val_stats.decisive_struct() >= params.min_structural_decisive;
}

bool FRValidator::check_structural_improvement(const TWFPatternSummary& summary,
                                                const FinalRulesParams& params) {
    if (summary.avg_val_struct_pct < 0.0) return false;
    double improvement = summary.avg_val_struct_pct - 50.0;
    return improvement >= params.min_structural_improvement_pp;
}

bool FRValidator::check_structural_wilson(const TWFPatternSummary& summary,
                                           const FinalRulesParams& params) {
    if (summary.avg_val_struct_pct < 0.0) return false;
    int decisive = summary.pooled_val_stats.decisive_struct();
    if (decisive <= 0) return false;
    double wilson = wilson_lower_95(summary.pooled_val_stats.struct_opposite, decisive);
    return wilson >= params.min_structural_wilson_lower95;
}

// ============================================================================
// Валидация одного паттерна
// ============================================================================
FinalRuleValidation FRValidator::validate(
    const TWFPatternSummary& summary,
    const FinalRulesParams& params)
{
    FinalRuleValidation result;
    result.pattern_key = summary.pattern_key;
    result.twf_summary = summary;

    // Grade
    result.grade = determine_grade(summary, params);
    result.passes_grade = (result.grade == params.required_oos_grade);

    // Отдельные проверки
    result.passes_oos_samples = check_oos_samples(summary, params);
    result.passes_local_decisive = check_local_decisive(summary, params);
    result.passes_local_improvement = check_local_improvement(summary, params);
    result.passes_local_wilson = check_local_wilson(summary, params);
    result.passes_local_mfe_mae = check_local_mfe_mae(summary, params);
    result.passes_structural_decisive = check_structural_decisive(summary, params);
    result.passes_structural_improvement = check_structural_improvement(summary, params);
    result.passes_structural_wilson = check_structural_wilson(summary, params);

    // Метрики
    // ИСПРАВЛЕНИЕ: pooled_oos_stats -> pooled_val_stats
    int decisive_local = summary.pooled_val_stats.decisive_local();
    int decisive_struct = summary.pooled_val_stats.decisive_struct();
    if (decisive_local > 0) {
        result.pooled_oos_local_pct = percent(
            summary.pooled_val_stats.local_reversal, decisive_local);
    }
    if (decisive_struct > 0) {
        result.pooled_oos_struct_pct = percent(
            summary.pooled_val_stats.struct_opposite, decisive_struct);
    }
    if (summary.pooled_val_stats.count_mfe > 0 &&
        summary.pooled_val_stats.count_mae > 0) {
        double avg_mfe = summary.pooled_val_stats.sum_mfe /
                         summary.pooled_val_stats.count_mfe;
        double avg_mae = summary.pooled_val_stats.sum_mae /
                         summary.pooled_val_stats.count_mae;
        if (avg_mae > 0.0) {
            result.pooled_oos_mfe_mae_ratio = avg_mfe / avg_mae;
        }
    }

    // Полная валидация
    result.fully_valid =
        result.passes_grade &&
        result.passes_oos_samples &&
        result.passes_local_decisive &&
        result.passes_local_improvement &&
        result.passes_local_wilson &&
        result.passes_local_mfe_mae &&
        result.passes_structural_decisive &&
        result.passes_structural_improvement &&
        result.passes_structural_wilson;

    if (!result.fully_valid) {
        std::ostringstream oss;
        if (!result.passes_grade) oss << "grade=" << result.grade << " ";
        if (!result.passes_oos_samples) oss << "oos_samples ";
        if (!result.passes_local_decisive) oss << "local_decisive ";
        if (!result.passes_local_improvement) oss << "local_improvement ";
        if (!result.passes_local_wilson) oss << "local_wilson ";
        if (!result.passes_local_mfe_mae) oss << "local_mfe_mae ";
        if (!result.passes_structural_decisive) oss << "struct_decisive ";
        if (!result.passes_structural_improvement) oss << "struct_improvement ";
        if (!result.passes_structural_wilson) oss << "struct_wilson ";
        result.fail_reason = oss.str();
    } else {
        result.fail_reason = "OK";
    }

    return result;
}

// ============================================================================
// Валидация всех паттернов
// ============================================================================
std::vector<FinalRuleValidation> FRValidator::validate_all(
    const TWFResult& twf_result,
    const FinalRulesParams& params)
{
    std::vector<FinalRuleValidation> result;
    result.reserve(twf_result.pattern_summaries.size());

    for (const auto& summary : twf_result.pattern_summaries) {
        result.push_back(validate(summary, params));
    }

    return result;
}

} // namespace rza