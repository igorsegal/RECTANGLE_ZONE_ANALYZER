#pragma once
// ============================================================================
// twf_window.h — Скользящее окно для True Walk-Forward (Блок 06)
// ============================================================================
// ИЗМЕНЕНО: работа с неделями вместо месяцев
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>

namespace rza {

// ============================================================================
// Одно окно True Walk-Forward
// ============================================================================
struct TWFWindow {
    int      index = -1;

    // Границы оптимизационного окна
    int64_t  opt_start_time = 0;
    int64_t  opt_end_time = 0;

    // Границы валидационного окна
    int64_t  val_start_time = 0;
    int64_t  val_end_time = 0;

    // Общее окно (opt + val)
    int64_t  window_start_time = 0;
    int64_t  window_end_time = 0;
};

// ============================================================================
// Результат построения окон
// ============================================================================
struct TWFWindowSet {
    bool                    success = false;
    std::string             error_message;

    std::vector<TWFWindow>  windows;

    int64_t  history_start_time = 0;
    int64_t  history_end_time = 0;
    int      total_windows = 0;
};

// ============================================================================
// Класс для построения окон
// ============================================================================
class TWFWindowBuilder {
public:
    // Построить набор окон по истории
    // ИЗМЕНЕНО: параметры в неделях
    static TWFWindowSet build_windows(
        int64_t history_start_time,
        int64_t history_end_time,
        int opt_weeks = 12,
        int oos_weeks = 4,
        int step_weeks = 4);

private:
    // Добавить недели к Unix-времени
    static int64_t add_weeks(int64_t unix_time, int weeks);
};

} // namespace rza