// ============================================================================
// opt_runner.cpp — Реализация запуска оптимизации
// ============================================================================

#include "optimizer/opt_runner.h"
#include "pipeline/pipe_orchestrator.h"
#include "utils/timer.h"
#include <iostream>

namespace rza {

SystemParams OptimizationRunner::apply_combination(
    const SystemParams& base_params,
    const ParameterCombination& combo)
{
    SystemParams params = base_params;

    // Discovery параметры
    params.zone.min_gap_atr = combo.min_gap_atr;
    params.zone.breakout_atr = combo.breakout_atr;
    params.touch.reaction_atr = combo.reaction_atr;
    params.touch.local_breakout_atr = combo.local_breakout_atr;
    params.touch.max_local_bars = combo.max_local_bars;
    params.touch.max_structural_bars = combo.max_structural_bars;

    // Backtest параметры
    params.backtest.risk_percent = combo.risk_percent;
    params.backtest.max_concurrent_positions = combo.max_concurrent_positions;

    // НОВОЕ: параметры окна валидации
    params.validation.opt_weeks = combo.opt_weeks;
    params.validation.oos_weeks = combo.oos_weeks;
    params.validation.step_weeks = combo.step_weeks;

    return params;
}

OptimizationRunResult OptimizationRunner::run_combination(
    const ParameterCombination& combo,
    const std::vector<std::string>& file_paths,
    const SystemParams& base_params)
{
    OptimizationRunResult result;
    result.combo_id = combo.combo_id;
    result.params = combo;

    Timer timer;

    // Применяем комбинацию к параметрам
    SystemParams params = apply_combination(base_params, combo);

    // Создаём задачи
    auto tasks = PipelineOrchestrator::create_tasks(file_paths);

    // Запускаем Pipeline
    auto pipeline_result = PipelineOrchestrator::run_all(tasks, params, 1);

    if (!pipeline_result.success) {
        result.success = false;
        result.error_message = pipeline_result.error_message;
        result.processing_time_ms = timer.elapsed_ms();
        return result;
    }

    // Собираем результаты
    for (const auto& task : tasks) {
        if (task.status == TaskStatus::COMPLETED) {
            result.task_results.push_back(task.result);
        }
    }

    // Вычисляем метрики качества
    result.quality = QualityCalculator::calculate(result.task_results);
    result.success = true;
    result.processing_time_ms = timer.elapsed_ms();

    return result;
}

} // namespace rza