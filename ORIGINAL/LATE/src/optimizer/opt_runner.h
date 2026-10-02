#pragma once
// ============================================================================
// opt_runner.h — Запуск одной комбинации параметров
// ============================================================================
// Одна ответственность: выполнение Pipeline для одной комбинации.
// ============================================================================

#include <vector>
#include "optimizer/opt_grid.h"
#include "optimizer/opt_quality.h"
#include "pipeline/pipe_task.h"
#include "core/params.h"

namespace rza {

// ============================================================================
// Результат запуска одной комбинации
// ============================================================================
struct OptimizationRunResult {
    int              combo_id = -1;
    ParameterCombination params;

    bool             success = false;
    std::string      error_message;

    QualityMetrics   quality;
    std::vector<TaskResult> task_results;

    double           processing_time_ms = 0.0;
};

// ============================================================================
// Класс для запуска оптимизации
// ============================================================================
class OptimizationRunner {
public:
    // Запустить одну комбинацию на наборе файлов
    static OptimizationRunResult run_combination(
        const ParameterCombination& combo,
        const std::vector<std::string>& file_paths,
        const SystemParams& base_params);

private:
    // Применить комбинацию к параметрам системы
    static SystemParams apply_combination(
        const SystemParams& base_params,
        const ParameterCombination& combo);
};

} // namespace rza