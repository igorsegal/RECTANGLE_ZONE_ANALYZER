// ============================================================================
// opt_result.cpp — Реализация управления результатами
// ============================================================================

#include "optimizer/opt_result.h"
#include <algorithm>

namespace rza {

std::vector<OptimizationRunResult> OptimizationResultManager::get_top_n(
    const std::vector<OptimizationRunResult>& runs,
    int top_n)
{
    std::vector<OptimizationRunResult> successful;
    for (const auto& run : runs) {
        if (run.success) {
            successful.push_back(run);
        }
    }

    // Сортировка по quality_score (убывание)
    std::sort(successful.begin(), successful.end(),
              [](const OptimizationRunResult& a, const OptimizationRunResult& b) {
                  return a.quality.quality_score > b.quality.quality_score;
              });

    if (static_cast<int>(successful.size()) > top_n) {
        successful.resize(top_n);
    }

    return successful;
}

OptimizationResult OptimizationResultManager::create_result(
    const std::vector<OptimizationRunResult>& runs,
    int top_n)
{
    OptimizationResult result;
    result.success = false;
    result.all_runs = runs;
    result.total_combinations = static_cast<int>(runs.size());

    for (const auto& run : runs) {
        if (run.success) {
            result.successful_runs++;
        } else {
            result.failed_runs++;
        }
    }

    result.top_results = get_top_n(runs, top_n);
    result.success = true;

    return result;
}

} // namespace rza