// ============================================================================
// opt_quality.cpp — Реализация вычисления метрик
// ============================================================================
// ИСПРАВЛЕНИЕ: плавная оценка качества со штрафным коэффициентом
// ============================================================================

#include "optimizer/opt_quality.h"
#include "utils/math_utils.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace rza {

QualityMetrics QualityCalculator::calculate(
    const std::vector<TaskResult>& results)
{
    QualityMetrics metrics;
    metrics.total_symbols = static_cast<int>(results.size());

    if (results.empty()) {
        return metrics;
    }

    double sum_profit = 0.0;
    double sum_pf = 0.0;
    double sum_dd = 0.0;
    int profitable_count = 0;
    int successful_count = 0;

    for (const auto& r : results) {
        if (!r.success) continue;
        
        successful_count++;
        sum_profit += r.net_profit;
        sum_pf += r.profit_factor;
        sum_dd += r.max_drawdown_pct;

        if (r.net_profit > 0.0) {
            profitable_count++;
        }
    }

    metrics.total_net_profit = sum_profit;
    metrics.profitable_symbols = profitable_count;
    metrics.total_symbols = successful_count;

    if (successful_count > 0) {
        metrics.stability_score = 100.0 * profitable_count / successful_count;
        metrics.profit_factor = sum_pf / successful_count;
        metrics.max_drawdown_pct = sum_dd / successful_count;
    }

    // Winrate
    int total_trades = 0;
    int total_winning = 0;
    for (const auto& r : results) {
        if (!r.success) continue;
        total_trades += r.total_trades;
        total_winning += r.winning_trades;
    }
    metrics.total_trades = total_trades;
    
    if (total_trades > 0) {
        metrics.winrate_pct = 100.0 * total_winning / total_trades;
    }

    // Quality score
    metrics.quality_score = calculate_quality_score(metrics);

    return metrics;
}

double QualityCalculator::calculate_quality_score(const QualityMetrics& metrics) {
    // Если нет сделок — качество = 0
    if (metrics.total_trades <= 0) {
        return 0.0;
    }
    
    // ИСПРАВЛЕНИЕ: если чистая прибыль отрицательная — качество = 0
    // (полностью убыточная стратегия)
    if (metrics.total_net_profit <= 0.0) {
        return 0.0;
    }

    // Базовая формула: Q = PF × √(trades) × DD_factor × stability
    double pf = metrics.profit_factor;
    if (pf < 0.1) pf = 0.1;  // защита от отрицательных/нулевых значений
    
    double trades_factor = std::sqrt(static_cast<double>(metrics.total_trades));
    
    // DD factor: минимум 0.1 (даже при огромных просадках)
    double dd_factor = std::max(0.1, 1.0 - metrics.max_drawdown_pct / 100.0);
    
    // Stability: % прибыльных символов
    double stability = 0.0;
    if (metrics.total_symbols > 0) {
        stability = static_cast<double>(metrics.profitable_symbols) / metrics.total_symbols;
    }

    // ИСПРАВЛЕНИЕ: штрафной коэффициент при PF < 1.0
    // Вместо обнуления — уменьшение оценки в 3 раза
    double pf_penalty = 1.0;
    if (metrics.profit_factor < 1.0) {
        pf_penalty = 0.3;  // штраф: оценка уменьшается в 3 раза
    }

    return pf * trades_factor * dd_factor * stability * pf_penalty;
}

} // namespace rza