#pragma once
// ============================================================================
// opt_result.h — Хранение результатов оптимизации
// ============================================================================
// Одна ответственность: хранение и сортировка результатов оптимизации.
// ============================================================================

#include <vector>
#include "optimizer/opt_runner.h"

namespace rza {

// ============================================================================
// Итоговый результат оптимизации
// ============================================================================
struct OptimizationResult {
    bool                                    success = false;
    std::string                             error_message;

    std::vector<OptimizationRunResult>      all_runs;

    int      total_combinations = 0;
    int      successful_runs = 0;
    int      failed_runs = 0;

    double   total_processing_time_ms = 0.0;

    // Топ-N результатов
    std::vector<OptimizationRunResult>      top_results;
};

// ============================================================================
// Класс для управления результатами
// ============================================================================
class OptimizationResultManager {
public:
    // Создать итоговый результат
    static OptimizationResult create_result(
        const std::vector<OptimizationRunResult>& runs,
        int top_n = 10);

    // Получить топ-N результатов
    static std::vector<OptimizationRunResult> get_top_n(
        const std::vector<OptimizationRunResult>& runs,
        int top_n);
};

} // namespace rza