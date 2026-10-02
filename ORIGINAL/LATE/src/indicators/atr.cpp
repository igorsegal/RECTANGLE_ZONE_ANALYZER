// ============================================================================
// atr.cpp — Реализация расчёта ATR
// ============================================================================

#include "indicators/atr.h"
#include <cmath>
#include <algorithm>

namespace rza {

std::vector<double> ATRCalculator::calculate(const std::vector<MqlRates>& bars,
                                             int period) {
    const size_t n = bars.size();
    std::vector<double> atr(n, 0.0);

    if (period <= 0 || n < static_cast<size_t>(period)) {
        return atr;
    }

    // Шаг 1: Считаем True Range для каждого бара
    std::vector<double> tr(n, 0.0);
    tr[0] = bars[0].high - bars[0].low;
    if (tr[0] < 0.0) tr[0] = 0.0;

    for (size_t i = 1; i < n; ++i) {
        double hl = bars[i].high - bars[i].low;
        double hc = std::abs(bars[i].high - bars[i-1].close);
        double lc = std::abs(bars[i].low - bars[i-1].close);

        double mx = hl;
        if (hc > mx) mx = hc;
        if (lc > mx) mx = lc;
        tr[i] = mx;
    }

    // Шаг 2: Первое значение ATR — SMA от TR за period баров
    double sum = 0.0;
    for (int i = 0; i < period; ++i) {
        sum += tr[i];
    }
    atr[period - 1] = sum / period;

    // Шаг 3: Wilder smoothing
    const double k = static_cast<double>(period - 1);
    const double inv_period = 1.0 / period;

    for (size_t i = period; i < n; ++i) {
        atr[i] = (atr[i-1] * k + tr[i]) * inv_period;
    }

    return atr;
}

double ATRCalculator::get_atr(const std::vector<double>& atr, size_t index) {
    if (index >= atr.size()) return 0.0;
    return atr[index];
}

double ATRCalculator::get_last_atr(const std::vector<double>& atr) {
    if (atr.empty()) return 0.0;
    return atr.back();
}

} // namespace rza