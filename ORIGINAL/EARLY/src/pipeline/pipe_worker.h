#pragma once
// ============================================================================
// pipe_worker.h — Обработка одной задачи
// ============================================================================
// Одна ответственность: полный анализ одного .bin файла.
// ============================================================================

#include "pipeline/pipe_task.h"
#include "core/params.h"

namespace rza {

class PipelineWorker {
public:
    // Обработать одну задачу
    static TaskResult process_task(
        const PipelineTask& task,
        const SystemParams& params);

private:
    // Отдельные этапы обработки
    static bool run_discovery(
        const std::string& file_path,
        const SystemParams& params,
        TaskResult& result);

    static bool run_backtest(
        const TaskResult& discovery_result,
        const SystemParams& params,
        TaskResult& result);
};

} // namespace rza