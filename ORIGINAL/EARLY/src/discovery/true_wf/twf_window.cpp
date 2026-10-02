// ============================================================================
// twf_window.cpp — Реализация построения окон
// ============================================================================
// ИЗМЕНЕНО: работа с неделями
// ============================================================================

#include "discovery/true_wf/twf_window.h"
#include <ctime>

namespace rza {

// ============================================================================
// Добавление недель к Unix-времени
// ============================================================================
int64_t TWFWindowBuilder::add_weeks(int64_t unix_time, int weeks) {
    // 1 неделя = 7 дней = 7 * 24 * 60 * 60 секунд = 604800 секунд
    return unix_time + static_cast<int64_t>(weeks) * 604800LL;
}

// ============================================================================
// Построение набора окон
// ============================================================================
TWFWindowSet TWFWindowBuilder::build_windows(
    int64_t history_start_time,
    int64_t history_end_time,
    int opt_weeks,
    int oos_weeks,
    int step_weeks)
{
    TWFWindowSet result;
    result.success = false;

    if (opt_weeks <= 0 || oos_weeks <= 0 || step_weeks <= 0) {
        result.error_message = "Invalid window parameters";
        return result;
    }

    if (history_start_time >= history_end_time) {
        result.error_message = "Invalid history time range";
        return result;
    }

    result.history_start_time = history_start_time;
    result.history_end_time = history_end_time;

    int window_index = 0;
    int64_t current_start = history_start_time;

    while (true) {
        int64_t opt_end = add_weeks(current_start, opt_weeks);
        int64_t window_end = add_weeks(opt_end, oos_weeks);

        // Если окно выходит за пределы истории — останавливаемся
        if (window_end > history_end_time) break;

        TWFWindow w;
        w.index = window_index;
        w.window_start_time = current_start;
        w.window_end_time = window_end;
        w.opt_start_time = current_start;
        w.opt_end_time = opt_end;
        w.val_start_time = opt_end;
        w.val_end_time = window_end;

        result.windows.push_back(w);
        window_index++;

        // Скользим на шаг
        current_start = add_weeks(current_start, step_weeks);

        // Защита от бесконечного цикла
        if (current_start >= history_end_time) break;
    }

    result.total_windows = window_index;
    result.success = true;
    return result;
}

} // namespace rza