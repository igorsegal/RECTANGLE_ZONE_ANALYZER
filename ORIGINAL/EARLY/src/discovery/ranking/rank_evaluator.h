#pragma once
// ============================================================================
// rank_evaluator.h — Вычисление метрик паттернов (Блок 04)
// ============================================================================
// Одна ответственность: расчёт Wilson lower bound и improvement
// для каждого паттерна относительно родителя.
// ============================================================================

#include <vector>
#include "discovery/statistics/stat_accumulator.h"

namespace rza {

// ============================================================================
// Результат оценки одного паттерна
// ============================================================================
struct RankedPattern {
    PatternKey   key;
    PatternStats stats;

    PatternKey   parent_key;
    PatternStats parent_stats;
    bool         has_parent = false;

    // Метрики
    double   local_wilson = -1.0;
    double   struct_wilson = -1.0;
    double   parent_local_wilson = -1.0;
    double   parent_struct_wilson = -1.0;
    double   local_improvement_pp = 0.0;
    double   struct_improvement_pp = 0.0;

    // MFE/MAE ratio
    double   avg_mfe = -1.0;
    double   avg_mae = -1.0;
    double   mfe_mae_ratio = -1.0;

    // Финальный рейтинг
    double   final_score = 0.0;
    bool     passes_filter = false;
    std::string filter_reason;
};

// ============================================================================
// Класс для оценки паттернов
// ============================================================================
class RankEvaluator {
public:
    // Оценить все паттерны из накопителя
    static std::vector<RankedPattern> evaluate_all(
        const StatAccumulator& acc);

    // Оценить один паттерн
    static RankedPattern evaluate(
        const PatternKey& key,
        const PatternStats& stats,
        const StatAccumulator& acc);

private:
    // Найти родительский паттерн
    static bool find_parent(
        const PatternKey& key,
        const StatAccumulator& acc,
        PatternKey& parent_key,
        PatternStats& parent_stats);

    // Вычислить Wilson lower bound для локального разворота
    static double compute_local_wilson(const PatternStats& stats);

    // Вычислить Wilson lower bound для структурного разворота
    static double compute_struct_wilson(const PatternStats& stats);

    // Вычислить improvement в процентных пунктах
    static double compute_improvement(double child_pct, double parent_pct);
};

} // namespace rza