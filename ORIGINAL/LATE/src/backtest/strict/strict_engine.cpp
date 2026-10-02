// ============================================================================
// strict_engine.cpp — Реализация строгого движка
// ============================================================================

#include "backtest/strict/strict_engine.h"
#include "backtest/bt_report_builder.h"
#include <cmath>
#include <algorithm>

namespace rza {

StrictBacktestResult StrictEngine::run(const StrictBacktestInput& input) {
    StrictBacktestResult result;
    result.success = false;

    if (input.bars.empty()) {
        result.error_message = "No bars to backtest";
        return result;
    }

    if (input.zones.empty()) {
        result.error_message = "No zones to backtest";
        return result;
    }

    if (input.rules.empty()) {
        result.error_message = "No rules to backtest";
        return result;
    }

    // Инициализация контекста
    StrictContext ctx;
    ctx.bars = &input.bars;
    ctx.atr = &input.atr;
    ctx.rules = &input.rules;
    ctx.bt_params = &input.bt_params;
    ctx.total_bars = static_cast<int>(input.bars.size());
    ctx.point = input.point;
    ctx.period_seconds = input.period_seconds;

    // Инициализация зон
    auto zones = StrictMonitor::init_zones(input.zones);
    ctx.zones = &zones;

    // Состояние бэктеста
    std::vector<StrictPosition> current_positions;
    int trade_counter = 0;
    double equity = input.bt_params.deposit;

    // Главный цикл по барам
    for (int bar_idx = 0; bar_idx < ctx.total_bars; ++bar_idx) {
        const MqlRates& bar = input.bars[bar_idx];
        double atr_value = (bar_idx < static_cast<int>(input.atr.size()))
                           ? input.atr[bar_idx] : 0.0;

        ctx.current_bar_idx = bar_idx;
        ctx.current_atr = atr_value;

        // Шаг 1: Контроль открытых позиций (проверка выхода)
        auto control_result = StrictController::update_positions(
            current_positions, zones, bar, bar_idx, atr_value, input.bt_params);

        // Обрабатываем закрытые позиции
        for (const auto& trade : control_result.closed_trades) {
            result.trades.push_back(trade);
            equity += trade.net_profit_money;
        }

        // Удаляем закрытые позиции из current_positions
        for (int i = static_cast<int>(control_result.closed_position_indexes.size()) - 1; i >= 0; --i) {
            int idx = control_result.closed_position_indexes[i];
            if (idx >= 0 && idx < static_cast<int>(current_positions.size())) {
                current_positions.erase(current_positions.begin() + idx);
            }
        }

        // Шаг 2: Мониторинг рынка (поиск касаний зон)
        auto touch_events = StrictMonitor::scan_bar(bar, bar_idx, zones, atr_value);

        // Шаг 3: Фильтрация правил для каждого касания
        std::vector<EntrySignal> all_signals;
        for (const auto& event : touch_events) {
            auto signals = StrictFilter::check_rules(event, input.rules, ctx);
            result.total_signals_generated += static_cast<int>(input.rules.size());
            result.signals_passed_filter += static_cast<int>(signals.size());
            all_signals.insert(all_signals.end(), signals.begin(), signals.end());
        }

        // Шаг 4: Диспетчеризация сигналов (открытие позиций)
        if (!all_signals.empty()) {
            auto dispatch_result = StrictDispatcher::dispatch(
                all_signals,
                current_positions,
                zones,
                trade_counter,
                input.bt_params,
                input.symbol,
                input.timeframe,
                input.point
            );

            result.positions_opened += static_cast<int>(dispatch_result.opened_positions.size());
            result.positions_rejected_by_limit += dispatch_result.rejected_by_limit;
            result.positions_rejected_by_reuse += dispatch_result.rejected_by_reuse;
        }
    }

    // Построение отчёта
    result.report = build_report(result.trades, input.symbol, input.timeframe, input.bt_params.deposit);
    result.success = true;

    return result;
}

BacktestReport StrictEngine::build_report(
    const std::vector<Trade>& trades,
    const std::string& symbol,
    const std::string& timeframe,
    double initial_deposit)
{
    BacktestReport report;
    report.symbol = symbol;
    report.timeframe = timeframe;
    report.rule_id = "STRICT_ENGINE";
    report.trades = trades;

    // Подсчёт статистики
    for (const auto& trade : trades) {
        report.total_trades++;

        if (trade.is_winning()) {
            report.winning_trades++;
            report.gross_profit += trade.net_profit_money;
        } else if (trade.is_losing()) {
            report.losing_trades++;
            report.gross_loss += (-trade.net_profit_money);  // модуль убытка
        } else {
            report.breakeven_trades++;
        }

        report.total_commission += trade.commission_money;

        switch (trade.exit_reason) {
            case ExitReason::TP_REACTION:    report.exit_tp_reaction++; break;
            case ExitReason::TP_STRUCTURAL:  report.exit_tp_structural++; break;
            case ExitReason::SL_BREAKOUT:    report.exit_sl_breakout++; break;
            case ExitReason::TIMEOUT:        report.exit_timeout++; break;
            case ExitReason::END_OF_HISTORY: report.exit_end_of_history++; break;
        }
    }

    // Построение финального отчёта через ReportBuilder
    report = ReportBuilder::build_report(report, initial_deposit);

    return report;
}

} // namespace rza