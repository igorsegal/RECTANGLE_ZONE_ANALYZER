#pragma once
// ============================================================================
// wf_evaluator.h — Оценка паттернов в Train/OOS окнах (Блок 05)
// ============================================================================
// Одна ответственность: для каждого паттерна проверить, проходит ли он
// обучение в train-окне и валидацию в OOS-окне.
// ============================================================================

#include <vector>
#include "core/params.h"
#include "discovery/statistics/stat_accumulator.h"
#include "discovery/walkforward/wf_segmenter.h"

namespace rza {

// ============================================================================
// Результат для одного паттерна в одном fold
// ============================================================================
struct WFFoldResult {
    int          fold_index = -1;
    PatternKey   pattern_key;

    // Train статистика
    PatternStats train_stats;
    double       train_local_wilson = -1.0;
    double       train_struct_wilson = -1.0;
    double       train_local_pct = -1.0;
    double       train_struct_pct = -1.0;
    bool         train_eligible = false;
    std::string  train_fail_reason;

    // OOS статистика
    PatternStats oos_stats;
    double       oos_local_wilson = -1.0;
    double       oos_struct_wilson = -1.0;
    double       oos_local_pct = -1.0;
    double       oos_struct_pct = -1.0;
    bool         oos_passed = false;
    std::string  oos_fail_reason;

    // Итог
    bool         fold_pass = false;
};

// ============================================================================
// Сводка по паттерну по всем fold
// ============================================================================
struct WFPatternSummary {
    PatternKey   pattern_key;

    int          total_folds = 0;
    int          train_eligible_folds = 0;
    int          oos_passed_folds = 0;

    double       avg_train_local_wilson = -1.0;
    double       avg_train_struct_wilson = -1.0;
    double       avg_oos_local_pct = -1.0;
    double       avg_oos_struct_pct = -1.0;

    // Pooled OOS статистика
    PatternStats pooled_oos_stats;

    bool         accepted = false;
    std::string  grade;
};

// ============================================================================
// Результат всего Walk-Forward
// ============================================================================
struct WFResult {
    bool                         success = false;
    std::string                  error_message;

    std::vector<WFFoldResult>    fold_results;
    std::vector<WFPatternSummary> pattern_summaries;

    int  total_folds = 0;
    int  total_patterns_evaluated = 0;
    int  accepted_patterns = 0;
};

// ============================================================================
// Класс для Walk-Forward оценки
// ============================================================================
class WFEvaluator {
public:
    // Полный Walk-Forward процесс
    static WFResult evaluate(
        const WFSegmentation& segmentation,
        const WalkForwardParams& params);

private:
    // Оценить один паттерн в одном fold
    static WFFoldResult evaluate_fold(
        const PatternKey& pattern_key,
        const WFSegment& train_segment,
        const WFSegment& oos_segment,
        const WalkForwardParams& params);

    // Проверить, проходит ли паттерн train-условия
    static bool passes_train(
        const PatternStats& stats,
        const WalkForwardParams& params,
        std::string& reason);

    // Проверить, проходит ли паттерн OOS-условия
    static bool passes_oos(
        const PatternStats& train_stats,
        const PatternStats& oos_stats,
        const WalkForwardParams& params,
        std::string& reason);

    // Собрать сводку по всем fold для одного паттерна
    static WFPatternSummary build_summary(
        const PatternKey& key,
        const std::vector<WFFoldResult>& fold_results);
};

} // namespace rza