#pragma once
// ============================================================================
// pipe_orchestrator.h — Диспетчер задач
// ============================================================================
// Одна ответственность: управление очередью задач и их выполнение.
// ============================================================================

#include <vector>
#include <string>
#include "pipeline/pipe_task.h"
#include "core/params.h"

namespace rza {

// ============================================================================
// Результат работы оркестратора
// ============================================================================
struct PipelineResult {
    bool                        success = false;
    std::string                 error_message;

    std::vector<PipelineTask>   tasks;

    int32_t  total_tasks = 0;
    int32_t  completed_tasks = 0;
    int32_t  failed_tasks = 0;

    double   total_processing_time_ms = 0.0;

    // Суммарная статистика
    int32_t  total_bars_processed = 0;
    int32_t  total_zones_found = 0;
    int32_t  total_touches_found = 0;
    int32_t  total_rules_accepted = 0;
    int32_t  total_trades = 0;
    double   total_net_profit = 0.0;
    int32_t  profitable_symbols = 0;
};

// ============================================================================
// Класс оркестратора
// ============================================================================
class PipelineOrchestrator {
public:
    // Создать задачи из списка файлов
    static std::vector<PipelineTask> create_tasks(
        const std::vector<std::string>& file_paths);

    // Выполнить все задачи
    static PipelineResult run_all(
        std::vector<PipelineTask>& tasks,
        const SystemParams& params,
        int num_threads = 1);

    // Получить топ-N результатов по качеству
    static std::vector<TaskResult> get_top_results(
        const PipelineResult& result,
        int top_n);
};

} // namespace rza