// ============================================================================
// strict_controller.cpp — Реализация контроля удержания
// ============================================================================

#include "backtest/strict/strict_controller.h"
#include <cmath>
#include <algorithm>

namespace rza {

// ============================================================================
// Проверка условий закрытия
// ============================================================================
bool StrictController::check_exit_conditions(
    const StrictPosition& sp,
    const MqlRates& bar,
    int bar_idx,
    const ZoneState& zone,
    ExitReason& reason,
    double& exit_price)
{
    const Trade& trade = sp.position.get_trade();

    // 1. Проверка TP
    if (sp.direction == TradeDirection::BUY) {
        if (bar.high >= trade.take_profit_price) {
            reason = ExitReason::TP_REACTION;
            exit_price = trade.take_profit_price;
            return true;
        }
    } else {
        if (bar.low <= trade.take_profit_price) {
            reason = ExitReason::TP_REACTION;
            exit_price = trade.take_profit_price;
            return true;
        }
    }

    // 2. Проверка SL
    if (sp.direction == TradeDirection::BUY) {
        if (bar.low <= trade.stop_loss_price) {
            reason = ExitReason::SL_BREAKOUT;
            exit_price = trade.stop_loss_price;
            return true;
        }
    } else {
        if (bar.high >= trade.stop_loss_price) {
            reason = ExitReason::SL_BREAKOUT;
            exit_price = trade.stop_loss_price;
            return true;
        }
    }

    // 3. Проверка пробития зоны (close за границей)
    if (zone.is_active) {
        if (sp.direction == TradeDirection::BUY && bar.close < zone.zone_low) {
            reason = ExitReason::SL_BREAKOUT;
            exit_price = bar.close;
            return true;
        }
        if (sp.direction == TradeDirection::SELL && bar.close > zone.zone_high) {
            reason = ExitReason::SL_BREAKOUT;
            exit_price = bar.close;
            return true;
        }
    }

    // 4. Таймаут
    int bars_held = bar_idx - trade.entry_bar_idx;
    // Используем max_bars_per_trade из параметров
    // Временно используем 30 как дефолт (будет передано через bt_params)
    if (bars_held >= 30) {
        reason = ExitReason::TIMEOUT;
        exit_price = bar.close;
        return true;
    }

    return false;
}

// ============================================================================
// Применение управления прибылью
// ============================================================================
void StrictController::apply_profit_management(
    StrictPosition& sp,
    const MqlRates& bar,
    double atr_value,
    const BacktestParams& bt_params)
{
    if (sp.breakeven_moved) return;
    if (sp.position.get_trade().direction != sp.direction) return;

    const Trade& trade = sp.position.get_trade();
    double current_price = bar.close;
    double price_move = 0.0;

    if (sp.direction == TradeDirection::BUY) {
        price_move = current_price - sp.entry_price;
    } else {
        price_move = sp.entry_price - current_price;
    }

    // Цена должна двигаться в нашу сторону
    if (price_move <= 0.0) return;

    bool should_move = false;

    switch (bt_params.profit_management) {
        case ProfitManagement::STATIC:
            // Ничего не делаем
            return;

        case ProfitManagement::BREAKEVEN:
            // Проверка порога переноса
            if (bt_params.breakeven_trigger == BreakevenTrigger::ATR_0_6) {
                // Порог = 0.6 ATR
                if (price_move >= bt_params.breakeven_atr_threshold * atr_value) {
                    should_move = true;
                }
            } else {
                // HALF_TP: половина пути до тейка
                if (sp.tp_distance > 0.0 && price_move >= sp.tp_distance * 0.5) {
                    should_move = true;
                }
            }
            break;

        case ProfitManagement::TRAILING:
            // Трейлинг обрабатывается отдельно в apply_trailing_by_zones
            return;

        default:
            return;
    }

    if (should_move) {
        // Переносим SL в точку входа (безубыток)
        // Это делается через модификацию stop_loss_price в trade
        // Поскольку Trade — константный в get_trade(), используем const_cast
        Trade& mutable_trade = const_cast<Trade&>(trade);
        mutable_trade.stop_loss_price = sp.entry_price;
        sp.breakeven_moved = true;
    }
}

// ============================================================================
// Трейлинг по зонам
// ============================================================================
void StrictController::apply_trailing_by_zones(
    StrictPosition& sp,
    const std::vector<ZoneState>& zones,
    const MqlRates& bar)
{
    if (sp.position.get_trade().direction != sp.direction) return;

    Trade& trade = const_cast<Trade&>(sp.position.get_trade());
    double current_price = bar.close;

    if (sp.direction == TradeDirection::BUY) {
        // Для BUY: ищем ближайшую зону ниже текущей цены
        // и устанавливаем SL на её нижней границе
        double best_sl = trade.stop_loss_price;
        for (const auto& zone : zones) {
            if (!zone.is_active) continue;
            if (zone.zone_type != ZoneType::BULL) continue;
            // Зона должна быть ниже текущей цены
            if (zone.zone_high < current_price) {
                // SL = lower boundary зоны
                if (zone.zone_low > best_sl) {
                    best_sl = zone.zone_low;
                }
            }
        }
        if (best_sl > trade.stop_loss_price) {
            trade.stop_loss_price = best_sl;
        }
    } else {
        // Для SELL: ищем ближайшую зону выше текущей цены
        double best_sl = trade.stop_loss_price;
        for (const auto& zone : zones) {
            if (!zone.is_active) continue;
            if (zone.zone_type != ZoneType::BEAR) continue;
            if (zone.zone_low > current_price) {
                if (zone.zone_high < best_sl) {
                    best_sl = zone.zone_high;
                }
            }
        }
        if (best_sl < trade.stop_loss_price) {
            trade.stop_loss_price = best_sl;
        }
    }
}

// ============================================================================
// Главная функция контроллера
// ============================================================================
ControlResult StrictController::update_positions(
    std::vector<StrictPosition>& current_positions,
    std::vector<ZoneState>& zones,
    const MqlRates& bar,
    int bar_idx,
    double atr_value,
    const BacktestParams& bt_params)
{
    ControlResult result;

    // Обрабатываем позиции в обратном порядке, чтобы безопасно удалять
    for (int i = static_cast<int>(current_positions.size()) - 1; i >= 0; --i) {
        StrictPosition& sp = current_positions[i];
        if (!sp.position.is_open()) continue;

        // Применяем управление прибылью (до проверки выхода)
        if (bt_params.profit_management == ProfitManagement::BREAKEVEN) {
            apply_profit_management(sp, bar, atr_value, bt_params);
        } else if (bt_params.profit_management == ProfitManagement::TRAILING) {
            apply_trailing_by_zones(sp, zones, bar);
        }

        // Проверяем условия закрытия
        ExitReason reason = ExitReason::TIMEOUT;
        double exit_price = bar.close;

        const ZoneState& zone = (sp.zone_id >= 0 && sp.zone_id < static_cast<int>(zones.size()))
                                ? zones[sp.zone_id]
                                : ZoneState();

        bool should_close = check_exit_conditions(sp, bar, bar_idx, zone, reason, exit_price);

        if (should_close) {
            sp.position.close(bar.time, exit_price, bar_idx, reason);
            result.closed_trades.push_back(sp.position.get_trade());
            result.closed_position_indexes.push_back(i);
        }
    }

    return result;
}

} // namespace rza