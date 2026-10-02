// ============================================================================
// bt_report_builder.cpp — Реализация построителя отчёта
// ============================================================================
// ИСПРАВЛЕНИЕ: gross_loss теперь положительное число (модуль убытков)
// ============================================================================

#include "backtest/bt_report_builder.h"
#include "utils/math_utils.h"
#include <algorithm>
#include <cmath>

namespace rza {

double ReportBuilder::calculate_winrate(int winning, int total) {
    if (total <= 0) return 0.0;
    return 100.0 * static_cast<double>(winning) / static_cast<double>(total);
}

double ReportBuilder::calculate_profit_factor(double gross_profit, double gross_loss) {
    // ИСПРАВЛЕНИЕ: gross_loss теперь положительное число (модуль убытков)
    // Если убытков нет (gross_loss <= 0) — PF = 0 (нельзя делить на 0)
    if (gross_loss <= 0.0) return 0.0;
    if (gross_profit <= 0.0) return 0.0;
    return gross_profit / gross_loss;
}

double ReportBuilder::calculate_max_drawdown(
    const std::vector<Trade>& trades,
    double initial_deposit)
{
    if (trades.empty()) return 0.0;

    double equity = initial_deposit;
    double peak = initial_deposit;
    double max_dd = 0.0;

    for (const auto& trade : trades) {
        equity += trade.net_profit_money;
        if (equity > peak) peak = equity;
        double dd = peak - equity;
        if (dd > max_dd) max_dd = dd;
    }

    return max_dd;
}

BacktestReport ReportBuilder::build_report(
    const BacktestReport& raw_report,
    double initial_deposit)
{
    BacktestReport report = raw_report;

    // Winrate
    report.winrate_pct = calculate_winrate(report.winning_trades, report.total_trades);

    // Profit factor
    report.profit_factor = calculate_profit_factor(report.gross_profit, report.gross_loss);

    // ИСПРАВЛЕНИЕ: gross_loss теперь положительное, поэтому ВЫЧИТАЕМ
    // NetProfit = GrossProfit - GrossLoss (оба положительные)
    report.net_profit = report.gross_profit - report.gross_loss;

    // Средние
    if (report.winning_trades > 0) {
        report.avg_win_money = report.gross_profit / report.winning_trades;
    }
    if (report.losing_trades > 0) {
        // ИСПРАВЛЕНИЕ: gross_loss положительное, поэтому avg_loss_money тоже положительное
        // (средний убыток по модулю)
        report.avg_loss_money = report.gross_loss / report.losing_trades;
    }
    if (report.total_trades > 0) {
        report.avg_trade_money = report.net_profit / report.total_trades;
    }

    // Средняя длительность
    if (!report.trades.empty()) {
        double total_minutes = 0.0;
        for (const auto& trade : report.trades) {
            total_minutes += trade.duration_minutes;
        }
        report.avg_duration_minutes = total_minutes / report.trades.size();

        if (!report.trades.empty()) {
            report.first_trade_time = report.trades.front().entry_time;
            report.last_trade_time = report.trades.back().exit_time;
        }
    }

    // Drawdown
    if (initial_deposit > 0.0) {
        report.max_drawdown_money = calculate_max_drawdown(report.trades, initial_deposit);
        report.max_drawdown_pct = 100.0 * report.max_drawdown_money / initial_deposit;
    }

    // Качество
    report.is_profitable = report.net_profit > 0.0;

    // Простая метрика качества: PF × sqrt(trades) × (1 - DD_pct/100)
    if (report.total_trades > 0 && report.profit_factor > 0.0) {
        double dd_factor = 1.0 - std::min(report.max_drawdown_pct / 100.0, 1.0);
        report.quality_score = report.profit_factor *
                               std::sqrt(static_cast<double>(report.total_trades)) *
                               dd_factor;
    }

    return report;
}

} // namespace rza