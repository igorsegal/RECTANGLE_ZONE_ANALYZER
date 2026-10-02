#pragma once
// ============================================================================
// strict_controller.h — Блок 4: Контроль удержания позиций
// ============================================================================
// Одна ответственность: на каждом баре проверять условия закрытия для всех
// открытых позиций и управлять защитой (статика/безубыток/трейлинг).
// ============================================================================

#include <vector>
#include "backtest/strict/strict_types.h"
#include "backtest/strict/strict_dispatcher.h"
#include "backtest/bt_types.h"

namespace rza {

// ============================================================================
// Результат работы контроллера
// ============================================================================
struct ControlResult {
    std::vector<Trade>    closed_trades;
    std::vector<int>      closed_position_indexes;  // индексы в массиве current_positions
};

// ============================================================================
// Класс контроллера
// ============================================================================
class StrictController {
public:
    // Обработать все открытые позиции на текущем баре
    static ControlResult update_positions(
        std::vector<StrictPosition>& current_positions,
        std::vector<ZoneState>& zones,
        const MqlRates& bar,
        int bar_idx,
        double atr_value,
        const BacktestParams& bt_params);

private:
    // Проверить условия закрытия для одной позиции
    // Возвращает true, если позиция должна быть закрыта
    static bool check_exit_conditions(
        const StrictPosition& sp,
        const MqlRates& bar,
        int bar_idx,
        const ZoneState& zone,
        ExitReason& reason,
        double& exit_price);

    // Применить управление прибылью (перенос защиты)
    static void apply_profit_management(
        StrictPosition& sp,
        const MqlRates& bar,
        double atr_value,
        const BacktestParams& bt_params);

    // Применить трейлинг по зонам
    static void apply_trailing_by_zones(
        StrictPosition& sp,
        const std::vector<ZoneState>& zones,
        const MqlRates& bar);
};

} // namespace rza