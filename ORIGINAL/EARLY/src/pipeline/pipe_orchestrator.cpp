// ============================================================================
// pipe_orchestrator.cpp — Реализация оркестратора
// ============================================================================

#include "pipeline/pipe_orchestrator.h"
#include "pipeline/pipe_worker.h"
#include "io/bin_reader.h"
#include "utils/timer.h"
#include <algorithm>
#include <filesystem>

namespace rza {

// ============================================================================
// Создание задач из списка файлов
// ============================================================================
std::vector<PipelineTask> PipelineOrchestrator::create_tasks(
    const std::vector<std::string>& file_paths)
{
    std::vector<PipelineTask> tasks;
    tasks.reserve(file_paths.size());

    int task_id = 0;
    for (const auto& path : file_paths) {
        PipelineTask task;
        task.task_id = task_id++;
        task.file_path = path;

        std::string filename = std::filesystem::path(path).filename().string();
        std::string symbol, tf;
        if (BinReader::parse_filename(filename, symbol, tf)) {
            task.symbol = symbol;
            task.timeframe = tf;
        } else {
            task.symbol = "UNKNOWN";
            task.timeframe = "??";
        }

        tasks.push_back(task);
    }

    return tasks;
}

// ============================================================================
// Выполнение всех задач (пока однопоточно)
// ============================================================================
PipelineResult PipelineOrchestrator::run_all(
    std::vector<PipelineTask>& tasks,
    const SystemParams& params,
    int num_threads)
{
    PipelineResult result;
    result.success = false;
    result.total_tasks = static_cast<int32_t>(tasks.size());

    (void)num_threads;  // пока не используется, зарезервировано для будущего

    Timer total_timer;

    for (auto& task : tasks) {
        task.status = TaskStatus::RUNNING;
        task.progress_message = "Processing...";

        TaskResult task_result = PipelineWorker::process_task(task, params);
        task.result = task_result;

        if (task_result.success) {
            task.status = TaskStatus::COMPLETED;
            result.completed_tasks++;

            result.total_bars_processed += task_result.bars_count;
            result.total_zones_found += task_result.zones_found;
            result.total_touches_found += task_result.touches_found;
            result.total_rules_accepted += task_result.rules_accepted;
            result.total_trades += task_result.total_trades;
            result.total_net_profit += task_result.net_profit;

            if (task_result.net_profit > 0.0) {
                result.profitable_symbols++;
            }
        } else {
            task.status = TaskStatus::FAILED;
            result.failed_tasks++;
        }

        task.progress_percent = 100;
    }

    result.total_processing_time_ms = total_timer.elapsed_ms();
    result.success = true;
    return result;
}

// ============================================================================
// Топ-N результатов
// ============================================================================
std::vector<TaskResult> PipelineOrchestrator::get_top_results(
    const PipelineResult& result,
    int top_n)
{
    std::vector<TaskResult> successful;
    for (const auto& task : result.tasks) {
        if (task.status == TaskStatus::COMPLETED && task.result.success) {
            successful.push_back(task.result);
        }
    }

    // Сортировка по quality_score (убывание)
    std::sort(successful.begin(), successful.end(),
              [](const TaskResult& a, const TaskResult& b) {
                  return a.quality_score > b.quality_score;
              });

    if (static_cast<int>(successful.size()) > top_n) {
        successful.resize(top_n);
    }

    return successful;
}

} // namespace rza