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

// MAE buckets are defined in broker points, relative to the SAME zero
// as the favorable target: the outer edge of the rectangle.
enum class MaeBucket : std::size_t {
    ZERO = 0,
    P1_50,
    P51_100,
    P101_150,
    P151_200,
    P201_300,
    GT300,
    COUNT
};

inline constexpr std::size_t kMaeBucketCount =
    static_cast<std::size_t>(MaeBucket::COUNT);

inline std::size_t mae_bucket_index(double mae_points) {
    if (mae_points <= 1e-9) return static_cast<std::size_t>(MaeBucket::ZERO);
    if (mae_points <= 50.0 + 1e-9) return static_cast<std::size_t>(MaeBucket::P1_50);
    if (mae_points <= 100.0 + 1e-9) return static_cast<std::size_t>(MaeBucket::P51_100);
    if (mae_points <= 150.0 + 1e-9) return static_cast<std::size_t>(MaeBucket::P101_150);
    if (mae_points <= 200.0 + 1e-9) return static_cast<std::size_t>(MaeBucket::P151_200);
    if (mae_points <= 300.0 + 1e-9) return static_cast<std::size_t>(MaeBucket::P201_300);
    return static_cast<std::size_t>(MaeBucket::GT300);
}

struct TargetPathStats {
    std::uint64_t reached = 0;
    std::uint64_t one_bar = 0;
    long double sum_bars_to_target = 0.0L;
    long double sum_mae_points = 0.0L;
    std::array<std::uint64_t, kMaeBucketCount> mae_buckets{};

    void add(double mae_points, std::size_t bars_to_target) {
        ++reached;

        if (bars_to_target == 1) {
            ++one_bar;
        }

        sum_bars_to_target +=
            static_cast<long double>(bars_to_target);

        sum_mae_points +=
            static_cast<long double>(mae_points);

        ++mae_buckets[mae_bucket_index(mae_points)];
    }

    double one_bar_pct() const {
        if (reached == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return 100.0 *
            static_cast<double>(one_bar) /
            static_cast<double>(reached);
    }

    double avg_bars() const {
        if (reached == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return static_cast<double>(
            sum_bars_to_target /
            static_cast<long double>(reached));
    }

    double avg_mae() const {
        if (reached == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return static_cast<double>(
            sum_mae_points /
            static_cast<long double>(reached));
    }

    double mae_bucket_pct(std::size_t i) const {
        if (reached == 0 || i >= mae_buckets.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return 100.0 *
            static_cast<double>(mae_buckets[i]) /
            static_cast<double>(reached);
    }

    void merge(const TargetPathStats& other) {
        reached += other.reached;
        one_bar += other.one_bar;
        sum_bars_to_target += other.sum_bars_to_target;
        sum_mae_points += other.sum_mae_points;

        for (std::size_t i = 0; i < mae_buckets.size(); ++i) {
            mae_buckets[i] += other.mae_buckets[i];
        }
    }
};

struct PathStats {
    std::uint64_t touches = 0;
    std::array<TargetPathStats, kLevelsPoints.size()> target{};

    double reached_pct(std::size_t i) const {
        if (touches == 0 || i >= target.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return 100.0 *
            static_cast<double>(target[i].reached) /
            static_cast<double>(touches);
    }

    void merge(const PathStats& other) {
        touches += other.touches;

        for (std::size_t i = 0; i < target.size(); ++i) {
            target[i].merge(other.target[i]);
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

    std::size_t first_high_ge(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();

        return find_high_ge(
            1, 0, size_, begin, end, threshold);
    }

    std::size_t first_low_le(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();

        return find_low_le(
            1, 0, size_, begin, end, threshold);
    }

    static constexpr std::size_t npos() noexcept {
        return std::numeric_limits<std::size_t>::max();
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

    std::size_t find_high_ge(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (max_high_[node] < threshold) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid =
            nl + (nr - nl) / 2;

        const std::size_t left =
            find_high_ge(
                node * 2,
                nl,
                mid,
                ql,
                qr,
                threshold);

        if (left != npos()) return left;

        return find_high_ge(
            node * 2 + 1,
            mid,
            nr,
            ql,
            qr,
            threshold);
    }

    std::size_t find_low_le(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();
        if (min_low_[node] > threshold) return npos();
        if (nr - nl == 1) return nl;

        const std::size_t mid =
            nl + (nr - nl) / 2;

        const std::size_t left =
            find_low_le(
                node * 2,
                nl,
                mid,
                ql,
                qr,
                threshold);

        if (left != npos()) return left;

        return find_low_le(
            node * 2 + 1,
            mid,
            nr,
            ql,
            qr,
            threshold);
    }
};

inline std::size_t alive_end_exclusive(
    std::size_t zone_break_index,
    std::size_t analysis_end_exclusive)
{
    std::size_t end =
        analysis_end_exclusive;

    // ABS_TRACK deletes the rectangle only AFTER the break bar closes.
    // Therefore that bar still belongs to the life of the rectangle.
    if (zone_break_index != abs_track::CloseIndex::npos() &&
        zone_break_index < end)
    {
        end = zone_break_index + 1;
    }

    return end;
}

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

    const std::size_t begin =
        touch_index + 1;

    const std::size_t end =
        alive_end_exclusive(
            zone_break_index,
            analysis_end_exclusive);

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

inline void measure_target_paths_after_touch(
    const HighLowIndex& index,
    std::size_t touch_index,
    std::size_t zone_break_index,
    std::size_t analysis_end_exclusive,
    Direction direction,
    double zone_low,
    double zone_high,
    double point,
    PathStats& out)
{
    ++out.touches;

    if (!(point > 0.0)) {
        return;
    }

    const std::size_t begin =
        touch_index + 1;

    const std::size_t end =
        alive_end_exclusive(
            zone_break_index,
            analysis_end_exclusive);

    if (begin >= end) {
        return;
    }

    for (std::size_t i = 0;
         i < kLevelsPoints.size();
         ++i)
    {
        const double target_distance =
            static_cast<double>(kLevelsPoints[i]) * point;

        std::size_t hit =
            HighLowIndex::npos();

        if (direction == Direction::BULLISH) {
            hit =
                index.first_high_ge(
                    begin,
                    end,
                    zone_high + target_distance);
        } else {
            hit =
                index.first_low_le(
                    begin,
                    end,
                    zone_low - target_distance);
        }

        if (hit == HighLowIndex::npos()) {
            // Higher targets cannot be hit if this lower target was not hit.
            break;
        }

        // We intentionally EXCLUDE the target-hit bar from MAE.
        // OHLC data cannot prove whether that bar's adverse extreme happened
        // before or after its favorable target extreme. This makes the metric
        // unambiguous: MAE is adverse movement on fully completed bars BEFORE
        // the first bar that reaches the target.
        double mae_points = 0.0;

        if (hit > begin) {
            if (direction == Direction::BULLISH) {
                const double min_low =
                    index.range_min_low(begin, hit);

                if (std::isfinite(min_low)) {
                    mae_points =
                        std::max(
                            0.0,
                            (zone_high - min_low) / point);
                }
            } else {
                const double max_high =
                    index.range_max_high(begin, hit);

                if (std::isfinite(max_high)) {
                    mae_points =
                        std::max(
                            0.0,
                            (max_high - zone_low) / point);
                }
            }
        }

        const std::size_t bars_to_target =
            hit - touch_index;

        out.target[i].add(
            mae_points,
            bars_to_target);
    }
}

} // namespace rza::canonical::reaction_levels
