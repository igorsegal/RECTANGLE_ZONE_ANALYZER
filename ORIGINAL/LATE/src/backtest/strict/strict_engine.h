#pragma once
// ============================================================================
// strict_engine.h — Строгий движок исполнения сделок
// ============================================================================
// Объединяет все 4 блока: Monitor, Filter, Dispatcher, Controller
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "core/params.h"
#include "backtest/bt_types.h"
#include "backtest/strict/strict_types.h"
#include "backtest/strict/strict_monitor.h"
#include "backtest/strict/strict_filter.h"
#include "backtest/strict/strict_dispatcher.h"
#include "backtest/strict/strict_controller.h"
#include "discovery/zones/zone_types.h"
#include "discovery/final_rules/fr_decoder.h"

namespace rza {

// ============================================================================
// Входные данные для строгого бэктеста
// ============================================================================
struct StrictBacktestInput {
    std::vector<MqlRates>        bars;
    std::vector<double>          atr;
    std::vector<ZoneRecord>      zones;          // реальные зоны из Discovery
    std::vector<FinalRule>       rules;          // финальные правила
    std::string                  symbol;
    std::string                  timeframe;
    int                          period_seconds;
    double                       point;
    int                          digits;
    BacktestParams               bt_params;
};

// ============================================================================
// Результат строгого бэктеста
// ============================================================================
struct StrictBacktestResult {
    bool                    success = false;
    std::string             error_message;

    std::vector<Trade>      trades;
    BacktestReport          report;

    // Статистика работы строгой логики
    int                     total_signals_generated = 0;
    int                     signals_passed_filter = 0;
    int                     positions_opened = 0;
    int                     positions_rejected_by_limit = 0;
    int                     positions_rejected_by_reuse = 0;
};

// ============================================================================
// Класс строгого движка
// ============================================================================
class StrictEngine {
public:
    static StrictBacktestResult run(const StrictBacktestInput& input);

private:
    static BacktestReport build_report(
        const std::vector<Trade>& trades,
        const std::string& symbol,
        const std::string& timeframe,
        double initial_deposit);
};

} // namespace rza