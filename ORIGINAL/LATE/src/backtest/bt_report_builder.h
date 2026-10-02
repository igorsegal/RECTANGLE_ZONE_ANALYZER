#pragma once
// ============================================================================
// bt_report_builder.h — Построитель отчёта по бэктесту
// ============================================================================
// Одна ответственность: вычисление метрик отчёта (winrate, PF, drawdown).
// ============================================================================

#include "backtest/bt_types.h"

namespace rza {

class ReportBuilder {
public:
    // Построить полный отчёт с метриками
    static BacktestReport build_report(
        const BacktestReport& raw_report,
        double initial_deposit);

    // Вычислить winrate
    static double calculate_winrate(int winning, int total);

    // Вычислить profit factor
    static double calculate_profit_factor(double gross_profit, double gross_loss);

    // Вычислить максимальный drawdown
    static double calculate_max_drawdown(
        const std::vector<Trade>& trades,
        double initial_deposit);
};

} // namespace rza