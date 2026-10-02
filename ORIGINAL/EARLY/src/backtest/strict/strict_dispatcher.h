#pragma once
// ============================================================================
// strict_dispatcher.h — Блок 3: Диспетчер позиций
// ============================================================================
// Одна ответственность: принимать сигналы на вход, проверять лимиты и
// правила повторного использования зоны, открывать позиции.
// ============================================================================

#include <vector>
#include <unordered_map>
#include "backtest/strict/strict_types.h"
#include "backtest/bt_position.h"

namespace rza {

// ============================================================================
// Открытая позиция с привязкой к зоне и правилу
// ============================================================================
struct StrictPosition {
    Position     position;
    std::string  rule_id;
    int          zone_id = -1;
    TradeDirection direction = TradeDirection::BUY;
    double       atr_at_entry = 0.0;
    double       tp_distance = 0.0;
    double       entry_price = 0.0;
    int32_t      entry_bar_idx = -1;
    bool         breakeven_moved = false;
};

// ============================================================================
// Результат работы диспетчера
// ============================================================================
struct DispatchResult {
    std::vector<StrictPosition> opened_positions;
    std::vector<EntrySignal>    rejected_signals;
    int                         rejected_by_limit = 0;
    int                         rejected_by_reuse = 0;
};

// ============================================================================
// Класс диспетчера
// ============================================================================
class StrictDispatcher {
public:
    // Обработать список сигналов и открыть позиции
    static DispatchResult dispatch(
        const std::vector<EntrySignal>& signals,
        std::vector<StrictPosition>& current_positions,
        std::vector<ZoneState>& zones,
        int& trade_counter,
        const BacktestParams& bt_params,
        const std::string& symbol,
        const std::string& timeframe,
        double point);

private:
    // Проверить лимит одновременных позиций
    static bool check_position_limit(
        const std::vector<StrictPosition>& current_positions,
        int max_concurrent);

    // Проверить правило повторного использования зоны
    static bool check_reuse_rule(
        const EntrySignal& signal,
        const ZoneState& zone,
        const BacktestParams& bt_params);
};

} // namespace rza