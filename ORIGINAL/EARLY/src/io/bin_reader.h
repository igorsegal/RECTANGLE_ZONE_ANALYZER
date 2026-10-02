#pragma once
// ============================================================================
// bin_reader.h — Чтение бинарных файлов истории в формате XFBAR
// ============================================================================
// Одна ответственность: чтение .bin файлов формата XFBAR.
// Контракт (см. contracts.h):
//   - success == true => все поля заполнены корректно
//   - bars.size() == bar_count
//   - bars упорядочены хронологически (от старых к новым)
//   - point > 0 (fallback применён автоматически)
//   - symbol не пустой
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include "core/types.h"

namespace rza {

// ============================================================================
// Результат чтения файла
// ============================================================================
struct BinFileData {
    bool         success = false;
    std::string  error_message;

    // Метаданные из заголовка
    std::string  symbol;
    int32_t      period_seconds = 0;
    int32_t      digits = 0;
    double       point = 0.0;
    int64_t      bar_count = 0;
    int64_t      first_time = 0;
    int64_t      last_time = 0;

    // Бары в хронологическом порядке (от старых к новым)
    std::vector<MqlRates> bars;

    // Путь к файлу
    std::string  file_path;

    // Имя таймфрейма (M5, H1, ...)
    std::string  timeframe_str() const;
};

// ============================================================================
// Класс для чтения бинарных файлов
// ============================================================================
class BinReader {
public:
    // Прочитать файл целиком
    static BinFileData read(const std::string& file_path);

    // Прочитать файл с ограничением на количество баров
    static BinFileData read(const std::string& file_path, int64_t max_bars);

    // Получить список всех .bin файлов в директории (рекурсивно)
    static std::vector<std::string> find_bin_files(const std::string& root_dir);

    // Извлечь символ и ТФ из имени файла
    static bool parse_filename(const std::string& filename,
                               std::string& symbol, std::string& timeframe);

    // Преобразование period_seconds -> строка ТФ
    static std::string timeframe_to_string(int period_seconds);

private:
    static bool validate_header(const FileHeader& header, std::string& error);
};

} // namespace rza