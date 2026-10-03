#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;

namespace {

constexpr std::int32_t H1_SECONDS = 3600;
constexpr std::int32_t M5_SECONDS = 300;

constexpr std::array<int, 10> LEVELS{{
    100, 150, 200, 250, 300,
    350, 400, 500, 1000, 2000
}};

struct Seen {
    std::uint64_t fingerprint = 0;
    std::string file;
};

std::string csv_field(const std::string& s) {
    if (s.find_first_of(";\"\r\n") == std::string::npos) {
        return s;
    }

    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

class InProgressGuard {
public:
    InProgressGuard(fs::path path, const std::string& symbol)
        : path_(std::move(path))
    {
        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        out << symbol << '\n';
        out.flush();
    }

    ~InProgressGuard() {
        std::error_code ec;
        fs::remove(path_, ec);
    }

    InProgressGuard(const InProgressGuard&) = delete;
    InProgressGuard& operator=(const InProgressGuard&) = delete;

private:
    fs::path path_;
};

std::vector<std::string> find_bin_files(const fs::path& root) {
    std::vector<std::string> out;
    std::error_code ec;

    for (fs::recursive_directory_iterator it(
             root,
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
            out.push_back(it->path().string());
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

std::uint64_t mix_u64(std::uint64_t h, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        const unsigned char b =
            static_cast<unsigned char>((v >> (i * 8)) & 0xffu);
        h ^= static_cast<std::uint64_t>(b);
        h *= 1099511628211ULL;
    }
    return h;
}

std::uint64_t double_bits(double x) {
    std::uint64_t b = 0;
    static_assert(sizeof(b) == sizeof(x), "double must be 64-bit");
    std::memcpy(&b, &x, sizeof(b));
    return b;
}

std::uint64_t fingerprint(const XfbarData& d) {
    std::uint64_t h = 14695981039346656037ULL;

    for (char ch : d.symbol) {
        const auto c = static_cast<unsigned char>(ch);
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }

    h = mix_u64(h, static_cast<std::uint64_t>(d.period_seconds));
    h = mix_u64(h, static_cast<std::uint64_t>(d.digits));
    h = mix_u64(h, double_bits(d.point));
    h = mix_u64(h, static_cast<std::uint64_t>(d.bars.size()));

    for (const auto& b : d.bars) {
        h = mix_u64(h, static_cast<std::uint64_t>(b.time));
        h = mix_u64(h, double_bits(b.open));
        h = mix_u64(h, double_bits(b.high));
        h = mix_u64(h, double_bits(b.low));
        h = mix_u64(h, double_bits(b.close));
        h = mix_u64(
            h,
            static_cast<std::uint64_t>(
                static_cast<std::int64_t>(b.spread)));
    }

    return h;
}

struct M5LoadResult {
    bool success = false;
    bool conflict = false;
    std::uint64_t identical_duplicates = 0;
    std::string error;
    XfbarData data;
};

M5LoadResult load_m5_strict(
    const std::string& h1_file,
    const std::string& symbol)
{
    M5LoadResult out;
    const fs::path dir = fs::path(h1_file).parent_path();
    std::vector<fs::path> candidates;
    std::error_code ec;

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

        if (it->path().extension() != ".bin") continue;

        std::string s;
        std::int32_t tf = 0;

        if (!ap::inspect_xfbar(it->path(), s, tf)) continue;
        if (s == symbol && tf == M5_SECONDS) {
            candidates.push_back(it->path());
        }
    }

    std::sort(candidates.begin(), candidates.end());

    if (candidates.empty()) {
        out.error = "missing_M5_sibling";
        return out;
    }

    std::uint64_t base_fp = 0;
    std::string base_file;

    for (const auto& candidate : candidates) {
        XfbarData d = read_xfbar(candidate.string());

        if (!d.success ||
            d.period_seconds != M5_SECONDS ||
            d.symbol != symbol)
        {
            out.error =
                "invalid_M5_candidate=" + candidate.string() +
                " reason=" + d.error;
            return out;
        }

        const std::uint64_t fp = fingerprint(d);

        if (!out.success) {
            out.success = true;
            out.data = std::move(d);
            base_fp = fp;
            base_file = candidate.string();
            continue;
        }

        if (fp == base_fp) {
            ++out.identical_duplicates;
            continue;
        }

        out.success = false;
        out.conflict = true;
        out.error =
            "duplicate_M5_conflict first=" + base_file +
            " second=" + candidate.string();
        return out;
    }

    return out;
}

std::size_t lower_bound_time(
    const std::vector<Bar>& bars,
    std::int64_t t)
{
    const auto it =
        std::lower_bound(
            bars.begin(),
            bars.end(),
            t,
            [](const Bar& b, std::int64_t v)
            {
                return b.time < v;
            });

    return static_cast<std::size_t>(
        std::distance(bars.begin(), it));
}

class M5Index {
public:
    explicit M5Index(const std::vector<Bar>& bars) {
        size_ = 1;
        while (size_ < bars.size()) size_ <<= 1;

        max_high_.assign(
            size_ * 2,
            -std::numeric_limits<double>::infinity());

        min_low_.assign(
            size_ * 2,
            std::numeric_limits<double>::infinity());

        max_close_.assign(
            size_ * 2,
            -std::numeric_limits<double>::infinity());

        min_close_.assign(
            size_ * 2,
            std::numeric_limits<double>::infinity());

        for (std::size_t i = 0; i < bars.size(); ++i) {
            max_high_[size_ + i] = bars[i].high;
            min_low_[size_ + i] = bars[i].low;
            max_close_[size_ + i] = bars[i].close;
            min_close_[size_ + i] = bars[i].close;
        }

        for (std::size_t i = size_; i-- > 1;) {
            max_high_[i] =
                std::max(max_high_[i * 2], max_high_[i * 2 + 1]);

            min_low_[i] =
                std::min(min_low_[i * 2], min_low_[i * 2 + 1]);

            max_close_[i] =
                std::max(max_close_[i * 2], max_close_[i * 2 + 1]);

            min_close_[i] =
                std::min(min_close_[i * 2], min_close_[i * 2 + 1]);
        }
    }

    static constexpr std::size_t npos() noexcept {
        return std::numeric_limits<std::size_t>::max();
    }

    std::size_t first_high_ge(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        return first_match(
            max_high_,
            true,
            begin,
            end,
            threshold);
    }

    std::size_t first_low_le(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        return first_match(
            min_low_,
            false,
            begin,
            end,
            threshold);
    }

    std::size_t first_close_gt(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        return first_match_strict(
            max_close_,
            true,
            begin,
            end,
            threshold);
    }

    std::size_t first_close_lt(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        return first_match_strict(
            min_close_,
            false,
            begin,
            end,
            threshold);
    }

    double range_max_high(
        std::size_t begin,
        std::size_t end) const
    {
        return range_value(
            max_high_,
            true,
            begin,
            end);
    }

    double range_min_low(
        std::size_t begin,
        std::size_t end) const
    {
        return range_value(
            min_low_,
            false,
            begin,
            end);
    }

private:
    std::size_t size_ = 1;
    std::vector<double> max_high_;
    std::vector<double> min_low_;
    std::vector<double> max_close_;
    std::vector<double> min_close_;

    std::size_t first_match(
        const std::vector<double>& tree,
        bool is_max,
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();

        return find_match(
            tree,
            is_max,
            false,
            1,
            0,
            size_,
            begin,
            end,
            threshold);
    }

    std::size_t first_match_strict(
        const std::vector<double>& tree,
        bool is_max,
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return npos();

        return find_match(
            tree,
            is_max,
            true,
            1,
            0,
            size_,
            begin,
            end,
            threshold);
    }

    std::size_t find_match(
        const std::vector<double>& tree,
        bool is_max,
        bool strict,
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return npos();

        if (is_max) {
            if (strict) {
                if (tree[node] <= threshold) return npos();
            } else {
                if (tree[node] < threshold) return npos();
            }
        } else {
            if (strict) {
                if (tree[node] >= threshold) return npos();
            } else {
                if (tree[node] > threshold) return npos();
            }
        }

        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;

        const auto left =
            find_match(
                tree,
                is_max,
                strict,
                node * 2,
                nl,
                mid,
                ql,
                qr,
                threshold);

        if (left != npos()) return left;

        return find_match(
            tree,
            is_max,
            strict,
            node * 2 + 1,
            mid,
            nr,
            ql,
            qr,
            threshold);
    }

    double range_value(
        const std::vector<double>& tree,
        bool is_max,
        std::size_t begin,
        std::size_t end) const
    {
        if (begin >= end) {
            return is_max
                ? -std::numeric_limits<double>::infinity()
                : std::numeric_limits<double>::infinity();
        }

        return query_value(
            tree,
            is_max,
            1,
            0,
            size_,
            begin,
            end);
    }

    double query_value(
        const std::vector<double>& tree,
        bool is_max,
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr) const
    {
        if (nr <= ql || qr <= nl) {
            return is_max
                ? -std::numeric_limits<double>::infinity()
                : std::numeric_limits<double>::infinity();
        }

        if (ql <= nl && nr <= qr) {
            return tree[node];
        }

        const std::size_t mid = nl + (nr - nl) / 2;

        const double a =
            query_value(
                tree,
                is_max,
                node * 2,
                nl,
                mid,
                ql,
                qr);

        const double b =
            query_value(
                tree,
                is_max,
                node * 2 + 1,
                mid,
                nr,
                ql,
                qr);

        return is_max ? std::max(a, b) : std::min(a, b);
    }
};

struct SeriesCoherenceResult {
    bool ok = false;
    std::string reason;
    std::int64_t h1_time = 0;
    std::int64_t m5_time = 0;
    double h1_value = 0.0;
    double m5_value = 0.0;
    double diff_points = 0.0;
    std::uint64_t h1_bars_checked = 0;
};

bool quote_close(double a, double b, double point) {
    return
        std::isfinite(a) &&
        std::isfinite(b) &&
        std::isfinite(point) &&
        point > 0.0 &&
        std::abs(a - b) <= point * (1.0 + 1e-9);
}

SeriesCoherenceResult validate_series_coherence(
    const XfbarData& h1,
    const XfbarData& m5,
    const M5Index& m5_index)
{
    SeriesCoherenceResult r;

    if (!std::isfinite(h1.point) ||
        !std::isfinite(m5.point) ||
        !(h1.point > 0.0) ||
        !(m5.point > 0.0))
    {
        r.reason = "NONPOSITIVE_OR_NONFINITE_POINT";
        return r;
    }

    if (h1.digits != m5.digits) {
        r.reason = "H1_M5_DIGITS_MISMATCH";
        r.h1_value = static_cast<double>(h1.digits);
        r.m5_value = static_cast<double>(m5.digits);
        return r;
    }

    const double point_scale =
        std::max(std::abs(h1.point), std::abs(m5.point));

    if (std::abs(h1.point - m5.point) >
        point_scale * 1e-12)
    {
        r.reason = "H1_M5_POINT_MISMATCH";
        r.h1_value = h1.point;
        r.m5_value = m5.point;
        return r;
    }

    const double point = h1.point;

    for (const auto& hb : h1.bars) {
        ++r.h1_bars_checked;
        r.h1_time = hb.time;

        const std::size_t begin =
            lower_bound_time(m5.bars, hb.time);

        if (begin >= m5.bars.size()) {
            r.reason = "M5_HISTORY_ENDS_BEFORE_H1_BAR";
            r.m5_time = 0;
            return r;
        }

        r.m5_time = m5.bars[begin].time;

        if (m5.bars[begin].time != hb.time) {
            r.reason = "M5_HOUR_ANCHOR_MISSING";
            return r;
        }

        const std::size_t end =
            lower_bound_time(
                m5.bars,
                hb.time + H1_SECONDS);

        if (end <= begin) {
            r.reason = "NO_M5_BARS_INSIDE_H1_INTERVAL";
            return r;
        }

        const double m_open = m5.bars[begin].open;
        const double m_high =
            m5_index.range_max_high(begin, end);
        const double m_low =
            m5_index.range_min_low(begin, end);
        const double m_close = m5.bars[end - 1].close;

        struct Check {
            const char* reason;
            double h;
            double m;
        };

        const std::array<Check, 4> checks{{
            {"H1_M5_OPEN_MISMATCH", hb.open, m_open},
            {"H1_M5_HIGH_MISMATCH", hb.high, m_high},
            {"H1_M5_LOW_MISMATCH", hb.low, m_low},
            {"H1_M5_CLOSE_MISMATCH", hb.close, m_close}
        }};

        for (const auto& c : checks) {
            if (!quote_close(c.h, c.m, point)) {
                r.reason = c.reason;
                r.h1_value = c.h;
                r.m5_value = c.m;
                r.diff_points =
                    std::abs(c.h - c.m) / point;
                return r;
            }
        }
    }

    r.ok = true;
    r.reason = "PASS";
    return r;
}

std::size_t first_zone_touch_after_departure(
    const XfbarData& m5,
    const M5Index& index,
    std::size_t begin,
    std::size_t end,
    Direction direction,
    double zone_low,
    double zone_high)
{
    std::size_t cursor = begin;

    while (cursor < end) {
        std::size_t candidate =
            direction == Direction::BULLISH
                ? index.first_low_le(
                      cursor,
                      end,
                      zone_high)
                : index.first_high_ge(
                      cursor,
                      end,
                      zone_low);

        if (candidate == M5Index::npos()) {
            return M5Index::npos();
        }

        const auto& b =
            m5.bars[candidate];

        if (b.high >= zone_low &&
            b.low <= zone_high)
        {
            return candidate;
        }

        cursor = candidate + 1;
    }

    return M5Index::npos();
}

enum class TargetOutcome {
    HIT_BEFORE_RETURN,
    RETURN_BEFORE_HIT,
    SAME_M5_AMBIGUOUS,
    LIFECYCLE_END_MISS,
    CENSORED_DATA_END
};

const char* outcome_name(TargetOutcome x) {
    switch (x) {
        case TargetOutcome::HIT_BEFORE_RETURN:
            return "HIT_BEFORE_RETURN";
        case TargetOutcome::RETURN_BEFORE_HIT:
            return "RETURN_BEFORE_HIT";
        case TargetOutcome::SAME_M5_AMBIGUOUS:
            return "SAME_M5_AMBIGUOUS";
        case TargetOutcome::LIFECYCLE_END_MISS:
            return "LIFECYCLE_END_MISS";
        case TargetOutcome::CENSORED_DATA_END:
            return "CENSORED_DATA_END";
    }
    return "UNKNOWN";
}

TargetOutcome classify_target_outcome(
    std::size_t hit,
    std::size_t ret,
    bool lifecycle_has_break)
{
    if (hit != M5Index::npos()) {
        if (ret == M5Index::npos() || hit < ret) {
            return TargetOutcome::HIT_BEFORE_RETURN;
        }
        if (hit == ret) {
            return TargetOutcome::SAME_M5_AMBIGUOUS;
        }
        return TargetOutcome::RETURN_BEFORE_HIT;
    }

    if (ret != M5Index::npos()) {
        return TargetOutcome::RETURN_BEFORE_HIT;
    }

    return lifecycle_has_break
        ? TargetOutcome::LIFECYCLE_END_MISS
        : TargetOutcome::CENSORED_DATA_END;
}

struct LevelStats {
    std::uint64_t hit = 0;
    std::uint64_t miss_return = 0;
    std::uint64_t miss_lifecycle = 0;
    std::uint64_t ambiguous = 0;
    std::uint64_t censored = 0;

    void add(TargetOutcome x) {
        switch (x) {
            case TargetOutcome::HIT_BEFORE_RETURN:
                ++hit;
                break;
            case TargetOutcome::RETURN_BEFORE_HIT:
                ++miss_return;
                break;
            case TargetOutcome::SAME_M5_AMBIGUOUS:
                ++ambiguous;
                break;
            case TargetOutcome::LIFECYCLE_END_MISS:
                ++miss_lifecycle;
                break;
            case TargetOutcome::CENSORED_DATA_END:
                ++censored;
                break;
        }
    }

    void merge(const LevelStats& o) {
        hit += o.hit;
        miss_return += o.miss_return;
        miss_lifecycle += o.miss_lifecycle;
        ambiguous += o.ambiguous;
        censored += o.censored;
    }

    std::uint64_t clear() const {
        return
            hit +
            miss_return +
            miss_lifecycle;
    }

    std::uint64_t decided() const {
        return clear() + ambiguous;
    }

    double hit_pct_clear() const {
        return clear() > 0
            ? 100.0 *
                  static_cast<double>(hit) /
                  static_cast<double>(clear())
            : std::numeric_limits<double>::quiet_NaN();
    }

    double hit_pct_lower() const {
        return decided() > 0
            ? 100.0 *
                  static_cast<double>(hit) /
                  static_cast<double>(decided())
            : std::numeric_limits<double>::quiet_NaN();
    }

    double hit_pct_upper() const {
        return decided() > 0
            ? 100.0 *
                  static_cast<double>(hit + ambiguous) /
                  static_cast<double>(decided())
            : std::numeric_limits<double>::quiet_NaN();
    }
};

struct DirectStats {
    std::uint64_t accepted_zones = 0;
    std::uint64_t signal_m5_missing = 0;
    std::uint64_t signal_m5_time_mismatch = 0;
    std::uint64_t signal_price_mismatch = 0;
    std::uint64_t no_direct_departure = 0;
    std::uint64_t direct_paths = 0;
    std::uint64_t returned_to_zone = 0;
    std::uint64_t ended_by_parent_deletion = 0;
    std::uint64_t open_ended_data = 0;

    double sum_max_excursion_points = 0.0;

    std::array<LevelStats, LEVELS.size()> level{};

    void merge(const DirectStats& o) {
        accepted_zones += o.accepted_zones;
        signal_m5_missing += o.signal_m5_missing;
        signal_m5_time_mismatch += o.signal_m5_time_mismatch;
        signal_price_mismatch += o.signal_price_mismatch;
        no_direct_departure += o.no_direct_departure;
        direct_paths += o.direct_paths;
        returned_to_zone += o.returned_to_zone;
        ended_by_parent_deletion += o.ended_by_parent_deletion;
        open_ended_data += o.open_ended_data;
        sum_max_excursion_points += o.sum_max_excursion_points;

        for (std::size_t i = 0; i < LEVELS.size(); ++i) {
            level[i].merge(o.level[i]);
        }
    }

    double avg_max_excursion_points() const {
        return direct_paths > 0
            ? sum_max_excursion_points /
                  static_cast<double>(direct_paths)
            : std::numeric_limits<double>::quiet_NaN();
    }
};

void write_level_header(std::ostream& os) {
    for (int level : LEVELS) {
        os
            << ";L" << level << "_Hit"
            << ";L" << level << "_MissReturn"
            << ";L" << level << "_MissLifecycle"
            << ";L" << level << "_Ambiguous"
            << ";L" << level << "_Censored"
            << ";L" << level << "_HitPctClear"
            << ";L" << level << "_HitPctLower"
            << ";L" << level << "_HitPctUpper";
    }
}

void write_stats_header(std::ostream& os) {
    os
        << "Scope"
        << ";AcceptedZones"
        << ";SignalM5Missing"
        << ";SignalM5TimeMismatch"
        << ";SignalPriceMismatch"
        << ";NoDirectDeparture"
        << ";DirectPaths"
        << ";ReturnedToZone"
        << ";EndedByParentDeletion"
        << ";OpenEndedData"
        << ";AvgMaxDirectExcursionPoints";

    write_level_header(os);
    os << '\n';
}

void write_stats_line(
    std::ostream& os,
    const std::string& scope,
    const DirectStats& s)
{
    os
        << csv_field(scope)
        << ';'
        << s.accepted_zones
        << ';'
        << s.signal_m5_missing
        << ';'
        << s.signal_m5_time_mismatch
        << ';'
        << s.signal_price_mismatch
        << ';'
        << s.no_direct_departure
        << ';'
        << s.direct_paths
        << ';'
        << s.returned_to_zone
        << ';'
        << s.ended_by_parent_deletion
        << ';'
        << s.open_ended_data
        << ';';

    os << std::fixed << std::setprecision(9);

    if (s.direct_paths > 0) {
        os << s.avg_max_excursion_points();
    }

    for (std::size_t i = 0; i < LEVELS.size(); ++i) {
        const auto& x = s.level[i];

        os
            << ';' << x.hit
            << ';' << x.miss_return
            << ';' << x.miss_lifecycle
            << ';' << x.ambiguous
            << ';' << x.censored
            << ';';

        if (x.clear() > 0) os << x.hit_pct_clear();

        os << ';';

        if (x.decided() > 0) os << x.hit_pct_lower();

        os << ';';

        if (x.decided() > 0) os << x.hit_pct_upper();
    }

    os << '\n';
}

int utc_year(std::int64_t epoch) {
    const std::time_t tt =
        static_cast<std::time_t>(epoch);

    std::tm g{};

#ifdef _WIN32
    if (gmtime_s(&g, &tt) != 0) return 0;
#else
    if (gmtime_r(&tt, &g) == nullptr) return 0;
#endif

    return g.tm_year + 1900;
}

int run_selftest() {
    int tests = 0;
    int failed = 0;

    auto check = [&](bool condition, const char* name) {
        ++tests;
        if (condition) {
            std::cout << "[OK]   " << name << '\n';
        } else {
            ++failed;
            std::cout << "[FAIL] " << name << '\n';
        }
    };

    std::vector<Bar> m5bars;
    m5bars.reserve(24);

    for (int i = 0; i < 24; ++i) {
        Bar b;
        b.time = static_cast<std::int64_t>(i) * M5_SECONDS;
        b.open = i < 12 ? 100.0 : 101.0;
        b.high = b.open + 0.2;
        b.low = b.open - 0.2;
        b.close = b.open + 0.1;
        m5bars.push_back(b);
    }

    XfbarData m5;
    m5.success = true;
    m5.symbol = "TEST";
    m5.period_seconds = M5_SECONDS;
    m5.digits = 2;
    m5.point = 0.01;
    m5.bars = m5bars;

    XfbarData h1;
    h1.success = true;
    h1.symbol = "TEST";
    h1.period_seconds = H1_SECONDS;
    h1.digits = 2;
    h1.point = 0.01;

    for (int hour = 0; hour < 2; ++hour) {
        const std::size_t begin = static_cast<std::size_t>(hour) * 12;
        const std::size_t end = begin + 12;
        Bar hb;
        hb.time = static_cast<std::int64_t>(hour) * H1_SECONDS;
        hb.open = m5bars[begin].open;
        hb.high = -std::numeric_limits<double>::infinity();
        hb.low = std::numeric_limits<double>::infinity();
        for (std::size_t i = begin; i < end; ++i) {
            hb.high = std::max(hb.high, m5bars[i].high);
            hb.low = std::min(hb.low, m5bars[i].low);
        }
        hb.close = m5bars[end - 1].close;
        h1.bars.push_back(hb);
    }

    M5Index idx(m5.bars);
    auto ok = validate_series_coherence(h1, m5, idx);
    check(ok.ok, "cross-timeframe OHLC coherence accepts exact aggregate");

    XfbarData late = m5;
    late.bars.erase(late.bars.begin(), late.bars.begin() + 12);
    M5Index late_idx(late.bars);
    auto late_result = validate_series_coherence(h1, late, late_idx);
    check(!late_result.ok &&
          late_result.reason == "M5_HOUR_ANCHOR_MISSING",
          "late M5 history is rejected before research replay");

    XfbarData bad_open = m5;
    bad_open.bars[12].open += 1.0;
    M5Index bad_open_idx(bad_open.bars);
    auto price_result =
        validate_series_coherence(h1, bad_open, bad_open_idx);
    check(!price_result.ok &&
          price_result.reason == "H1_M5_OPEN_MISMATCH",
          "same-time H1/M5 price-scale mismatch is rejected");

    XfbarData bad_point = m5;
    bad_point.point = 0.1;
    M5Index bad_point_idx(bad_point.bars);
    auto point_result =
        validate_series_coherence(h1, bad_point, bad_point_idx);
    check(!point_result.ok &&
          point_result.reason == "H1_M5_POINT_MISMATCH",
          "H1/M5 point metadata mismatch is rejected");

    check(idx.first_high_ge(0, m5.bars.size(), 101.15) == 12,
          "segment index first_high_ge is chronological");
    check(idx.first_close_gt(0, m5.bars.size(), 100.5) == 12,
          "segment index first_close_gt is chronological");

    std::vector<Bar> touch_bars(3);
    touch_bars[0] = Bar{0, 102.0, 103.0, 101.5, 102.5, 0};
    touch_bars[1] = Bar{300, 102.0, 102.2, 100.5, 101.0, 0};
    touch_bars[2] = Bar{600, 100.0, 100.4, 99.5, 100.0, 0};
    XfbarData touch_m5;
    touch_m5.bars = touch_bars;
    M5Index touch_idx(touch_bars);
    check(first_zone_touch_after_departure(
              touch_m5,
              touch_idx,
              1,
              3,
              Direction::BULLISH,
              99.8,
              100.2) == 2,
          "return touch requires actual zone intersection");

    XfbarData bad_high = m5;
    bad_high.bars[3].high += 1.0;
    M5Index bad_high_idx(bad_high.bars);
    const auto high_result =
        validate_series_coherence(h1, bad_high, bad_high_idx);
    check(!high_result.ok &&
          high_result.reason == "H1_M5_HIGH_MISMATCH",
          "H1/M5 hourly high aggregation mismatch is rejected");

    XfbarData bad_digits = m5;
    bad_digits.digits = 3;
    M5Index bad_digits_idx(bad_digits.bars);
    const auto digits_result =
        validate_series_coherence(h1, bad_digits, bad_digits_idx);
    check(!digits_result.ok &&
          digits_result.reason == "H1_M5_DIGITS_MISMATCH",
          "H1/M5 digits metadata mismatch is rejected");

    check(classify_target_outcome(5, 7, false) ==
              TargetOutcome::HIT_BEFORE_RETURN,
          "target before return classification");
    check(classify_target_outcome(7, 7, false) ==
              TargetOutcome::SAME_M5_AMBIGUOUS,
          "same-M5 target/return remains ambiguous");
    check(classify_target_outcome(M5Index::npos(),
                                  M5Index::npos(),
                                  true) ==
              TargetOutcome::LIFECYCLE_END_MISS,
          "lifecycle miss classification");

    LevelStats stats;
    stats.hit = 6;
    stats.miss_return = 4;
    stats.ambiguous = 2;
    check(std::abs(stats.hit_pct_clear() - 60.0) < 1e-12 &&
          std::abs(stats.hit_pct_lower() - 50.0) < 1e-12 &&
          std::abs(stats.hit_pct_upper() - (8.0 / 12.0 * 100.0)) < 1e-12,
          "clear/lower/upper percentage arithmetic");

    XfbarData internal_gap = m5;
    internal_gap.bars.erase(internal_gap.bars.begin() + 12);
    M5Index internal_gap_idx(internal_gap.bars);
    const auto internal_gap_result =
        validate_series_coherence(h1, internal_gap, internal_gap_idx);
    check(!internal_gap_result.ok &&
          internal_gap_result.reason == "M5_HOUR_ANCHOR_MISSING",
          "internal missing H1-hour M5 anchor is rejected");

    XfbarData bad_low = m5;
    bad_low.bars[4].low -= 1.0;
    M5Index bad_low_idx(bad_low.bars);
    const auto low_result =
        validate_series_coherence(h1, bad_low, bad_low_idx);
    check(!low_result.ok &&
          low_result.reason == "H1_M5_LOW_MISMATCH",
          "H1/M5 hourly low aggregation mismatch is rejected");

    XfbarData bad_close = m5;
    bad_close.bars[11].close += 1.0;
    M5Index bad_close_idx(bad_close.bars);
    const auto close_result =
        validate_series_coherence(h1, bad_close, bad_close_idx);
    check(!close_result.ok &&
          close_result.reason == "H1_M5_CLOSE_MISMATCH",
          "H1/M5 hourly close aggregation mismatch is rejected");

    check(classify_target_outcome(9, 7, false) ==
              TargetOutcome::RETURN_BEFORE_HIT,
          "return before target classification");

    check(classify_target_outcome(M5Index::npos(),
                                  M5Index::npos(),
                                  false) ==
              TargetOutcome::CENSORED_DATA_END,
          "open-ended data classification is censored");

    std::vector<Bar> bear_touch_bars(3);
    bear_touch_bars[0] = Bar{0, 98.0, 98.5, 97.0, 97.5, 0};
    bear_touch_bars[1] = Bar{300, 98.0, 99.5, 97.8, 98.8, 0};
    bear_touch_bars[2] = Bar{600, 100.0, 100.4, 99.8, 100.0, 0};
    XfbarData bear_touch_m5;
    bear_touch_m5.bars = bear_touch_bars;
    M5Index bear_touch_idx(bear_touch_bars);
    check(first_zone_touch_after_departure(
              bear_touch_m5,
              bear_touch_idx,
              1,
              3,
              Direction::BEARISH,
              99.8,
              100.2) == 2,
          "bearish return touch requires actual zone intersection");

    check(quote_close(100.00, 100.01, 0.01) &&
          !quote_close(100.00, 100.011, 0.01),
          "one-quote-point coherence tolerance is enforced");

    std::cout
        << "SELFTESTS=" << tests
        << " FAILED=" << failed
        << '\n';

    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--selftest") {
        return run_selftest();
    }
    const fs::path data_root =
        argc >= 2
            ? fs::path(argv[1])
            : fs::path("D:/AHexaTrader/1DataFiles/raw");

    const fs::path out_root =
        argc >= 3
            ? fs::path(argv[2])
            : fs::path("../15out");

    const fs::path in_progress_path =
        out_root / "15_IN_PROGRESS.txt";

    std::error_code ec;

    if (!fs::exists(data_root, ec) ||
        !fs::is_directory(data_root, ec))
    {
        std::cerr
            << "BLOCK15 FAIL - DATA_ROOT_NOT_FOUND\n";
        return 2;
    }

    fs::create_directories(out_root, ec);

    if (ec) {
        std::cerr
            << "BLOCK15 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 3;
    }

    const fs::path events_path =
        out_root / "15_EVENTS.csv";

    const fs::path global_path =
        out_root / "15_GLOBAL.csv";

    const fs::path by_symbol_path =
        out_root / "15_BY_SYMBOL.csv";

    const fs::path by_year_path =
        out_root / "15_BY_YEAR.csv";

    const fs::path skipped_path =
        out_root / "15_SKIPPED.csv";

    const fs::path failures_path =
        out_root / "15_FAILURES.csv";

    const fs::path integrity_path =
        out_root / "15_DATA_INTEGRITY.csv";

    const fs::path summary_path =
        out_root / "15_SUMMARY.txt";

    std::ofstream events(events_path, std::ios::binary);
    std::ofstream global_out(global_path, std::ios::binary);
    std::ofstream by_symbol(by_symbol_path, std::ios::binary);
    std::ofstream by_year(by_year_path, std::ios::binary);
    std::ofstream skipped(skipped_path, std::ios::binary);
    std::ofstream failures(failures_path, std::ios::binary);
    std::ofstream integrity(integrity_path, std::ios::binary);

    if (!events ||
        !global_out ||
        !by_symbol ||
        !by_year ||
        !skipped ||
        !failures ||
        !integrity)
    {
        std::cerr
            << "BLOCK15 FAIL - CANNOT_OPEN_OUTPUTS\n";
        return 4;
    }

    events
        << "Symbol;Direction;SignalTime;"
        << "ZoneLow;ZoneHigh;"
        << "SignalH1Close;SignalH1Open;SignalM5Time;SignalM5Open;"
        << "H1M5OpenDiffPoints;H1M5OpenPriceRatio;"
        << "DepartureTime;ReturnTime;EndReason;"
        << "MaxDirectExcursionPoints";

    for (int level : LEVELS) {
        events << ";L" << level;
    }

    events << '\n';

    write_stats_header(global_out);
    write_stats_header(by_symbol);
    write_stats_header(by_year);

    skipped << "Symbol;Status;Reason\n";
    failures << "File;Reason\n";
    integrity
        << "Symbol;H1Time;M5Time;"
        << "H1Value;M5Value;DiffPoints;ValueRatio;Reason\n";

    const auto files =
        find_bin_files(data_root);

    if (files.empty()) {
        std::cerr
            << "BLOCK15 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    struct TfInventory {
        bool has_h1 = false;
        bool has_m5 = false;
    };

    std::map<std::string, TfInventory> inventory;

    for (const auto& file : files) {
        std::string symbol;
        std::int32_t tf = 0;

        if (!ap::inspect_xfbar(
                fs::path(file),
                symbol,
                tf))
        {
            continue;
        }

        auto& item = inventory[symbol];

        if (tf == H1_SECONDS) item.has_h1 = true;
        if (tf == M5_SECONDS) item.has_m5 = true;
    }

    std::uint64_t skipped_missing_h1 = 0;
    std::uint64_t skipped_missing_m5 = 0;

    for (const auto& kv : inventory) {
        if (!kv.second.has_h1) {
            ++skipped_missing_h1;
            skipped
                << csv_field(kv.first)
                << ";SKIP;missing_H1\n";
        } else if (!kv.second.has_m5) {
            ++skipped_missing_m5;
            skipped
                << csv_field(kv.first)
                << ";SKIP;missing_M5\n";
        }
    }

    std::map<std::string, Seen> seen;

    DirectStats all;
    std::map<int, DirectStats> by_year_stats;

    std::uint64_t h1_files = 0;
    std::uint64_t h1_series_tested = 0;
    std::uint64_t skipped_non_xfbar = 0;
    std::uint64_t skipped_invalid_m5 = 0;
    std::uint64_t skipped_short_h1 = 0;
    std::uint64_t skipped_incoherent_series = 0;
    std::uint64_t duplicate_identical_h1 = 0;
    std::uint64_t duplicate_conflict_h1 = 0;
    std::uint64_t duplicate_identical_m5 = 0;
    std::uint64_t duplicate_conflict_m5 = 0;
    std::uint64_t invalid_xfbar = 0;
    std::uint64_t candidate_formations = 0;
    std::uint64_t rejected_gap = 0;

    std::cout
        << "============================================================\n"
        << "RZA BLOCK 15 FIX2R - DIRECT MOVE AFTER ENGULFING\n"
        << "RESEARCH_SCOPE=ALL_AVAILABLE_HISTORY\n"
        << "SIGNAL=ABS_TRACK_V2_ACCEPTED_H1_RECTANGLE\n"
        << "DIRECT_PHASE=FIRST_EXPECTED_M5_CLOSE_OUTSIDE_ZONE -> FIRST_RETURN_TO_ZONE\n"
        << "LEVELS=100,150,200,250,300,350,400,500,1000,2000\n"
        << "LEVEL_REFERENCE=PARENT_OUTER_EDGE\n"
        << "NO_REACTION_LEG_USED=1\n"
        << "PRECHECK=STRICT_FULL_SERIES_H1_M5_COHERENCE\n"
        << "============================================================\n";

    for (const auto& file : files) {
        XfbarData h1 =
            read_xfbar(file);

        if (!h1.success) {
            if (h1.error == "bad_magic") {
                ++skipped_non_xfbar;
                continue;
            }

            ++invalid_xfbar;

            failures
                << csv_field(file)
                << ';'
                << csv_field(h1.error)
                << '\n';

            continue;
        }

        if (h1.period_seconds != H1_SECONDS) {
            continue;
        }

        ++h1_files;

        const std::string key =
            h1.symbol + "|H1";

        const std::uint64_t fp =
            fingerprint(h1);

        const auto seen_it =
            seen.find(key);

        if (seen_it != seen.end()) {
            if (seen_it->second.fingerprint == fp) {
                ++duplicate_identical_h1;
                continue;
            }

            ++duplicate_conflict_h1;

            failures
                << csv_field(file)
                << ';'
                << "duplicate_H1_conflict_with="
                << csv_field(seen_it->second.file)
                << '\n';

            continue;
        }

        seen.emplace(
            key,
            Seen{fp, file});

        InProgressGuard progress_guard(
            in_progress_path,
            h1.symbol);

        const auto inv_it =
            inventory.find(h1.symbol);

        if (inv_it == inventory.end() ||
            !inv_it->second.has_m5)
        {
            continue;
        }

        const M5LoadResult m5_load =
            load_m5_strict(file, h1.symbol);

        duplicate_identical_m5 +=
            m5_load.identical_duplicates;

        if (m5_load.conflict) {
            ++duplicate_conflict_m5;
            failures
                << csv_field(file)
                << ';'
                << csv_field(m5_load.error)
                << '\n';
            continue;
        }

        if (!m5_load.success) {
            ++skipped_invalid_m5;

            skipped
                << csv_field(h1.symbol)
                << ";SKIP;"
                << csv_field(m5_load.error)
                << '\n';

            continue;
        }

        const XfbarData& m5 = m5_load.data;

        if (h1.bars.size() < 4 ||
            m5.bars.size() < 2)
        {
            ++skipped_short_h1;

            skipped
                << csv_field(h1.symbol)
                << ";SKIP;insufficient_history\n";

            continue;
        }

        const M5Index m5_index(
            m5.bars);

        const SeriesCoherenceResult coherence =
            validate_series_coherence(
                h1,
                m5,
                m5_index);

        if (!coherence.ok) {
            ++skipped_incoherent_series;

            integrity
                << csv_field(h1.symbol)
                << ';'
                << coherence.h1_time
                << ';'
                << coherence.m5_time
                << ';'
                << std::fixed
                << std::setprecision(10)
                << coherence.h1_value
                << ';'
                << coherence.m5_value
                << ';'
                << coherence.diff_points
                << ";;"
                << csv_field(coherence.reason)
                << '\n';

            skipped
                << csv_field(h1.symbol)
                << ";SKIP;FULL_SERIES_H1_M5_INCOHERENT:"
                << csv_field(coherence.reason)
                << '\n';

            continue;
        }

        ap::AtrLookup atr(
            m5.bars,
            ap::params().atr_period);

        if (!atr.available()) {
            ++skipped_invalid_m5;

            skipped
                << csv_field(h1.symbol)
                << ";SKIP;M5_ATR_unavailable\n";

            continue;
        }

        const ap::CloseIndex close_index(
            h1.bars);

        ap::ActiveZones active;
        DirectStats series;

        for (std::size_t ci = 0;
             ci + 1 < h1.bars.size();
             ++ci)
        {
            const auto e_opt =
                detect_at(
                    h1.bars,
                    ci);

            if (!e_opt.has_value()) {
                continue;
            }

            ++candidate_formations;

            FormationEvent e =
                *e_opt;

            const auto z =
                ap::calculate_zone_bounds(
                    h1.bars[e.source_index],
                    e.direction,
                    h1.point);

            e.zone_low = z.low;
            e.zone_high = z.high;

            const std::int64_t signal_time =
                h1.bars[ci + 1].time;

            active.expire(ci);

            const double req_gap =
                ap::required_gap(
                    h1.point,
                    atr.at_decision(
                        signal_time));

            if (!active.can_accept(
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    req_gap))
            {
                ++rejected_gap;
                continue;
            }

            const std::size_t break_idx =
                ap::zone_break_index(
                    close_index,
                    h1.bars,
                    ci,
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    h1.point);

            active.insert(
                e.direction,
                e.zone_low,
                e.zone_high,
                break_idx);

            ++series.accepted_zones;

            const int year =
                utc_year(signal_time);
            DirectStats& year_stats =
                by_year_stats[year];
            ++year_stats.accepted_zones;

            const std::size_t signal_m5 =
                lower_bound_time(
                    m5.bars,
                    signal_time);

            if (signal_m5 >= m5.bars.size()) {
                ++series.signal_m5_missing;
                ++year_stats.signal_m5_missing;
                continue;
            }

            const std::int64_t signal_m5_time =
                m5.bars[signal_m5].time;

            const double signal_h1_open =
                h1.bars[ci + 1].open;

            const double signal_m5_open =
                m5.bars[signal_m5].open;

            const double point_tolerance =
                std::max(h1.point, m5.point);

            const double open_diff_price =
                std::abs(signal_h1_open - signal_m5_open);

            const double open_diff_points =
                point_tolerance > 0.0
                    ? open_diff_price / point_tolerance
                    : std::numeric_limits<double>::infinity();

            double open_price_ratio =
                std::numeric_limits<double>::quiet_NaN();

            if (std::abs(signal_h1_open) > 0.0 &&
                std::abs(signal_m5_open) > 0.0)
            {
                open_price_ratio =
                    std::max(
                        std::abs(signal_h1_open),
                        std::abs(signal_m5_open)) /
                    std::min(
                        std::abs(signal_h1_open),
                        std::abs(signal_m5_open));
            }

            if (signal_m5_time != signal_time) {
                ++series.signal_m5_time_mismatch;
                ++year_stats.signal_m5_time_mismatch;

                integrity
                    << csv_field(h1.symbol)
                    << ';'
                    << signal_time
                    << ';'
                    << signal_m5_time
                    << ';'
                    << std::fixed
                    << std::setprecision(10)
                    << signal_h1_open
                    << ';'
                    << signal_m5_open
                    << ';'
                    << open_diff_points
                    << ';';

                if (std::isfinite(open_price_ratio)) {
                    integrity << open_price_ratio;
                }

                integrity
                    << ";M5_TIME_NOT_EQUAL_H1_SIGNAL_TIME\n";

                continue;
            }

            if (!(point_tolerance > 0.0) ||
                open_diff_price >
                    point_tolerance * (1.0 + 1e-9))
            {
                ++series.signal_price_mismatch;
                ++year_stats.signal_price_mismatch;

                integrity
                    << csv_field(h1.symbol)
                    << ';'
                    << signal_time
                    << ';'
                    << signal_m5_time
                    << ';'
                    << std::fixed
                    << std::setprecision(10)
                    << signal_h1_open
                    << ';'
                    << signal_m5_open
                    << ';'
                    << open_diff_points
                    << ';';

                if (std::isfinite(open_price_ratio)) {
                    integrity << open_price_ratio;
                }

                integrity
                    << ";H1_M5_OPEN_MISMATCH_GT_ONE_QUOTE_POINT\n";

                continue;
            }

            const std::int64_t lifecycle_end_time =
                break_idx != ap::CloseIndex::npos()
                    ? h1.bars[break_idx].time +
                          H1_SECONDS
                    : m5.bars.back().time +
                          M5_SECONDS;

            const std::size_t m5_end =
                std::min(
                    m5.bars.size(),
                    lower_bound_time(
                        m5.bars,
                        lifecycle_end_time));

            if (signal_m5 >= m5_end) {
                ++series.no_direct_departure;
                ++year_stats.no_direct_departure;
                continue;
            }

            const std::size_t departure =
                e.direction == Direction::BULLISH
                    ? m5_index.first_close_gt(
                          signal_m5,
                          m5_end,
                          e.zone_high)
                    : m5_index.first_close_lt(
                          signal_m5,
                          m5_end,
                          e.zone_low);

            if (departure == M5Index::npos()) {
                ++series.no_direct_departure;
                ++year_stats.no_direct_departure;
                continue;
            }

            ++series.direct_paths;
            ++year_stats.direct_paths;

            const std::size_t ret =
                first_zone_touch_after_departure(
                    m5,
                    m5_index,
                    departure + 1,
                    m5_end,
                    e.direction,
                    e.zone_low,
                    e.zone_high);

            const std::size_t direct_end =
                ret != M5Index::npos()
                    ? ret
                    : m5_end;

            if (ret != M5Index::npos()) {
                ++series.returned_to_zone;
                ++year_stats.returned_to_zone;
            } else if (break_idx != ap::CloseIndex::npos()) {
                ++series.ended_by_parent_deletion;
                ++year_stats.ended_by_parent_deletion;
            } else {
                ++series.open_ended_data;
                ++year_stats.open_ended_data;
            }

            double max_excursion_points = 0.0;

            if (direct_end > departure) {
                if (e.direction == Direction::BULLISH) {
                    const double max_high =
                        m5_index.range_max_high(
                            departure,
                            direct_end);

                    max_excursion_points =
                        std::max(
                            0.0,
                            (max_high - e.zone_high) /
                                h1.point);
                } else {
                    const double min_low =
                        m5_index.range_min_low(
                            departure,
                            direct_end);

                    max_excursion_points =
                        std::max(
                            0.0,
                            (e.zone_low - min_low) /
                                h1.point);
                }
            }

            series.sum_max_excursion_points +=
                max_excursion_points;
            year_stats.sum_max_excursion_points +=
                max_excursion_points;

            std::array<TargetOutcome, LEVELS.size()> outcomes{};

            for (std::size_t li = 0;
                 li < LEVELS.size();
                 ++li)
            {
                const double target =
                    e.direction == Direction::BULLISH
                        ? e.zone_high +
                              static_cast<double>(LEVELS[li]) *
                                  h1.point
                        : e.zone_low -
                              static_cast<double>(LEVELS[li]) *
                                  h1.point;

                const std::size_t hit =
                    e.direction == Direction::BULLISH
                        ? m5_index.first_high_ge(
                              departure,
                              m5_end,
                              target)
                        : m5_index.first_low_le(
                              departure,
                              m5_end,
                              target);

                const TargetOutcome outcome =
                    classify_target_outcome(
                        hit,
                        ret,
                        break_idx != ap::CloseIndex::npos());

                outcomes[li] = outcome;
                series.level[li].add(outcome);
                year_stats.level[li].add(outcome);
            }

            const double h1_close =
                h1.bars[ci].close;

            events
                << csv_field(h1.symbol)
                << ';'
                << (e.direction ==
                            Direction::BULLISH
                        ? "BULLISH"
                        : "BEARISH")
                << ';'
                << signal_time
                << ';'
                << std::fixed
                << std::setprecision(10)
                << e.zone_low
                << ';'
                << e.zone_high
                << ';'
                << h1_close
                << ';'
                << signal_h1_open
                << ';'
                << signal_m5_time
                << ';'
                << signal_m5_open
                << ';'
                << open_diff_points
                << ';';

            if (std::isfinite(open_price_ratio)) {
                events << open_price_ratio;
            }

            events
                << ';'
                << m5.bars[departure].time
                << ';';

            if (ret != M5Index::npos()) {
                events << m5.bars[ret].time;
            }

            events
                << ';'
                << (ret != M5Index::npos()
                        ? "RETURN_TO_ZONE"
                        : (break_idx != ap::CloseIndex::npos()
                               ? "PARENT_DELETION"
                               : "DATA_END"))
                << ';'
                << max_excursion_points;

            for (const auto outcome : outcomes) {
                events
                    << ';'
                    << outcome_name(outcome);
            }

            events << '\n';
        }

        ++h1_series_tested;

        all.merge(series);

        write_stats_line(
            by_symbol,
            h1.symbol,
            series);

        std::cout
            << h1.symbol
            << " H1"
            << " accepted="
            << series.accepted_zones
            << " direct="
            << series.direct_paths
            << " return="
            << series.returned_to_zone
            << '\n';
    }

    write_stats_line(
        global_out,
        "ALL_SYMBOLS",
        all);

    for (const auto& kv : by_year_stats) {
        write_stats_line(
            by_year,
            std::to_string(kv.first),
            kv.second);
    }

    events.close();
    global_out.close();
    by_symbol.close();
    by_year.close();
    skipped.close();
    failures.close();
    integrity.close();

    std::ofstream summary(
        summary_path,
        std::ios::binary);

    if (!summary) {
        std::cerr
            << "BLOCK15 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 6;
    }

    summary
        << "RZA BLOCK 15 FIX2R - DIRECT MOVE AFTER ENGULFING\n"
        << "RESEARCH_SCOPE=ALL_AVAILABLE_HISTORY\n"
        << "SIGNAL_TIMEFRAME=H1\n"
        << "PATH_TIMEFRAME=M5\n"
        << "PARENT_POLICY=ABS_TRACK_V2_EXACT\n"
        << "EVENT_POPULATION=ACCEPTED_RECTANGLES_ONLY\n"
        << "DIRECT_PHASE_START=FIRST_M5_CLOSE_OUTSIDE_PARENT_ZONE_IN_SIGNAL_DIRECTION\n"
        << "DIRECT_PHASE_END=FIRST_M5_RETURN_TOUCH_PARENT_ZONE_OR_PARENT_DELETION_OR_DATA_END\n"
        << "LEVEL_REFERENCE=PARENT_OUTER_EDGE\n"
        << "LEVELS_POINTS=100,150,200,250,300,350,400,500,1000,2000\n"
        << "RETURN_BAR_TARGET_ORDER_ASSUMED=0\n"
        << "REACTION_LEG_USED=0\n"
        << "SPREAD_COMMISSION_SLIPPAGE=NOT_APPLICABLE_BEHAVIOR_TEST\n"
        << "H1_FILES_FOUND="
        << h1_files
        << '\n'
        << "H1_SERIES_TESTED="
        << h1_series_tested
        << '\n'
        << "SKIPPED_NON_XFBAR="
        << skipped_non_xfbar
        << '\n'
        << "SKIPPED_MISSING_H1="
        << skipped_missing_h1
        << '\n'
        << "SKIPPED_MISSING_M5="
        << skipped_missing_m5
        << '\n'
        << "SKIPPED_INVALID_M5="
        << skipped_invalid_m5
        << '\n'
        << "SKIPPED_SHORT_H1="
        << skipped_short_h1
        << '\n'
        << "SKIPPED_INCOHERENT_SERIES="
        << skipped_incoherent_series
        << '\n'
        << "RUNTIME_CRASH_RECOVERY=DISABLED_FAIL_CLOSED\n"
        << "DUPLICATE_IDENTICAL_H1="
        << duplicate_identical_h1
        << '\n'
        << "DUPLICATE_CONFLICT_H1="
        << duplicate_conflict_h1
        << '\n'
        << "DUPLICATE_IDENTICAL_M5="
        << duplicate_identical_m5
        << '\n'
        << "DUPLICATE_CONFLICT_M5="
        << duplicate_conflict_m5
        << '\n'
        << "INVALID_XFBAR="
        << invalid_xfbar
        << '\n'
        << "CANDIDATE_FORMATIONS="
        << candidate_formations
        << '\n'
        << "REJECTED_GAP="
        << rejected_gap
        << '\n'
        << "ACCEPTED_ZONES="
        << all.accepted_zones
        << '\n'
        << "SIGNAL_M5_MISSING="
        << all.signal_m5_missing
        << '\n'
        << "SIGNAL_M5_TIME_MISMATCH="
        << all.signal_m5_time_mismatch
        << '\n'
        << "SIGNAL_PRICE_MISMATCH="
        << all.signal_price_mismatch
        << '\n'
        << "NO_DIRECT_DEPARTURE="
        << all.no_direct_departure
        << '\n'
        << "DIRECT_PATHS="
        << all.direct_paths
        << '\n'
        << "RETURNED_TO_ZONE="
        << all.returned_to_zone
        << '\n'
        << "ENDED_BY_PARENT_DELETION="
        << all.ended_by_parent_deletion
        << '\n'
        << "OPEN_ENDED_DATA="
        << all.open_ended_data
        << '\n';

    if (all.direct_paths > 0) {
        summary
            << std::fixed
            << std::setprecision(9)
            << "AVG_MAX_DIRECT_EXCURSION_POINTS="
            << all.avg_max_excursion_points()
            << '\n';
    }

    for (std::size_t i = 0;
         i < LEVELS.size();
         ++i)
    {
        const auto& x =
            all.level[i];

        summary
            << "L"
            << LEVELS[i]
            << "_HIT="
            << x.hit
            << '\n'
            << "L"
            << LEVELS[i]
            << "_MISS_RETURN="
            << x.miss_return
            << '\n'
            << "L"
            << LEVELS[i]
            << "_MISS_LIFECYCLE="
            << x.miss_lifecycle
            << '\n'
            << "L"
            << LEVELS[i]
            << "_AMBIGUOUS="
            << x.ambiguous
            << '\n'
            << "L"
            << LEVELS[i]
            << "_CENSORED="
            << x.censored
            << '\n';

        if (x.clear() > 0) {
            summary
                << std::fixed
                << std::setprecision(9)
                << "L"
                << LEVELS[i]
                << "_HIT_PCT_CLEAR="
                << x.hit_pct_clear()
                << '\n';
        }

        if (x.decided() > 0) {
            summary
                << std::fixed
                << std::setprecision(9)
                << "L"
                << LEVELS[i]
                << "_HIT_PCT_LOWER="
                << x.hit_pct_lower()
                << '\n'
                << "L"
                << LEVELS[i]
                << "_HIT_PCT_UPPER="
                << x.hit_pct_upper()
                << '\n';
        }
    }

    summary
        << "\nCONTRACT:\n"
        << "- The same ABS_TRACK_v2 accepted rectangle population is used so direct departure can be compared with prior return/reaction studies.\n"
        << "- The engulfing is known only after its H1 confirmation bar closes; M5 observation starts at that causal signal time.\n"
        << "- Direct departure is confirmed only when an M5 bar closes beyond the parent rectangle in the engulfing direction.\n"
        << "- Distance levels are measured from the parent outer edge, not from a hypothetical trade entry.\n"
        << "- The direct leg ends at the first later M5 bar whose range intersects the parent rectangle.\n"
        << "- If target and return first occur in the same M5 bar, the event is AMBIGUOUS; intrabar order is never invented.\n"
        << "- If the parent lifecycle ends before target and without a return touch, the target is a lifecycle miss.\n"
        << "- Open-ended final-history cases are censored and excluded from clear hit percentages.\n"
        << "- 1000 and 2000 point levels are added exactly as requested; no optimization or threshold fitting is performed.\n"
        << "- FIX2 performs a STRICT FULL-SERIES H1/M5 preflight BEFORE ATR, gap acceptance, active-zone replay, and path measurement.\n"
        << "- H1 and M5 must have matching symbol metadata, digits and point size.\n"
        << "- Every H1 bar must have an M5 bar at the exact same opening timestamp.\n"
        << "- For every H1 bar, M5 aggregation over that hour must reproduce H1 open/high/low/close within one quote point.\n"
        << "- A series with any coherence failure is excluded in full, preventing unknown active-zone state from contaminating later events.\n"
        << "- Duplicate M5 siblings are accepted only when byte-semantic fingerprints are identical; conflicting M5 duplicates are a data-contract failure.\n"
        << "- The one-point tolerance is tied to quote granularity, not fitted to outcomes.\n"
        << "- Incoherent series are written to 15_DATA_INTEGRITY.csv before research replay.\n"
        << "- BY_YEAR lifecycle counters include all accepted coherent events, including no-departure cases; they are not direct-path-only counters.\n"
        << "- Runtime crash recovery is fail-closed: a crashed symbol is never silently skipped and the block must be rerun only after the cause is understood.\n";

    summary.close();

    if (duplicate_conflict_h1 > 0 ||
        duplicate_conflict_m5 > 0 ||
        invalid_xfbar > 0)
    {
        std::cerr
            << "BLOCK15 FAIL - DATA_CONTRACT_ERROR\n";
        return 7;
    }

    if (h1_series_tested == 0 ||
        all.accepted_zones == 0)
    {
        std::cerr
            << "BLOCK15 FAIL - NO_USABLE_EVENTS\n";
        return 8;
    }

    std::cout
        << "============================================================\n"
        << "BLOCK15 FIX2R PASS\n"
        << "ACCEPTED_ZONES="
        << all.accepted_zones
        << '\n'
        << "DIRECT_PATHS="
        << all.direct_paths
        << '\n'
        << "GLOBAL="
        << global_path.string()
        << '\n'
        << "BY_YEAR="
        << by_year_path.string()
        << '\n'
        << "SUMMARY="
        << summary_path.string()
        << '\n'
        << "============================================================\n";

    return 0;
}
