#pragma once
// ============================================================================
// math_utils.h — Математические утилиты
// ============================================================================
// Одна ответственность: статистические и математические функции.
// ============================================================================

#include <cmath>
#include <algorithm>

namespace rza {

// ============================================================================
// Нижняя 95%-граница Уилсона
// ============================================================================
inline double wilson_lower_95(int successes, int total) {
    if (total <= 0) return -1.0;
    const double z = 1.959963984540054;
    const double n = static_cast<double>(total);
    const double p = static_cast<double>(successes) / n;
    const double z2 = z * z;
    const double center = p + z2 / (2.0 * n);
    const double margin = z * std::sqrt((p * (1.0 - p) + z2 / (4.0 * n)) / n);
    const double denominator = 1.0 + z2 / n;
    const double lower = (center - margin) / denominator;
    return (lower < 0.0 ? 0.0 : lower) * 100.0;
}

// ============================================================================
// Процент
// ============================================================================
inline double percent(int successes, int total) {
    if (total <= 0) return -1.0;
    return 100.0 * static_cast<double>(successes) / static_cast<double>(total);
}

// ============================================================================
// Среднее
// ============================================================================
inline double average(double sum, int count) {
    if (count <= 0) return -1.0;
    return sum / static_cast<double>(count);
}

// ============================================================================
// Безопасное отношение
// ============================================================================
inline double safe_ratio(double num, double den) {
    if (num < 0.0 || den <= 0.0) return -1.0;
    return num / den;
}

// ============================================================================
// Ограничение значения
// ============================================================================
inline double clamp_double(double value, double min_val, double max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// ============================================================================
// Проверка на "валидное" значение (не -1)
// ============================================================================
inline bool is_valid(double value) {
    return value >= 0.0;
}

// ============================================================================
// Безопасное сложение (если оба значения валидны)
// ============================================================================
inline double safe_add(double a, double b) {
    if (a < 0.0 || b < 0.0) return -1.0;
    return a + b;
}

// ============================================================================
// Profit Factor
// ============================================================================
inline double profit_factor(double gross_profit, double gross_loss) {
    if (gross_loss >= 0.0) return -1.0;
    if (gross_profit <= 0.0) return 0.0;
    return gross_profit / (-gross_loss);
}

// ============================================================================
// Winrate
// ============================================================================
inline double winrate(int wins, int total) {
    if (total <= 0) return -1.0;
    return 100.0 * static_cast<double>(wins) / static_cast<double>(total);
}

} // namespace rza