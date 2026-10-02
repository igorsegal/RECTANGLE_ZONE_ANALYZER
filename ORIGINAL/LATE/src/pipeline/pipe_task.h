#pragma once
// ============================================================================
// pipe_task.h — Задача для одного файла (Symbol+TF)
// ============================================================================
// Одна ответственность: описание задачи обработки одного .bin файла.
// ============================================================================

#include <string>
#include <vector>
#include "core/types.h"
#include "discovery/final_rules/fr_decoder.h"
#include "backtest/bt_types.h"

namespace rza {

// ============================================================================
// Статус задачи
// ============================================================================
enum class TaskStatus : int {
    PENDING = 0,
    RUNNING = 1,
    COMPLETED = 2,
    FAILED = 3
};

// ============================================================================
// Результат обработки одного файла
// ============================================================================
struct TaskResult {
    bool         success = false;
    std::string  error_message;

    std::string  symbol;
    std::string  timeframe;
    std::string  file_path;

    // Discovery статистика
    int32_t  bars_count = 0;
    int32_t  zones_found = 0;
    int32_t  touches_found = 0;
    int32_t  patterns_evaluated = 0;
    int32_t  rules_accepted = 0;

    // Backtest статистика
    int32_t  total_trades = 0;
    int32_t  winning_trades = 0;
    int32_t  losing_trades = 0;
    double   net_profit = 0.0;
    double   profit_factor = 0.0;
    double   winrate_pct = 0.0;
    double   max_drawdown_pct = 0.0;
    double   quality_score = 0.0;

    // Время выполнения
    double   processing_time_ms = 0.0;

    // Принятые правила
    std::vector<FinalRule> accepted_rules;

    // Отчёт бэктеста
    BacktestReport backtest_report;
};

// ============================================================================
// Задача обработки одного файла
// ============================================================================
struct PipelineTask {
    int          task_id = -1;
    std::string  file_path;
    std::string  symbol;
    std::string  timeframe;

    TaskStatus   status = TaskStatus::PENDING;
    TaskResult   result;

    int          progress_percent = 0;
    std::string  progress_message;
};

// ============================================================================
// Вспомогательные функции
// ============================================================================

inline const char* task_status_str(TaskStatus s) {
    switch (s) {
        case TaskStatus::PENDING:   return "PENDING";
        case TaskStatus::RUNNING:   return "RUNNING";
        case TaskStatus::COMPLETED: return "COMPLETED";
        case TaskStatus::FAILED:    return "FAILED";
        default:                    return "UNKNOWN";
    }
}

} // namespace rza