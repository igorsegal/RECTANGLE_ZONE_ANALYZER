#pragma once
// ============================================================================
// bt_position.h — Одна позиция (открытие/закрытие)
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "backtest/bt_types.h"
#include "backtest/bt_lot_calculator.h"
#include "backtest/bt_commission.h"

namespace rza {

class Position {
public:
    Position();

    // Открыть позицию
    void open(
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
        double commission_per_lot);

    bool is_open() const;
    void update(const MqlRates& bar, int32_t bar_idx, double atr_value);
    void close(int64_t exit_time, double exit_price,
               int32_t exit_bar_idx, ExitReason reason);
    const Trade& get_trade() const;
    double current_profit_money() const;

private:
    Trade trade_;
    bool is_open_;
    double max_favorable_excursion_;
    double max_adverse_excursion_;
    double commission_per_lot_;
};

} // namespace rza