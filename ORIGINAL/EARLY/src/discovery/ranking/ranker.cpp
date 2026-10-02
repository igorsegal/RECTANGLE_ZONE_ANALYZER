// ============================================================================
// ranker.cpp — Реализация координатора рейтинга
// ============================================================================

#include "discovery/ranking/ranker.h"
#include "discovery/ranking/rank_scorer.h"
#include <algorithm>

namespace rza {

std::vector<RankedPattern> Ranker::rank(
    const StatAccumulator& acc,
    const RankingParams& params)
{
    // 1. Оценка всех паттернов (Wilson, improvement)
    auto patterns = RankEvaluator::evaluate_all(acc);

    // 2. Скоринг и фильтрация
    RankScorer::score_and_filter(patterns, params);

    // 3. Сортировка по финальному скору (убывание)
    std::sort(patterns.begin(), patterns.end(),
              [](const RankedPattern& a, const RankedPattern& b) {
                  return a.final_score > b.final_score;
              });

    return patterns;
}

std::vector<RankedPattern> Ranker::get_passed(
    const std::vector<RankedPattern>& all_patterns)
{
    std::vector<RankedPattern> passed;
    for (const auto& p : all_patterns) {
        if (p.passes_filter) {
            passed.push_back(p);
        }
    }
    return passed;
}

} // namespace rza