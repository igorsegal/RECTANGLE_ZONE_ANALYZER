#pragma once
// ============================================================================
// stat_writer.h — Запись статистики паттернов в CSV (Блок 03)
// ============================================================================
// Одна ответственность: запись накопленной статистики в CSV файл.
// ============================================================================

#include <string>
#include "discovery/statistics/stat_accumulator.h"
#include "io/csv_writer.h"

namespace rza {

class StatWriter {
public:
    // Записать всю статистику в CSV файл
    static bool write(const std::string& file_path,
                      const StatAccumulator& acc);

private:
    // Записать заголовок CSV
    static void write_header(CsvWriter& writer);

    // Записать одну строку для одного паттерна
    static void write_pattern(CsvWriter& writer,
                              const PatternKey& key,
                              const PatternStats& stats);

    // Вспомогательные функции для форматирования
    static std::string pattern_type_str(PatternType type);
    static std::string zone_type_str(ZoneType type);
    static std::string bucket_name(PatternType type, int bucket);
};

} // namespace rza