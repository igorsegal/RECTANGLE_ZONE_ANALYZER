#pragma once
// ============================================================================
// csv_writer.h — Запись CSV файлов
// ============================================================================
// Одна ответственность: запись данных в CSV с разделителем ';'
// и экранированием полей.
// ============================================================================

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>          // ← ИСПРАВЛЕНИЕ: для int64_t

namespace rza {

class CsvWriter {
public:
    CsvWriter();
    ~CsvWriter();

    // Открыть файл для записи
    bool open(const std::string& file_path);

    // Закрыть файл
    void close();

    // Проверка, открыт ли файл
    bool is_open() const;

    // Записать заголовок (список колонок)
    void write_header(const std::vector<std::string>& columns);

    // Начать новую строку
    void begin_row();

    // Добавить поле (string)
    void add_field(const std::string& value);

    // Добавить поле (int)
    void add_field(int value);

    // Добавить поле (int64_t)
    void add_field(int64_t value);

    // Добавить поле (double) с указанием количества знаков
    void add_field(double value, int precision = 6);

    // Добавить поле или пустую строку, если value < 0
    void add_field_or_blank(double value, int precision = 6);

    // Добавить поле или пустую строку, если value < 0
    void add_field_or_blank(int value);

    // Добавить поле "YES"/"NO"
    void add_field_bool(bool value);

    // Завершить строку (добавить \r\n)
    void end_row();

    // Записать готовую строку (без экранирования)
    void write_raw_line(const std::string& line);

    // Сбросить буфер
    void flush();

    // Получить количество записанных строк
    int rows_written() const;

private:
    std::ofstream file_;
    std::string current_line_;
    bool first_field_;
    int rows_written_;

    // Экранирование значения для CSV
    static std::string escape_csv(const std::string& value);
};

} // namespace rza