#pragma once
// ============================================================================
// rank_scorer.h — Расчёт финального рейтинга (Блок 04)
// ============================================================================
// Одна ответственность: вычисление финального скоринга и применение
// фильтров отбора к оценённым паттернам.
// ============================================================================

#include <vector>
#include "core/params.h"
#include "discovery/ranking/rank_evaluator.h"

namespace rza {

class RankScorer {
public:
    // Применить скоринг и фильтры ко всем паттернам
    static void score_and_filter(
        std::vector<RankedPattern>& patterns,
        const RankingParams& params);

    // Применить скоринг к одному паттерну
    static void score(RankedPattern& pattern, const RankingParams& params);

    // Проверить, проходит ли паттерн фильтр
    static bool passes_filter(const RankedPattern& pattern,
                              const RankingParams& params,
                              std::string& reason);
};

} // namespace rza