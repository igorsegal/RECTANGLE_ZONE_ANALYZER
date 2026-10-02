#pragma once
// ============================================================================
// fr_writer.h — Запись финальных правил в CSV (Блок 07)
// ============================================================================
// Одна ответственность: запись финальных правил в CSV.
// ============================================================================

#include <string>
#include <vector>
#include "discovery/final_rules/fr_decoder.h"

namespace rza {

class FRWriter {
public:
    // Записать финальные правила в CSV
    static bool write(const std::string& file_path,
                       const std::vector<FinalRule>& rules);
};

} // namespace rza