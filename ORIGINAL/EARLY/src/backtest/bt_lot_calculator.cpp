// ============================================================================
// bt_lot_calculator.cpp — Реализация расчёта лота
// ============================================================================
// ИСПРАВЛЕНИЕ: ограничение максимального лота
// ============================================================================

#include "backtest/bt_lot_calculator.h"
#include <cmath>
#include <algorithm>

namespace rza {

// Стандартный contract size для форекса
static constexpr double FOREX_CONTRACT_SIZE = 100000.0;

double LotCalculator::calculate_lot(
    double stop_loss_distance_price,
    const LotCalculatorParams& params)
{
    if (stop_loss_distance_price <= 0.0) {
        return params.min_lot;
    }

    // Риск в деньгах
    double risk_money = params.deposit * params.risk_percent / 100.0;

    // Стоимость пункта для 1 стандартного лота форекса
    // 1 пункт = 0.0001 (для 5-значных котировок) × 100,000 = $10
    double point_value = FOREX_CONTRACT_SIZE * params.point;
    if (point_value <= 0.0) {
        return params.min_lot;
    }

    // SL в пунктах
    double sl_points = stop_loss_distance_price / params.point;
    if (sl_points <= 0.0) {
        return params.min_lot;
    }

    // Лот = риск / (SL_пункты × стоимость_пункта_для_1_лота)
    double lot = risk_money / (sl_points * point_value);

    return round_lot(lot, params.lot_step, params.min_lot, params.max_lot);
}

double LotCalculator::calculate_risk_money(
    double lot,
    double stop_loss_distance_price,
    const LotCalculatorParams& params)
{
    if (lot <= 0.0 || stop_loss_distance_price <= 0.0) {
        return 0.0;
    }

    double sl_points = stop_loss_distance_price / params.point;
    double point_value = FOREX_CONTRACT_SIZE * params.point;

    return lot * sl_points * point_value;
}

double LotCalculator::round_lot(double lot, double lot_step,
                                 double min_lot, double max_lot) {
    if (lot_step <= 0.0) lot_step = 0.01;

    // Округление вниз до шага
    double rounded = std::floor(lot / lot_step) * lot_step;

    // Ограничение диапазона
    if (rounded < min_lot) rounded = min_lot;
    if (rounded > max_lot) rounded = max_lot;

    return rounded;
}

} // namespace rza