// ============================================================================
// bt_position.cpp — Реализация управления позицией
// ============================================================================
// ИСПРАВЛЕНИЕ: корректная формула P&L с contract_size
// ============================================================================

#include "backtest/bt_position.h"
#include <cmath>
#include <algorithm>

namespace rza {

// Стандартный contract size для форекса (1 стандартный лот = 100,000 единиц)
static constexpr double FOREX_CONTRACT_SIZE = 100000.0;

Position::Position() : is_open_(false), max_favorable_excursion_(0.0),
                       max_adverse_excursion_(0.0), commission_per_lot_(4.0) {}

void Position::open(
    int32_t trade_id,
    const std::string& rule_id,
    const std::string& symbol,
    const std::string& timeframe,
    TradeDirection direction,
    int64_t entry_time,
    double entry_price,
    int32_t entry_bar_idx,
    double stop_loss_price,
    double take_profit_price,
    double lot,
    double point,
    double commission_per_lot)
{
    trade_.trade_id = trade_id;
    trade_.rule_id = rule_id;
    trade_.symbol = symbol;
    trade_.timeframe = timeframe;
    trade_.direction = direction;
    trade_.entry_time = entry_time;
    trade_.entry_price = entry_price;
    trade_.entry_bar_idx = entry_bar_idx;
    trade_.stop_loss_price = stop_loss_price;
    trade_.take_profit_price = take_profit_price;
    trade_.lot = lot;
    trade_.point = point;

    commission_per_lot_ = commission_per_lot;

    is_open_ = true;
    max_favorable_excursion_ = 0.0;
    max_adverse_excursion_ = 0.0;
}

bool Position::is_open() const {
    return is_open_;
}

void Position::update(const MqlRates& bar, int32_t bar_idx, double atr_value) {
    if (!is_open_) return;

    double current_price = bar.close;
    double price_diff;

    if (trade_.direction == TradeDirection::BUY) {
        price_diff = current_price - trade_.entry_price;
    } else {
        price_diff = trade_.entry_price - current_price;
    }

    // ИСПРАВЛЕНИЕ: корректная формула P&L для форекса
    // P&L = price_diff × contract_size × lot
    trade_.profit_money = price_diff * FOREX_CONTRACT_SIZE * trade_.lot;
    
    // В пунктах
    trade_.profit_points = price_diff / trade_.point;

    // В ATR
    if (atr_value > 0.0) {
        trade_.profit_atr = price_diff / atr_value;
    }

    // MFE/MAE
    if (trade_.profit_money > max_favorable_excursion_) {
        max_favorable_excursion_ = trade_.profit_money;
    }
    if (trade_.profit_money < max_adverse_excursion_) {
        max_adverse_excursion_ = trade_.profit_money;
    }

    trade_.duration_bars = bar_idx - trade_.entry_bar_idx;
    double seconds = static_cast<double>(bar.time - trade_.entry_time);
    if (seconds < 0.0) seconds = 0.0;
    trade_.duration_minutes = seconds / 60.0;
}

void Position::close(int64_t exit_time, double exit_price,
                     int32_t exit_bar_idx, ExitReason reason) {
    if (!is_open_) return;

    trade_.exit_time = exit_time;
    trade_.exit_price = exit_price;
    trade_.exit_bar_idx = exit_bar_idx;
    trade_.exit_reason = reason;

    double price_diff;
    if (trade_.direction == TradeDirection::BUY) {
        price_diff = exit_price - trade_.entry_price;
    } else {
        price_diff = trade_.entry_price - exit_price;
    }

    // ИСПРАВЛЕНИЕ: корректная формула P&L
    trade_.profit_money = price_diff * FOREX_CONTRACT_SIZE * trade_.lot;
    trade_.profit_points = price_diff / trade_.point;

    // Комиссия = 2 стороны × commission_per_lot × lot
    trade_.commission_money = 2.0 * commission_per_lot_ * trade_.lot;
    trade_.net_profit_money = trade_.profit_money - trade_.commission_money;

    trade_.duration_bars = exit_bar_idx - trade_.entry_bar_idx;
    double seconds = static_cast<double>(exit_time - trade_.entry_time);
    if (seconds < 0.0) seconds = 0.0;
    trade_.duration_minutes = seconds / 60.0;

    is_open_ = false;
}

const Trade& Position::get_trade() const {
    return trade_;
}

double Position::current_profit_money() const {
    return trade_.profit_money;
}

} // namespace rza