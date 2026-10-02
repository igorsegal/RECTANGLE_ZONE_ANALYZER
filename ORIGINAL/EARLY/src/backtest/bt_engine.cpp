// ============================================================================
// bt_engine.cpp — Реализация главного движка бэктеста
// ============================================================================
// ИСПРАВЛЕНИЕ: gross_loss теперь содержит модуль убытков (положительное число)
// ============================================================================

#include "backtest/bt_engine.h"
#include "backtest/bt_report_builder.h"
#include <cmath>
#include <algorithm>

namespace rza {

// ============================================================================
// Фильтрация правил по grade
// ============================================================================
static std::vector<FinalRule> filter_rules_by_grade(
    const std::vector<FinalRule>& rules,
    bool only_confirmed)
{
    if (!only_confirmed) {
        return rules;
    }
    
    std::vector<FinalRule> filtered;
    for (const auto& rule : rules) {
        if (rule.grade == "CONFIRMED_TWO_FOLDS" || 
            rule.grade == "SINGLE_FOLD_PASS" ||
            rule.accepted) {
            filtered.push_back(rule);
        }
    }
    return filtered;
}

// ============================================================================
// Бэктест одного правила
// ============================================================================
BacktestReport BacktestEngine::run_single_rule(
    const BacktestInput& input,
    const FinalRule& rule,
    int& trade_counter)
{
    BacktestReport report;
    report.symbol = input.symbol;
    report.timeframe = input.timeframe;
    report.rule_id = rule.rule_id;

    LotCalculatorParams lot_params;
    lot_params.deposit = input.bt_params.deposit;
    lot_params.risk_percent = input.bt_params.risk_percent;
    lot_params.min_lot = input.bt_params.min_lot;
    lot_params.lot_step = input.bt_params.lot_step;
    lot_params.max_lot = input.bt_params.max_lot;
    lot_params.point = input.point;
    lot_params.tick_value = 10.0;

    Position position;
    double equity = input.bt_params.deposit;
    double peak_equity = equity;

    const int total_bars = static_cast<int>(input.bars.size());
    int last_entry_bar = -input.bt_params.min_bars_between_entries;

    for (int bar_idx = 0; bar_idx < total_bars; ++bar_idx) {
        const MqlRates& bar = input.bars[bar_idx];
        double atr_value = (bar_idx < static_cast<int>(input.atr.size()))
                           ? input.atr[bar_idx] : 0.0;

        if (position.is_open()) {
            position.update(bar, bar_idx, atr_value);

            bool should_close = false;
            ExitReason reason = ExitReason::TIMEOUT;

            if (rule.direction == "BUY") {
                if (bar.low <= position.get_trade().stop_loss_price) {
                    should_close = true;
                    reason = ExitReason::SL_BREAKOUT;
                } else if (bar.high >= position.get_trade().take_profit_price) {
                    should_close = true;
                    reason = ExitReason::TP_REACTION;
                }
            } else {
                if (bar.high >= position.get_trade().stop_loss_price) {
                    should_close = true;
                    reason = ExitReason::SL_BREAKOUT;
                } else if (bar.low <= position.get_trade().take_profit_price) {
                    should_close = true;
                    reason = ExitReason::TP_REACTION;
                }
            }

            if (!should_close) {
                int bars_held = bar_idx - position.get_trade().entry_bar_idx;
                if (bars_held >= input.bt_params.max_bars_per_trade) {
                    should_close = true;
                    reason = ExitReason::TIMEOUT;
                }
            }

            if (!should_close && bar_idx == total_bars - 1) {
                should_close = true;
                reason = ExitReason::END_OF_HISTORY;
            }

            if (should_close) {
                double exit_price = bar.close;
                position.close(bar.time, exit_price, bar_idx, reason);

                const Trade& trade = position.get_trade();
                report.trades.push_back(trade);
                report.total_trades++;

                equity += trade.net_profit_money;
                report.total_commission += trade.commission_money;

                // ИСПРАВЛЕНИЕ: gross_loss теперь содержит МОДУЛЬ убытков
                // (положительное число), а не отрицательное
                if (trade.is_winning()) {
                    report.winning_trades++;
                    report.gross_profit += trade.net_profit_money;
                } else if (trade.is_losing()) {
                    report.losing_trades++;
                    report.gross_loss += (-trade.net_profit_money);  // ← ИСПРАВЛЕНИЕ
                } else {
                    report.breakeven_trades++;
                }

                switch (trade.exit_reason) {
                    case ExitReason::TP_REACTION:    report.exit_tp_reaction++; break;
                    case ExitReason::TP_STRUCTURAL:  report.exit_tp_structural++; break;
                    case ExitReason::SL_BREAKOUT:    report.exit_sl_breakout++; break;
                    case ExitReason::TIMEOUT:        report.exit_timeout++; break;
                    case ExitReason::END_OF_HISTORY: report.exit_end_of_history++; break;
                }

                if (equity > peak_equity) peak_equity = equity;
            }
        }

        if (!position.is_open()) {
            bool can_enter = 
                atr_value > 0.0 && 
                bar_idx > 14 &&
                (bar_idx - last_entry_bar) >= input.bt_params.min_bars_between_entries &&
                bar_idx < total_bars - input.bt_params.max_bars_per_trade;

            if (can_enter) {
                trade_counter++;
                last_entry_bar = bar_idx;

                double entry_price = bar.close;
                double sl_distance = atr_value * input.bt_params.sl_atr_multiplier;
                double tp_distance = atr_value * input.bt_params.tp_atr_multiplier;

                double stop_loss, take_profit;
                TradeDirection direction;

                if (rule.direction == "BUY") {
                    direction = TradeDirection::BUY;
                    stop_loss = entry_price - sl_distance;
                    take_profit = entry_price + tp_distance;
                } else {
                    direction = TradeDirection::SELL;
                    stop_loss = entry_price + sl_distance;
                    take_profit = entry_price - tp_distance;
                }

                double lot = LotCalculator::calculate_lot(sl_distance, lot_params);

                position.open(
                    trade_counter,
                    rule.rule_id,
                    input.symbol,
                    input.timeframe,
                    direction,
                    bar.time,
                    entry_price,
                    bar_idx,
                    stop_loss,
                    take_profit,
                    lot,
                    input.point,
                    input.bt_params.commission_per_lot
                );
            }
        }
    }

    report = ReportBuilder::build_report(report, input.bt_params.deposit);

    return report;
}

// ============================================================================
// Построение суммарного отчёта
// ============================================================================
BacktestReport BacktestEngine::build_combined_report(
    const std::vector<BacktestReport>& reports,
    const std::string& symbol,
    const std::string& timeframe,
    double initial_deposit)
{
    BacktestReport combined;
    combined.symbol = symbol;
    combined.timeframe = timeframe;
    combined.rule_id = "COMBINED";

    for (const auto& report : reports) {
        combined.total_trades += report.total_trades;
        combined.winning_trades += report.winning_trades;
        combined.losing_trades += report.losing_trades;
        combined.breakeven_trades += report.breakeven_trades;
        combined.gross_profit += report.gross_profit;
        combined.gross_loss += report.gross_loss;
        combined.total_commission += report.total_commission;

        combined.exit_tp_reaction += report.exit_tp_reaction;
        combined.exit_tp_structural += report.exit_tp_structural;
        combined.exit_sl_breakout += report.exit_sl_breakout;
        combined.exit_timeout += report.exit_timeout;
        combined.exit_end_of_history += report.exit_end_of_history;

        for (const auto& trade : report.trades) {
            combined.trades.push_back(trade);
        }
    }

    // ИСПРАВЛЕНИЕ: gross_loss теперь положительное, поэтому вычитаем
    combined.net_profit = combined.gross_profit - combined.gross_loss;
    combined = ReportBuilder::build_report(combined, initial_deposit);

    return combined;
}

// ============================================================================
// Главный метод
// ============================================================================
BacktestResult BacktestEngine::run(const BacktestInput& input) {
    BacktestResult result;
    result.success = false;

    if (input.bars.empty()) {
        result.error_message = "No bars to backtest";
        return result;
    }

    if (input.rules.empty()) {
        result.error_message = "No rules to backtest";
        return result;
    }

    std::vector<FinalRule> filtered_rules = filter_rules_by_grade(
        input.rules, input.bt_params.use_only_confirmed_rules);

    if (filtered_rules.empty()) {
        result.error_message = "No confirmed rules to backtest";
        return result;
    }

    result.total_rules = static_cast<int>(filtered_rules.size());

    int trade_counter = 0;

    for (const auto& rule : filtered_rules) {
        BacktestReport report = run_single_rule(input, rule, trade_counter);
        result.reports.push_back(report);
        result.total_trades += report.total_trades;
        result.total_net_profit += report.net_profit;
        result.total_commission += report.total_commission;
    }

    result.combined_report = build_combined_report(
        result.reports, input.symbol, input.timeframe, input.bt_params.deposit);

    result.success = true;
    return result;
}

} // namespace rza