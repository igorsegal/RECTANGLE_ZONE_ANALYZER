#pragma once
// ============================================================================
// bt_commission.h — Расчёт комиссий и спреда
// ============================================================================
// Одна ответственность: вычисление комиссий за сделку.
// ============================================================================

namespace rza {

struct CommissionParams {
    double commission_per_lot = 8.0;   // комиссия за лот (в деньгах)
    double slippage_points = 0.0;      // проскальзывание в пунктах
    double point = 0.01;               // размер пункта
    double spread_points = 0.0;        // спред в пунктах
};

class CommissionCalculator {
public:
    // Комиссия за одну сделку (открытие + закрытие)
    static double calculate_commission(double lot,
                                        const CommissionParams& params);

    // Стоимость проскальзывания
    static double calculate_slippage_money(double lot,
                                            const CommissionParams& params);

    // Стоимость спреда
    static double calculate_spread_money(double lot,
                                          const CommissionParams& params);

    // Общая стоимость входа (комиссия + проскальзывание + спред)
    static double calculate_total_cost(double lot,
                                        const CommissionParams& params);
};

} // namespace rza