#pragma once
// ============================================================================
// opt_grid.h — Генерация сетки параметров для оптимизации
// ============================================================================

#include <vector>
#include "core/params.h"

namespace rza {

// ============================================================================
// Одна комбинация параметров
// ============================================================================
struct ParameterCombination {
    int      combo_id = -1;

    // Discovery параметры
    double   min_gap_atr = 0.30;
    double   breakout_atr = 0.10;
    double   reaction_atr = 1.0;
    double   local_breakout_atr = 0.10;
    int      max_local_bars = 30;
    int      max_structural_bars = 300;

    // Backtest параметры
    double   risk_percent = 1.0;
    int      max_concurrent_positions = 1;

    // НОВОЕ: параметры окна валидации (в неделях)
    int      opt_weeks = 3;
    int      oos_weeks = 1;
    int      step_weeks = 1;
};

// ============================================================================
// Класс для генерации сетки
// ============================================================================
class OptimizationGrid {
public:
    // Сгенерировать все комбинации
    static std::vector<ParameterCombination> generate_grid(
        const OptimizationParams& params);

    // Получить количество комбинаций
    static int get_total_combinations(const OptimizationParams& params);

private:
    // Создать одну комбинацию
    static ParameterCombination create_combination(
        int combo_id,
        double min_gap_atr,
        double breakout_atr,
        double reaction_atr,
        double local_breakout_atr,
        int max_local_bars,
        int max_structural_bars,
        double risk_percent,
        int max_concurrent_positions,
        int opt_weeks,
        int oos_weeks,
        int step_weeks);
};

} // namespace rza