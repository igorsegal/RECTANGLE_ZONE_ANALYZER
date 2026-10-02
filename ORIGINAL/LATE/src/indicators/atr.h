#pragma once
// ============================================================================
// atr.h — Быстрый расчёт ATR (Wilder smoothing)
// ============================================================================
// Одна ответственность: расчёт индикатора ATR.
// Wilder ATR (как в MT4):
//   ATR[0..period-2] = 0.0 (недостаточно данных)
//   ATR[period-1] = SMA(TR, period)
//   ATR[i] = (ATR[i-1] * (period-1) + TR[i]) / period
//
// True Range:
//   TR[0] = High - Low
//   TR[i] = max(High - Low, |High - PreviousClose|, |Low - PreviousClose|)
// ============================================================================

#include <vector>
#include <cstdint>
#include "core/types.h"

namespace rza {

class ATRCalculator {
public:
    // Рассчитать ATR для массива баров
    // Возвращает массив той же длины, что и bars
    // Первые (period-1) элементов будут 0.0
    static std::vector<double> calculate(const std::vector<MqlRates>& bars,
                                         int period = 14);

    // Получить ATR для конкретного индекса (с проверкой границ)
    static double get_atr(const std::vector<double>& atr, size_t index);

    // Получить последний доступный ATR
    static double get_last_atr(const std::vector<double>& atr);
};

} // namespace rza