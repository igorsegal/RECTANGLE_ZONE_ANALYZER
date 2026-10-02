#pragma once
// ============================================================================
// opt_quality.h — Метрика качества для оптимизации
// ============================================================================
// Одна ответственность: вычисление метрики качества комбинации.
// ============================================================================

#include "pipeline/pipe_task.h"

namespace rza {

// ============================================================================
// Результат оценки качества
// ============================================================================
struct QualityMetrics {
    double   quality_score = 0.0;
    double   profit_factor = 0.0;
    double   winrate_pct = 0.0;
    double   max_drawdown_pct = 0.0;
    double   total_net_profit = 0.0;
    int      total_trades = 0;
    int      profitable_symbols = 0;
    int      total_symbols = 0;
    double   stability_score = 0.0;  // % прибыльных символов
};

// ============================================================================
// Класс для вычисления метрик
// ============================================================================
class QualityCalculator {
public:
    // Вычислить метрики для набора результатов
    static QualityMetrics calculate(
        const std::vector<TaskResult>& results);

    // Вычислить итоговый quality score
    static double calculate_quality_score(
        const QualityMetrics& metrics);
};

} // namespace rza