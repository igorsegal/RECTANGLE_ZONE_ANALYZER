#pragma once
// ============================================================================
// twf_summary.h — Итоговая сводка True Walk-Forward (Блок 06)
// ============================================================================
// Одна ответственность: сборка итоговой сводки по всем окнам и паттернам.
// ============================================================================

#include <vector>
#include "core/params.h"
#include "discovery/statistics/stat_accumulator.h"
#include "discovery/true_wf/twf_selector.h"
#include "discovery/true_wf/twf_window.h"

namespace rza {

// ============================================================================
// Сводка по паттерну по всем окнам
// ============================================================================
struct TWFPatternSummary {
    PatternKey   pattern_key;

    int          total_windows = 0;
    int          opt_eligible_windows = 0;
    int          val_passed_windows = 0;

    double       avg_opt_local_wilson = -1.0;
    double       avg_opt_struct_wilson = -1.0;
    double       avg_val_local_pct = -1.0;
    double       avg_val_struct_pct = -1.0;

    // Pooled VAL статистика
    PatternStats pooled_val_stats;

    bool         accepted = false;
    std::string  grade;
};

// ============================================================================
// Результат всего True Walk-Forward
// ============================================================================
struct TWFResult {
    bool                         success = false;
    std::string                  error_message;

    std::vector<TWFWindowPatternResult> window_results;
    std::vector<TWFPatternSummary>      pattern_summaries;

    int  total_windows = 0;
    int  total_patterns_evaluated = 0;
    int  accepted_patterns = 0;
};

// ============================================================================
// Класс для сборки сводки
// ============================================================================
class TWFSummary {
public:
    // Построить сводку по всем окнам
    static TWFResult build_summary(
        const std::vector<TWFWindowPatternResult>& window_results,
        const FinalRulesParams& final_params);

private:
    // Собрать сводку по одному паттерну
    static TWFPatternSummary build_pattern_summary(
        const PatternKey& key,
        const std::vector<TWFWindowPatternResult>& window_results);

    // Определить grade
    static std::string determine_grade(
        const TWFPatternSummary& summary,
        const FinalRulesParams& params,
        bool& accepted);
};

} // namespace rza