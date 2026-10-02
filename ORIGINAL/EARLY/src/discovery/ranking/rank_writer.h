#pragma once
// ============================================================================
// rank_writer.h — Запись результатов рейтинга в CSV (Блок 04)
// ============================================================================
// Одна ответственность: запись оценённых паттернов в CSV.
// ============================================================================

#include <string>
#include <vector>
#include "discovery/ranking/rank_evaluator.h"

namespace rza {

class RankWriter {
public:
    // Записать все оценённые паттерны в CSV
    static bool write(const std::string& file_path,
                      const std::vector<RankedPattern>& patterns);
};

} // namespace rza