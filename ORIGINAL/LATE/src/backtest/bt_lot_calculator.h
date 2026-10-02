#pragma once
// ============================================================================
// bt_lot_calculator.h — Расчёт лота по депозиту и риску
// ============================================================================
// Одна ответственность: вычисление размера лота на основе риска.
// ============================================================================

#include <cstdint>

namespace rza {

struct LotCalculatorParams {
    double deposit = 100000.0;
    double risk_percent = 1.0;
    double min_lot = 0.01;
    double lot_step = 0.01;
    double max_lot = 100.0;
    double point = 0.01;
    double tick_value = 1.0;  // стоимость пункта для 1 лота
};

class LotCalculator {
public:
    // Рассчитать лот по стоп-лоссу в пунктах
    static double calculate_lot(
        double stop_loss_distance_price,
        const LotCalculatorParams& params);

    // Рассчитать риск в деньгах для заданного лота и SL
    static double calculate_risk_money(
        double lot,
        double stop_loss_distance_price,
        const LotCalculatorParams& params);

    // Округлить лот до шага
    static double round_lot(double lot, double lot_step, double min_lot, double max_lot);
};

} // namespace rza