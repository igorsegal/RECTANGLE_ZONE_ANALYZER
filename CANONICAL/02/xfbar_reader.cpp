#include "xfbar_reader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace fs = std::filesystem;

namespace rza::canonical {

#pragma pack(push, 1)
struct FileHeader {
    char magic[8];
    int32_t version;
    int32_t record_size;
    int32_t period_seconds;
    int32_t digits;
    double point;
    int64_t bar_count;
    int64_t first_time;
    int64_t last_time;
    int32_t symbol_len;
};

struct XfbarRecord {
    int64_t time;
    double open;
    double high;
    double low;
    double close;
    int64_t tick_volume;
    int32_t spread;
    int64_t real_volume;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 60, "XFBAR header must be 60 bytes");
static_assert(sizeof(XfbarRecord) == 60, "XFBAR record must be 60 bytes");

std::string timeframe_name(int32_t period_seconds) {
    switch (period_seconds) {
        case 60: return "M1";
        case 300: return "M5";
        case 900: return "M15";
        case 1800: return "M30";
        case 3600: return "H1";
        case 14400: return "H4";
        case 86400: return "D1";
        case 604800: return "W1";
        case 2592000: return "MN1";
        default: return "TF" + std::to_string(period_seconds);
    }
}

static bool finite_ohlc(const XfbarRecord& r) {
    return std::isfinite(r.open) &&
           std::isfinite(r.high) &&
           std::isfinite(r.low) &&
           std::isfinite(r.close);
}

XfbarData read_xfbar(const std::string& file_path) {
    XfbarData out;
    out.file_path = file_path;

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        out.error = "cannot_open";
        return out;
    }

    FileHeader h{};
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in) {
        out.error = "cannot_read_header";
        return out;
    }

    if (std::strncmp(h.magic, "XFBAR001", 8) != 0) {
        out.error = "bad_magic";
        return out;
    }
    if (h.version != 1) {
        out.error = "unsupported_version=" + std::to_string(h.version);
        return out;
    }
    if (h.record_size != static_cast<int32_t>(sizeof(XfbarRecord))) {
        out.error = "bad_record_size=" + std::to_string(h.record_size);
        return out;
    }
    if (h.bar_count <= 0) {
        out.error = "nonpositive_bar_count";
        return out;
    }
    if (h.symbol_len <= 0 || h.symbol_len > 64) {
        out.error = "bad_symbol_len=" + std::to_string(h.symbol_len);
        return out;
    }
    if (h.period_seconds <= 0) {
        out.error = "nonpositive_period";
        return out;
    }

    std::error_code ec;
    const auto actual_size = fs::file_size(file_path, ec);
    if (ec) {
        out.error = "cannot_get_file_size";
        return out;
    }

    const std::uintmax_t expected_size =
        static_cast<std::uintmax_t>(sizeof(FileHeader)) +
        static_cast<std::uintmax_t>(h.symbol_len) +
        static_cast<std::uintmax_t>(h.bar_count) *
            static_cast<std::uintmax_t>(sizeof(XfbarRecord));

    if (actual_size != expected_size) {
        out.error = "size_mismatch expected=" + std::to_string(expected_size) +
                    " actual=" + std::to_string(actual_size);
        return out;
    }

    out.symbol.resize(static_cast<std::size_t>(h.symbol_len));
    in.read(out.symbol.data(), h.symbol_len);
    if (!in) {
        out.error = "cannot_read_symbol";
        return out;
    }

    out.period_seconds = h.period_seconds;
    out.digits = h.digits;
    out.point = h.point;
    out.declared_bar_count = h.bar_count;
    out.first_time = h.first_time;
    out.last_time = h.last_time;

    std::vector<XfbarRecord> raw(static_cast<std::size_t>(h.bar_count));
    in.read(
        reinterpret_cast<char*>(raw.data()),
        static_cast<std::streamsize>(
            raw.size() * sizeof(XfbarRecord)));

    if (!in) {
        out.error = "cannot_read_records";
        return out;
    }

    out.bars.reserve(raw.size());

    int64_t previous_time = std::numeric_limits<int64_t>::min();

    for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto& r = raw[i];

        if (!finite_ohlc(r)) {
            out.error = "nonfinite_ohlc_at=" + std::to_string(i);
            out.bars.clear();
            return out;
        }

        if (r.low > r.high ||
            r.high < std::max(r.open, r.close) ||
            r.low > std::min(r.open, r.close)) {
            out.error = "bad_ohlc_invariant_at=" + std::to_string(i);
            out.bars.clear();
            return out;
        }

        if (i > 0 && r.time <= previous_time) {
            out.error = "non_increasing_time_at=" + std::to_string(i);
            out.bars.clear();
            return out;
        }

        previous_time = r.time;

        Bar b;
        b.time = r.time;
        b.open = r.open;
        b.high = r.high;
        b.low = r.low;
        b.close = r.close;
        out.bars.push_back(b);
    }

    if (!out.bars.empty()) {
        if (h.first_time != 0 && out.bars.front().time != h.first_time) {
            out.error = "first_time_mismatch";
            out.bars.clear();
            return out;
        }
        if (h.last_time != 0 && out.bars.back().time != h.last_time) {
            out.error = "last_time_mismatch";
            out.bars.clear();
            return out;
        }
    }

    out.success = true;
    return out;
}

} // namespace rza::canonical
