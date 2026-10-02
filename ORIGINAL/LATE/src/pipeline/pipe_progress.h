#pragma once
// ============================================================================
// pipe_progress.h — Отслеживание прогресса Pipeline
// ============================================================================
// Одна ответственность: отображение прогресса обработки задач.
// ============================================================================

#include <vector>
#include <string>
#include "pipeline/pipe_task.h"

namespace rza {

class PipelineProgress {
public:
    // Вывести текущий прогресс
    static void print_progress(
        const std::vector<PipelineTask>& tasks,
        int completed,
        int failed,
        int total);

    // Вывести итоговую сводку
    static void print_summary(
        const std::vector<PipelineTask>& tasks,
        double total_time_ms);
};

} // namespace rza