// ============================================================================
// bt_commission.cpp — Реализация расчёта комиссий
// ============================================================================

#include "backtest/bt_commission.h"
#include <cmath>

namespace rza {

double CommissionCalculator::calculate_commission(
    double lot,
    const CommissionParams& params)
{
    if (lot <= 0.0) return 0.0;
    // Комиссия за открытие + закрытие = 2 × commission_per_lot × lot
    return 2.0 * params.commission_per_lot * lot;
}

double CommissionCalculator::calculate_slippage_money(
    double lot,
    const CommissionParams& params)
{
    if (lot <= 0.0 || params.slippage_points <= 0.0) return 0.0;
    // Проскальзывание при входе и выходе = 2 × slippage_points × point × lot
    return 2.0 * params.slippage_points * params.point * lot;
}

double CommissionCalculator::calculate_spread_money(
    double lot,
    const CommissionParams& params)
{
    if (lot <= 0.0 || params.spread_points <= 0.0) return 0.0;
    return params.spread_points * params.point * lot;
}

double CommissionCalculator::calculate_total_cost(
    double lot,
    const CommissionParams& params)
{
    return calculate_commission(lot, params) +
           calculate_slippage_money(lot, params) +
           calculate_spread_money(lot, params);
}

} // namespace rza