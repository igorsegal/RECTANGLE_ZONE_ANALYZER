// ============================================================================
// bin_reader.cpp — Реализация чтения XFBAR
// ============================================================================

#include "io/bin_reader.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace fs = std::filesystem;

namespace rza {

// ============================================================================
// Преобразование period_seconds -> строка ТФ
// ============================================================================
std::string BinReader::timeframe_to_string(int period_seconds) {
    switch (period_seconds) {
        case 60:     return "M1";
        case 300:    return "M5";
        case 900:    return "M15";
        case 1800:   return "M30";
        case 3600:   return "H1";
        case 14400:  return "H4";
        case 86400:  return "D1";
        case 604800: return "W1";
        case 2592000:return "MN1";
        default:     return "??";
    }
}

std::string BinFileData::timeframe_str() const {
    return BinReader::timeframe_to_string(period_seconds);
}

// ============================================================================
// Валидация заголовка
// ============================================================================
bool BinReader::validate_header(const FileHeader& header, std::string& error) {
    if (std::strncmp(header.magic, "XFBAR001", 8) != 0) {
        error = "Invalid magic: expected 'XFBAR001'";
        return false;
    }
    if (header.version != 1) {
        error = "Unsupported version: " + std::to_string(header.version);
        return false;
    }
    if (header.record_size != 60) {
        error = "Invalid record_size: expected 60, got " +
                std::to_string(header.record_size);
        return false;
    }
    if (header.bar_count <= 0) {
        error = "bar_count must be positive";
        return false;
    }
    if (header.symbol_len <= 0 || header.symbol_len > 64) {
        error = "Invalid symbol_len: " + std::to_string(header.symbol_len);
        return false;
    }
    return true;
}

// ============================================================================
// Чтение файла
// ============================================================================
BinFileData BinReader::read(const std::string& file_path, int64_t max_bars) {
    BinFileData result;
    result.file_path = file_path;

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        result.error_message = "Cannot open file: " + file_path;
        return result;
    }

    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
    if (!file) {
        result.error_message = "Cannot read header";
        return result;
    }

    std::string error;
    if (!validate_header(header, error)) {
        result.error_message = error;
        return result;
    }

    result.symbol.resize(header.symbol_len);
    file.read(&result.symbol[0], header.symbol_len);
    if (!file) {
        result.error_message = "Cannot read symbol";
        return result;
    }

    result.period_seconds = header.period_seconds;
    result.digits = header.digits;
    result.point = header.point;
    result.bar_count = header.bar_count;
    result.first_time = header.first_time;
    result.last_time = header.last_time;

    // Fallback для point, если в файле он равен 0
    if (result.point <= 0.0 && result.digits > 0 && result.digits <= 10) {
        result.point = std::pow(10.0, -static_cast<double>(result.digits));
        if (result.point < 1e-10) {
            result.point = 1e-10;
        }
    }
    if (result.point <= 0.0) {
        result.point = 0.0001;
    }

    int64_t bars_to_read = header.bar_count;
    if (max_bars > 0 && max_bars < bars_to_read) {
        bars_to_read = max_bars;
    }

    result.bars.resize(bars_to_read);
    file.read(reinterpret_cast<char*>(result.bars.data()),
              bars_to_read * sizeof(MqlRates));
    if (!file) {
        result.error_message = "Cannot read bars data";
        return result;
    }

    // В файле XFBAR бары идут от старых к новым (first_time < last_time).
    // Оставляем в хронологическом порядке. std::reverse НЕ нужен.

    result.success = true;
    return result;
}

BinFileData BinReader::read(const std::string& file_path) {
    return read(file_path, 0);
}

// ============================================================================
// Поиск всех .bin файлов в директории
// ============================================================================
std::vector<std::string> BinReader::find_bin_files(const std::string& root_dir) {
    std::vector<std::string> result;
    std::error_code ec;

    if (!fs::exists(root_dir, ec)) {
        return result;
    }

    for (const auto& entry : fs::recursive_directory_iterator(
            root_dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file()) continue;

        const auto& path = entry.path();
        if (path.extension() == ".bin") {
            result.push_back(path.string());
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================================
// Парсинг имени файла
// ============================================================================
bool BinReader::parse_filename(const std::string& filename,
                               std::string& symbol, std::string& timeframe) {
    std::string name = filename;

    if (name.size() > 4 && name.substr(name.size() - 4) == ".bin") {
        name = name.substr(0, name.size() - 4);
    } else {
        return false;
    }

    size_t pos = name.find_last_of('_');
    if (pos == std::string::npos || pos == 0 || pos == name.size() - 1) {
        return false;
    }

    symbol = name.substr(0, pos);
    timeframe = name.substr(pos + 1);
    return true;
}

} // namespace rza