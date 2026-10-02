// ============================================================================
// strict_dispatcher.cpp — Реализация диспетчера позиций
// ============================================================================

#include "backtest/strict/strict_dispatcher.h"
#include "backtest/bt_lot_calculator.h"
#include <cmath>

namespace rza {

// ============================================================================
// Проверка лимита одновременных позиций
// ============================================================================
bool StrictDispatcher::check_position_limit(
    const std::vector<StrictPosition>& current_positions,
    int max_concurrent)
{
    int active_count = 0;
    for (const auto& sp : current_positions) {
        if (sp.position.is_open()) active_count++;
    }
    return active_count < max_concurrent;
}

// ============================================================================
// Проверка правила повторного использования зоны
// ============================================================================
bool StrictDispatcher::check_reuse_rule(
    const EntrySignal& signal,
    const ZoneState& zone,
    const BacktestParams& bt_params)
{
    int usage_count = zone.get_usage_count(signal.rule_id);

    switch (bt_params.reuse_mode) {
        case ReuseMode::ONCE:
            // Одноразовая зона: если уже использовали — запрет, пока цена не покинет зону
            if (usage_count > 0) {
                if (!zone.price_left_zone) return false;
                // Если цена покинула зону и вернулась — разрешаем ещё один вход
            }
            return true;

        case ReuseMode::LIMITED:
            // Ограниченное число входов
            if (usage_count >= bt_params.reuse_limit) return false;
            return true;

        case ReuseMode::UNLIMITED:
            // Без ограничений — всегда разрешаем
            return true;

        default:
            return true;
    }
}

// ============================================================================
// Главная функция диспетчера
// ============================================================================
DispatchResult StrictDispatcher::dispatch(
    const std::vector<EntrySignal>& signals,
    std::vector<StrictPosition>& current_positions,
    std::vector<ZoneState>& zones,
    int& trade_counter,
    const BacktestParams& bt_params,
    const std::string& symbol,
    const std::string& timeframe,
    double point)
{
    DispatchResult result;

    for (const auto& signal : signals) {
        if (!signal.valid) continue;

        // Проверка 1: лимит одновременных позиций
        if (!check_position_limit(current_positions, bt_params.max_concurrent_positions)) {
            result.rejected_by_limit++;
            result.rejected_signals.push_back(signal);
            continue;
        }

        // Проверка 2: правило повторного использования зоны
        if (signal.zone_id < 0 || signal.zone_id >= static_cast<int>(zones.size())) {
            result.rejected_signals.push_back(signal);
            continue;
        }

        ZoneState& zone = zones[signal.zone_id];
        if (!check_reuse_rule(signal, zone, bt_params)) {
            result.rejected_by_reuse++;
            result.rejected_signals.push_back(signal);
            continue;
        }

        // Расчёт лота по риску
        double sl_distance = std::abs(signal.entry_price - signal.stop_loss_price);
        if (sl_distance <= 0.0) {
            result.rejected_signals.push_back(signal);
            continue;
        }

        LotCalculatorParams lot_params;
        lot_params.deposit = bt_params.deposit;
        lot_params.risk_percent = bt_params.risk_percent;
        lot_params.min_lot = bt_params.min_lot;
        lot_params.lot_step = bt_params.lot_step;
        lot_params.max_lot = bt_params.max_lot;
        lot_params.point = point;
        lot_params.tick_value = 10.0;

        double lot = LotCalculator::calculate_lot(sl_distance, lot_params);

        // Открываем позицию
        trade_counter++;
        StrictPosition sp;
        sp.rule_id = signal.rule_id;
        sp.zone_id = signal.zone_id;
        sp.direction = signal.direction;
        sp.atr_at_entry = signal.atr_at_entry;
        sp.tp_distance = signal.tp_distance;
        sp.entry_price = signal.entry_price;
        sp.entry_bar_idx = signal.signal_bar_idx;
        sp.breakeven_moved = false;

        sp.position.open(
            trade_counter,
            signal.rule_id,
            symbol,
            timeframe,
            signal.direction,
            signal.signal_time,
            signal.entry_price,
            signal.signal_bar_idx,
            signal.stop_loss_price,
            signal.take_profit_price,
            lot,
            point,
            bt_params.commission_per_lot
        );

        // Помечаем зону как использованную этим правилом
        zone.increment_usage(signal.rule_id);

        result.opened_positions.push_back(sp);
        current_positions.push_back(sp);
    }

    return result;
}

} // namespace rza