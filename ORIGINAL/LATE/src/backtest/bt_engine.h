#pragma once
// ============================================================================
// bt_engine.h — Главный движок бэктеста
// ============================================================================

#include <vector>
#include <string>
#include "core/types.h"
#include "core/params.h"
#include "backtest/bt_types.h"
#include "backtest/bt_position.h"
#include "backtest/bt_lot_calculator.h"
#include "backtest/bt_commission.h"
#include "discovery/final_rules/fr_decoder.h"

namespace rza {

struct BacktestInput {
    std::vector<MqlRates>           bars;
    std::vector<double>             atr;
    std::vector<FinalRule>          rules;
    std::string                     symbol;
    std::string                     timeframe;
    int                             period_seconds;
    double                          point;
    int                             digits;

    BacktestParams                  bt_params;
    CommissionParams                commission_params;
};

struct BacktestResult {
    bool                            success = false;
    std::string                     error_message;

    std::vector<BacktestReport>     reports;
    BacktestReport                  combined_report;

    int32_t  total_trades = 0;
    int32_t  total_rules = 0;
    double   total_net_profit = 0.0;
    double   total_commission = 0.0;
};

class BacktestEngine {
public:
    static BacktestResult run(const BacktestInput& input);

private:
    static BacktestReport run_single_rule(
        const BacktestInput& input,
        const FinalRule& rule,
        int& trade_counter);

    static BacktestReport build_combined_report(
        const std::vector<BacktestReport>& reports,
        const std::string& symbol,
        const std::string& timeframe,
        double initial_deposit);
};

} // namespace rza