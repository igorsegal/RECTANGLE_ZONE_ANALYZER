#pragma once
// ============================================================================
// opt_writer.h — Запись результатов оптимизации в CSV
// ============================================================================
// Одна ответственность: запись результатов оптимизации в CSV.
// ============================================================================

#include <string>
#include "optimizer/opt_result.h"

namespace rza {

class OptimizationWriter {
public:
    // Записать все результаты
    static bool write_all_results(
        const std::string& file_path,
        const OptimizationResult& result);

    // Записать только топ-N
    static bool write_top_results(
        const std::string& file_path,
        const OptimizationResult& result);
};

} // namespace rza