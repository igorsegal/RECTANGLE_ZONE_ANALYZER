#pragma once

#include "01/formation_detector.h"
#include "abs_track_policy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace rza::canonical::reaction_levels {

inline constexpr std::array<int, 8> kLevelsPoints{
    100, 150, 200, 250, 300, 350, 400, 500
};

struct Stats {
    std::uint64_t touches = 0;
    std::array<std::uint64_t, kLevelsPoints.size()> reached{};

    void add(double max_reaction_points) {
        ++touches;

        for (std::size_t i = 0; i < kLevelsPoints.size(); ++i) {
            if (max_reaction_points + 1e-9 >=
                static_cast<double>(kLevelsPoints[i]))
            {
                ++reached[i];
            }
        }
    }

    double pct(std::size_t i) const {
        if (touches == 0 || i >= reached.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return 100.0 *
            static_cast<double>(reached[i]) /
            static_cast<double>(touches);
    }

    void merge(const Stats& other) {
        touches += other.touches;

        for (std::size_t i = 0; i < reached.size(); ++i) {
            reached[i] += other.reached[i];
        }
    }
};

class HighLowIndex {
public:
    explicit HighLowIndex(const std::vector<Bar>& bars) {
        size_ = 1;
        while (size_ < bars.size()) size_ <<= 1;

        max_high_.assign(
            size_ * 2,
            -std::numeric_limits<double>::infinity());

        min_low_.assign(
            size_ * 2,
            std::numeric_limits<double>::infinity());

        for (std::size_t i = 0; i < bars.size(); ++i) {
            max_high_[size_ + i] = bars[i].high;
            min_low_[size_ + i] = bars[i].low;
        }

        for (std::size_t i = size_; i-- > 1;) {
            max_high_[i] = std::max(
                max_high_[i * 2],
                max_high_[i * 2 + 1]);

            min_low_[i] = std::min(
                min_low_[i * 2],
                min_low_[i * 2 + 1]);
        }
    }

    double range_max_high(
        std::size_t begin,
        std::size_t end) const
    {
        if (begin >= end) {
            return -std::numeric_limits<double>::infinity();
        }

        return query_max(1, 0, size_, begin, end);
    }

    double range_min_low(
        std::size_t begin,
        std::size_t end) const
    {
        if (begin >= end) {
            return std::numeric_limits<double>::infinity();
        }

        return query_min(1, 0, size_, begin, end);
    }

private:
    std::size_t size_ = 1;
    std::vector<double> max_high_;
    std::vector<double> min_low_;

    double query_max(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr) const
    {
        if (nr <= ql || qr <= nl) {
            return -std::numeric_limits<double>::infinity();
        }

        if (ql <= nl && nr <= qr) {
            return max_high_[node];
        }

        const std::size_t mid =
            nl + (nr - nl) / 2;

        return std::max(
            query_max(node * 2, nl, mid, ql, qr),
            query_max(node * 2 + 1, mid, nr, ql, qr));
    }

    double query_min(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr) const
    {
        if (nr <= ql || qr <= nl) {
            return std::numeric_limits<double>::infinity();
        }

        if (ql <= nl && nr <= qr) {
            return min_low_[node];
        }

        const std::size_t mid =
            nl + (nr - nl) / 2;

        return std::min(
            query_min(node * 2, nl, mid, ql, qr),
            query_min(node * 2 + 1, mid, nr, ql, qr));
    }
};

inline double max_reaction_points_after_touch(
    const HighLowIndex& index,
    std::size_t touch_index,
    std::size_t zone_break_index,
    std::size_t analysis_end_exclusive,
    Direction direction,
    double zone_low,
    double zone_high,
    double point)
{
    if (!(point > 0.0)) {
        return 0.0;
    }

    // Touch itself is known only at its close, so reaction measurement
    // starts from the NEXT bar. This avoids intrabar ordering ambiguity.
    const std::size_t begin =
        touch_index + 1;

    std::size_t end =
        analysis_end_exclusive;

    // ABS_TRACK zone remains alive through the break bar and is deleted
    // only after that bar CLOSES beyond the deletion threshold.
    if (zone_break_index != abs_track::CloseIndex::npos() &&
        zone_break_index < end)
    {
        end = zone_break_index + 1;
    }

    if (begin >= end) {
        return 0.0;
    }

    if (direction == Direction::BULLISH) {
        const double highest =
            index.range_max_high(begin, end);

        if (!std::isfinite(highest)) {
            return 0.0;
        }

        return std::max(
            0.0,
            (highest - zone_high) / point);
    }

    const double lowest =
        index.range_min_low(begin, end);

    if (!std::isfinite(lowest)) {
        return 0.0;
    }

    return std::max(
        0.0,
        (zone_low - lowest) / point);
}

} // namespace rza::canonical::reaction_levels
