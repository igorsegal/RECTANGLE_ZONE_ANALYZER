// ============================================================================
// csv_writer.cpp — Реализация записи CSV
// ============================================================================

#include "io/csv_writer.h"
#include <iomanip>
#include <sstream>
#include <cstdint>          // ← ИСПРАВЛЕНИЕ: для int64_t

namespace rza {

CsvWriter::CsvWriter()
    : first_field_(true), rows_written_(0) {}

CsvWriter::~CsvWriter() {
    close();
}

bool CsvWriter::open(const std::string& file_path) {
    file_.open(file_path, std::ios::out | std::ios::trunc);
    return file_.is_open();
}

void CsvWriter::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool CsvWriter::is_open() const {
    return file_.is_open();
}

void CsvWriter::write_header(const std::vector<std::string>& columns) {
    begin_row();
    for (const auto& col : columns) {
        add_field(col);
    }
    end_row();
}

void CsvWriter::begin_row() {
    current_line_.clear();
    first_field_ = true;
}

void CsvWriter::add_field(const std::string& value) {
    if (!first_field_) {
        current_line_ += ";";
    }
    current_line_ += escape_csv(value);
    first_field_ = false;
}

void CsvWriter::add_field(int value) {
    add_field(std::to_string(value));
}

void CsvWriter::add_field(int64_t value) {
    add_field(std::to_string(value));
}

void CsvWriter::add_field(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    add_field(oss.str());
}

void CsvWriter::add_field_or_blank(double value, int precision) {
    if (value < 0.0) {
        add_field("");
    } else {
        add_field(value, precision);
    }
}

void CsvWriter::add_field_or_blank(int value) {
    if (value < 0) {
        add_field("");
    } else {
        add_field(value);
    }
}

void CsvWriter::add_field_bool(bool value) {
    add_field(value ? "YES" : "NO");
}

void CsvWriter::end_row() {
    current_line_ += "\r\n";
    file_ << current_line_;
    rows_written_++;
    first_field_ = true;
}

void CsvWriter::write_raw_line(const std::string& line) {
    file_ << line << "\r\n";
    rows_written_++;
}

void CsvWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

int CsvWriter::rows_written() const {
    return rows_written_;
}

std::string CsvWriter::escape_csv(const std::string& value) {
    bool need_quotes = false;
    for (char c : value) {
        if (c == ';' || c == '"' || c == '\r' || c == '\n') {
            need_quotes = true;
            break;
        }
    }
    if (!need_quotes) {
        return value;
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped += '"';
    for (char c : value) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped += c;
        }
    }
    escaped += '"';
    return escaped;
}

} // namespace rza