#pragma once

#include "01/formation_detector.h"
#include "02/xfbar_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <vector>

namespace rza::canonical::abs_track {

namespace fs = std::filesystem;

struct Params {
    int atr_timeframe_seconds = 300;       // M5
    int atr_period = 14;
    double min_gap_atr = 0.30;
    double min_gap_points = 20.0;
    double deletion_threshold_points = 10.0;
    double min_zone_height_points = 225.0;
    int historical_spread_points = 30;
};

inline const Params& params() {
    static const Params p{};
    return p;
}

struct ZoneBounds {
    double low = 0.0;
    double high = 0.0;
};

inline ZoneBounds calculate_zone_bounds(
    const Bar& source,
    Direction direction,
    double point)
{
    const auto& p = params();
    const double min_height = p.min_zone_height_points * point;

    ZoneBounds z{};

    if (direction == Direction::BULLISH) {
        const double body_top = std::max(source.open, source.close);
        const double height = body_top - source.low;

        z.low = source.low;
        z.high = (height < min_height) ? source.high : body_top;
    } else {
        const double body_bottom = std::min(source.open, source.close);
        const double height = source.high - body_bottom;

        z.low = (height < min_height) ? source.low : body_bottom;
        z.high = source.high;
    }

    // ABS_TRACK historical replay uses a fixed historical spread floor.
    const double spread_floor =
        static_cast<double>(p.historical_spread_points) * point;

    if ((z.high - z.low) < spread_floor) {
        if (direction == Direction::BULLISH) {
            z.high = z.low + spread_floor;
        } else {
            z.low = z.high - spread_floor;
        }
    }

    return z;
}

inline double zones_gap(
    double first_low,
    double first_high,
    double second_low,
    double second_high)
{
    if (first_low > second_high) return first_low - second_high;
    if (second_low > first_high) return second_low - first_high;
    return 0.0;
}

class AtrLookup {
public:
    AtrLookup() = default;

    explicit AtrLookup(const std::vector<Bar>& bars, int period) {
        build(bars, period);
    }

    void build(const std::vector<Bar>& bars, int period) {
        times_.clear();
        atr_.clear();

        if (bars.empty() || period <= 0) return;

        times_.reserve(bars.size());
        atr_.assign(bars.size(), 0.0);

        std::vector<double> tr(bars.size(), 0.0);

        for (std::size_t i = 0; i < bars.size(); ++i) {
            times_.push_back(bars[i].time);

            if (i == 0) {
                tr[i] = bars[i].high - bars[i].low;
            } else {
                const double a = bars[i].high - bars[i].low;
                const double b = std::abs(bars[i].high - bars[i - 1].close);
                const double c = std::abs(bars[i].low - bars[i - 1].close);
                tr[i] = std::max(a, std::max(b, c));
            }
        }

        if (bars.size() < static_cast<std::size_t>(period)) return;

        double sum = 0.0;
        for (int i = 0; i < period; ++i) {
            sum += tr[static_cast<std::size_t>(i)];
        }

        atr_[static_cast<std::size_t>(period - 1)] =
            sum / static_cast<double>(period);

        for (std::size_t i = static_cast<std::size_t>(period);
             i < bars.size();
             ++i)
        {
            atr_[i] =
                (atr_[i - 1] * static_cast<double>(period - 1) + tr[i]) /
                static_cast<double>(period);
        }
    }

    bool available() const noexcept {
        return !times_.empty();
    }

    double at_decision(std::int64_t decision_time) const {
        if (times_.empty()) return 0.0;

        // MT4 ABS_TRACK:
        // shift = iBarShift(M5, decisionTime, false);
        // shift++;
        // iATR(M5, 14, shift)
        //
        // In ascending time this means: find the M5 bar mapped to the
        // decision time, then use the immediately PREVIOUS M5 bar.
        const auto it =
            std::upper_bound(times_.begin(), times_.end(), decision_time);

        if (it == times_.begin()) return 0.0;

        std::size_t idx =
            static_cast<std::size_t>(std::distance(times_.begin(), it) - 1);

        if (idx == 0) return 0.0;
        --idx;

        return atr_[idx];
    }

private:
    std::vector<std::int64_t> times_;
    std::vector<double> atr_;
};

#pragma pack(push, 1)
struct LiteHeader {
    char magic[8];
    std::int32_t version;
    std::int32_t record_size;
    std::int32_t period_seconds;
    std::int32_t digits;
    double point;
    std::int64_t bar_count;
    std::int64_t first_time;
    std::int64_t last_time;
    std::int32_t symbol_len;
};
#pragma pack(pop)

static_assert(sizeof(LiteHeader) == 60, "XFBAR header must be 60 bytes");

inline bool inspect_xfbar(
    const fs::path& path,
    std::string& symbol,
    std::int32_t& period_seconds)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    LiteHeader h{};
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in) return false;

    if (std::string(h.magic, h.magic + 8) != "XFBAR001") return false;
    if (h.version != 1 || h.record_size != 60) return false;
    if (h.symbol_len <= 0 || h.symbol_len > 64) return false;

    symbol.resize(static_cast<std::size_t>(h.symbol_len));
    in.read(symbol.data(), h.symbol_len);
    if (!in) return false;

    period_seconds = h.period_seconds;
    return true;
}

inline fs::path find_m5_sibling(
    const fs::path& current_file,
    const std::string& symbol)
{
    std::error_code ec;
    const fs::path dir = current_file.parent_path();

    std::vector<fs::path> candidates;

    for (fs::directory_iterator it(
             dir,
             fs::directory_options::skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec))
    {
        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        if (it->path().extension() == ".bin") {
            candidates.push_back(it->path());
        }
    }

    std::sort(candidates.begin(), candidates.end());

    for (const auto& p : candidates) {
        std::string s;
        std::int32_t tf = 0;

        if (!inspect_xfbar(p, s, tf)) continue;

        if (s == symbol && tf == params().atr_timeframe_seconds) {
            return p;
        }
    }

    return {};
}

inline double required_gap(
    double point,
    double atr_at_decision)
{
    const auto& p = params();

    return std::max(
        p.min_gap_points * point,
        p.min_gap_atr * atr_at_decision);
}

class CloseIndex {
public:
    explicit CloseIndex(const std::vector<Bar>& bars) {
        size_ = 1;
        while (size_ < bars.size()) size_ <<= 1;

        mins_.assign(size_ * 2, std::numeric_limits<double>::infinity());
        maxs_.assign(size_ * 2, -std::numeric_limits<double>::infinity());

        for (std::size_t i = 0; i < bars.size(); ++i) {
            mins_[size_ + i] = bars[i].close;
            maxs_[size_ + i] = bars[i].close;
        }

        for (std::size_t i = size_; i-- > 1;) {
            mins_[i] = std::min(mins_[i * 2], mins_[i * 2 + 1]);
            maxs_[i] = std::max(maxs_[i * 2], maxs_[i * 2 + 1]);
        }
    }

    std::size_t first_outside(
        std::size_t begin,
        std::size_t end,
        double low,
        double high) const
    {
        if (begin >= end) return npos();
        return find_outside(1, 0, size_, begin, end, low, high);
    }

    std::size_t first_le(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();
        return find_le(1, 0, size_, begin, end, threshold);
    }

    std::size_t first_ge(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();
        return find_ge(1, 0, size_, begin, end, threshold);
    }

    std::size_t first_lt(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();
        return find_lt(1, 0, size_, begin, end, threshold);
    }

    std::size_t first_gt(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();
        return find_gt(1, 0, size_, begin, end, threshold);
    }

    static constexpr std::size_t npos() noexcept {
        return std::numeric_limits<std::size_t>::max();
    }

private:
    std::size_t size_ = 1;
    std::vector<double> mins_;
    std::vector<double> maxs_;

    std::size_t find_outside(
        std::size_t node, std::size_t nl, std::size_t nr,
        std::size_t ql, std::size_t qr,
        double low, double high) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (maxs_[node] <= high && mins_[node] >= low) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_outside(node * 2, nl, mid, ql, qr, low, high);

        if (left != npos()) return left;

        return find_outside(
            node * 2 + 1, mid, nr, ql, qr, low, high);
    }

    std::size_t find_le(
        std::size_t node, std::size_t nl, std::size_t nr,
        std::size_t ql, std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (mins_[node] > threshold) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_le(node * 2, nl, mid, ql, qr, threshold);

        if (left != npos()) return left;

        return find_le(
            node * 2 + 1, mid, nr, ql, qr, threshold);
    }

    std::size_t find_ge(
        std::size_t node, std::size_t nl, std::size_t nr,
        std::size_t ql, std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (maxs_[node] < threshold) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_ge(node * 2, nl, mid, ql, qr, threshold);

        if (left != npos()) return left;

        return find_ge(
            node * 2 + 1, mid, nr, ql, qr, threshold);
    }

    std::size_t find_lt(
        std::size_t node, std::size_t nl, std::size_t nr,
        std::size_t ql, std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (mins_[node] >= threshold) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_lt(node * 2, nl, mid, ql, qr, threshold);

        if (left != npos()) return left;

        return find_lt(
            node * 2 + 1, mid, nr, ql, qr, threshold);
    }

    std::size_t find_gt(
        std::size_t node, std::size_t nl, std::size_t nr,
        std::size_t ql, std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (maxs_[node] <= threshold) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_gt(node * 2, nl, mid, ql, qr, threshold);

        if (left != npos()) return left;

        return find_gt(
            node * 2 + 1, mid, nr, ql, qr, threshold);
    }
};

inline std::size_t zone_break_index(
    const CloseIndex& index,
    const std::vector<Bar>& bars,
    std::size_t confirmation_index,
    Direction direction,
    double zone_low,
    double zone_high,
    double point)
{
    const std::size_t begin = confirmation_index + 1;
    const std::size_t end = bars.size();
    const double threshold =
        params().deletion_threshold_points * point;

    if (direction == Direction::BULLISH) {
        return index.first_lt(begin, end, zone_low - threshold);
    }

    return index.first_gt(begin, end, zone_high + threshold);
}

class ActiveZones {
public:
    void expire(std::size_t current_index) {
        expire_one(bull_, bull_breaks_, current_index);
        expire_one(bear_, bear_breaks_, current_index);
    }

    bool can_accept(
        Direction direction,
        double low,
        double high,
        double required_gap_price) const
    {
        const auto& zones =
            direction == Direction::BULLISH ? bull_ : bear_;

        if (zones.empty()) return true;

        auto it = zones.lower_bound(low);

        if (it != zones.end()) {
            if (zones_gap(
                    low, high,
                    it->second.low, it->second.high) <
                required_gap_price)
            {
                return false;
            }
        }

        if (it != zones.begin()) {
            auto prev = it;
            --prev;

            if (zones_gap(
                    low, high,
                    prev->second.low, prev->second.high) <
                required_gap_price)
            {
                return false;
            }
        }

        return true;
    }

    void insert(
        Direction direction,
        double low,
        double high,
        std::size_t break_index)
    {
        auto& zones =
            direction == Direction::BULLISH ? bull_ : bear_;
        auto& breaks =
            direction == Direction::BULLISH ? bull_breaks_ : bear_breaks_;

        StoredZone z;
        z.id = next_id_++;
        z.low = low;
        z.high = high;
        z.break_index = break_index;

        zones.emplace(low, z);

        if (break_index != CloseIndex::npos()) {
            breaks.push(BreakItem{break_index, low, z.id});
        }
    }

private:
    struct StoredZone {
        std::uint64_t id = 0;
        double low = 0.0;
        double high = 0.0;
        std::size_t break_index = CloseIndex::npos();
    };

    struct BreakItem {
        std::size_t break_index = CloseIndex::npos();
        double low = 0.0;
        std::uint64_t id = 0;
    };

    struct BreakLater {
        bool operator()(const BreakItem& a, const BreakItem& b) const {
            return a.break_index > b.break_index;
        }
    };

    using ZoneMap = std::map<double, StoredZone>;
    using BreakQueue =
        std::priority_queue<
            BreakItem,
            std::vector<BreakItem>,
            BreakLater>;

    ZoneMap bull_;
    ZoneMap bear_;
    BreakQueue bull_breaks_;
    BreakQueue bear_breaks_;
    std::uint64_t next_id_ = 1;

    static void expire_one(
        ZoneMap& zones,
        BreakQueue& breaks,
        std::size_t current_index)
    {
        while (!breaks.empty() &&
               breaks.top().break_index <= current_index)
        {
            const BreakItem item = breaks.top();
            breaks.pop();

            const auto it = zones.find(item.low);

            if (it != zones.end() && it->second.id == item.id) {
                zones.erase(it);
            }
        }
    }
};

} // namespace rza::canonical::abs_track
