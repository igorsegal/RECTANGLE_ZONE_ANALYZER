#pragma once
// ============================================================================
// wf_writer.h — Запись результатов Walk-Forward в CSV (Блок 05)
// ============================================================================
// Одна ответственность: запись результатов WF в CSV.
// ============================================================================

#include <string>
#include "discovery/walkforward/wf_evaluator.h"

namespace rza {

class WFWriter {
public:
    // Записать сводку по паттернам
    static bool write_summary(const std::string& file_path,
                               const WFResult& result);

    // Записать детали по fold
    static bool write_folds(const std::string& file_path,
                             const WFResult& result);
};

} // namespace rza