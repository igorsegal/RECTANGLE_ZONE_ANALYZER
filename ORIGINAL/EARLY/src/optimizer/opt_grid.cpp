// ============================================================================
// opt_grid.cpp — Реализация генерации сетки
// ============================================================================

#include "optimizer/opt_grid.h"

namespace rza {

ParameterCombination OptimizationGrid::create_combination(
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
    int step_weeks)
{
    ParameterCombination combo;
    combo.combo_id = combo_id;
    combo.min_gap_atr = min_gap_atr;
    combo.breakout_atr = breakout_atr;
    combo.reaction_atr = reaction_atr;
    combo.local_breakout_atr = local_breakout_atr;
    combo.max_local_bars = max_local_bars;
    combo.max_structural_bars = max_structural_bars;
    combo.risk_percent = risk_percent;
    combo.max_concurrent_positions = max_concurrent_positions;
    combo.opt_weeks = opt_weeks;
    combo.oos_weeks = oos_weeks;
    combo.step_weeks = step_weeks;
    return combo;
}

int OptimizationGrid::get_total_combinations(const OptimizationParams& params) {
    return static_cast<int>(
        params.min_gap_atr_values.size() *
        params.breakout_atr_values.size() *
        params.reaction_atr_values.size() *
        params.local_breakout_atr_values.size() *
        params.max_local_bars_values.size() *
        params.max_structural_bars_values.size() *
        params.risk_percent_values.size() *
        params.max_concurrent_pos_values.size() *
        params.opt_weeks_values.size() *
        params.oos_weeks_values.size() *
        params.step_weeks_values.size()
    );
}

std::vector<ParameterCombination> OptimizationGrid::generate_grid(
    const OptimizationParams& params)
{
    std::vector<ParameterCombination> grid;
    int total = get_total_combinations(params);
    grid.reserve(total);

    int combo_id = 0;

    for (double min_gap_atr : params.min_gap_atr_values) {
        for (double breakout_atr : params.breakout_atr_values) {
            for (double reaction_atr : params.reaction_atr_values) {
                for (double local_breakout_atr : params.local_breakout_atr_values) {
                    for (int max_local_bars : params.max_local_bars_values) {
                        for (int max_structural_bars : params.max_structural_bars_values) {
                            for (double risk_percent : params.risk_percent_values) {
                                for (int max_concurrent_pos : params.max_concurrent_pos_values) {
                                    // НОВОЕ: циклы по параметрам окна
                                    for (int opt_weeks : params.opt_weeks_values) {
                                        for (int oos_weeks : params.oos_weeks_values) {
                                            for (int step_weeks : params.step_weeks_values) {
                                                grid.push_back(create_combination(
                                                    combo_id++,
                                                    min_gap_atr,
                                                    breakout_atr,
                                                    reaction_atr,
                                                    local_breakout_atr,
                                                    max_local_bars,
                                                    max_structural_bars,
                                                    risk_percent,
                                                    max_concurrent_pos,
                                                    opt_weeks,
                                                    oos_weeks,
                                                    step_weeks
                                                ));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return grid;
}

} // namespace rza