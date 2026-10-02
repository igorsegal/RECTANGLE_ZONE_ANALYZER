#pragma once
// ============================================================================
// ranker.h — Координатор рейтинга (Блок 04)
// ============================================================================
// Одна ответственность: координирует процесс оценки, скоринга
// и фильтрации паттернов.
// ============================================================================

#include <vector>
#include "core/params.h"
#include "discovery/statistics/stat_accumulator.h"
#include "discovery/ranking/rank_evaluator.h"

namespace rza {

class Ranker {
public:
    // Полный процесс: оценка + скоринг + фильтрация
    static std::vector<RankedPattern> rank(
        const StatAccumulator& acc,
        const RankingParams& params);

    // Получить только прошедшие фильтр паттерны
    static std::vector<RankedPattern> get_passed(
        const std::vector<RankedPattern>& all_patterns);
};

} // namespace rza