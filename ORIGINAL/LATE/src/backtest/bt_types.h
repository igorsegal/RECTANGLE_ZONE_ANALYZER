#pragma once
// ============================================================================
// bt_types.h — Типы данных для бэктеста
// ============================================================================
// Одна ответственность: объявление структур Trade и BacktestReport.
// Enum-ы TradeDirection и ExitReason уже определены в core/types.h.
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include "core/types.h"

namespace rza {

// ============================================================================
// Направление сделки (специфично для бэктеста)
// ============================================================================
enum class TradeDirection : int {
    BUY = 0,
    SELL = 1
};

// ============================================================================
// Одна сделка
// ============================================================================
struct Trade {
    int32_t      trade_id = -1;
    std::string  rule_id;
    std::string  symbol;
    std::string  timeframe;

    TradeDirection direction = TradeDirection::BUY;

    // Вход
    int64_t      entry_time = 0;
    double       entry_price = 0.0;
    int32_t      entry_bar_idx = -1;

    // Выход
    int64_t      exit_time = 0;
    double       exit_price = 0.0;
    int32_t      exit_bar_idx = -1;
    ExitReason   exit_reason = ExitReason::TIMEOUT;  // из core/types.h

    // Параметры
    double       lot = 0.0;
    double       stop_loss_price = 0.0;
    double       take_profit_price = 0.0;
    double       point = 0.0;

    // Результат
    double       profit_money = 0.0;
    double       commission_money = 0.0;
    double       net_profit_money = 0.0;
    double       profit_points = 0.0;
    double       profit_atr = 0.0;

    // Время жизни
    double       duration_minutes = 0.0;
    int32_t      duration_bars = 0;

    // Вспомогательные методы
    bool is_winning() const { return net_profit_money > 0.0; }
    bool is_losing() const { return net_profit_money < 0.0; }
    bool is_breakeven() const { return net_profit_money == 0.0; }
};

// ============================================================================
// Отчёт по бэктесту
// ============================================================================
struct BacktestReport {
    std::string  symbol;
    std::string  timeframe;
    std::string  rule_id;

    // Статистика сделок
    int32_t  total_trades = 0;
    int32_t  winning_trades = 0;
    int32_t  losing_trades = 0;
    int32_t  breakeven_trades = 0;

    // Прибыль/убыток
    double   gross_profit = 0.0;
    double   gross_loss = 0.0;
    double   net_profit = 0.0;
    double   total_commission = 0.0;

    // Проценты
    double   winrate_pct = 0.0;
    double   profit_factor = 0.0;
    double   avg_win_money = 0.0;
    double   avg_loss_money = 0.0;
    double   avg_trade_money = 0.0;

    // Drawdown
    double   max_drawdown_money = 0.0;
    double   max_drawdown_pct = 0.0;

    // Время
    double   avg_duration_minutes = 0.0;
    int64_t  first_trade_time = 0;
    int64_t  last_trade_time = 0;

    // Распределение по exit_reason
    int32_t  exit_tp_reaction = 0;
    int32_t  exit_tp_structural = 0;
    int32_t  exit_sl_breakout = 0;
    int32_t  exit_timeout = 0;
    int32_t  exit_end_of_history = 0;

    // Список сделок
    std::vector<Trade> trades;

    // Качество
    double   quality_score = 0.0;
    bool     is_profitable = false;
};

// ============================================================================
// Вспомогательные функции
// ============================================================================

inline const char* trade_direction_str(TradeDirection d) {
    return d == TradeDirection::BUY ? "BUY" : "SELL";
}

inline const char* bt_exit_reason_str(ExitReason r) {
    switch (r) {
        case ExitReason::TP_REACTION:     return "TP_REACTION";
        case ExitReason::TP_STRUCTURAL:   return "TP_STRUCTURAL";
        case ExitReason::SL_BREAKOUT:     return "SL_BREAKOUT";
        case ExitReason::TIMEOUT:         return "TIMEOUT";
        case ExitReason::END_OF_HISTORY:  return "END_OF_HISTORY";
        default:                          return "UNKNOWN";
    }
}

} // namespace rza