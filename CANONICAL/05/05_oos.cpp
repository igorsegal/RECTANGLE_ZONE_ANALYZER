#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;

namespace {

constexpr std::int64_t OOS_START_UTC = 1704067200LL; // 2024-01-01T00:00:00Z
constexpr std::uint64_t SAMPLE_MODULUS = 64;
constexpr std::uint64_t SAMPLE_FOLD = 0;
constexpr std::size_t NPOS = std::numeric_limits<std::size_t>::max();

enum class LifecycleResult {
    NO_OUTSIDE_BEFORE_END,
    BROKEN_BEFORE_EXPECTED_DEPARTURE,
    NO_RETURN_BEFORE_END,
    DIRECT_BREAKOUT_NO_CLOSE_TOUCH,
    REACTION_FIRST,
    BREAKOUT_FIRST,
    TOUCH_UNRESOLVED_AT_END
};

struct Stats {
    std::uint64_t rows = 0;
    std::uint64_t reaction = 0;
    std::uint64_t breakout = 0;
    std::uint64_t other = 0;

    std::uint64_t resolved() const {
        return reaction + breakout;
    }
};

struct Totals {
    std::uint64_t files_found = 0;
    std::uint64_t files_passed = 0;
    std::uint64_t files_failed = 0;
    std::uint64_t skipped_non_xfbar = 0;

    std::uint64_t formations_oos = 0;
    std::uint64_t selected = 0;

    std::uint64_t no_outside = 0;
    std::uint64_t broken_before_departure = 0;
    std::uint64_t no_return = 0;
    std::uint64_t direct_breakout = 0;
    std::uint64_t touch_unresolved = 0;

    Stats overall;
    std::map<std::string, Stats> by_direction;
    std::map<std::string, Stats> by_timeframe;
};

const char* direction_name(Direction d) {
    return d == Direction::BULLISH ? "BULLISH" : "BEARISH";
}

std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

std::string event_key(
    const std::string& relative_file,
    const XfbarData& data,
    const FormationEvent& e)
{
    std::ostringstream oss;
    oss
        << relative_file << '|'
        << data.symbol << '|'
        << data.period_seconds << '|'
        << data.bars[e.source_index].time << '|'
        << data.bars[e.confirmation_index].time << '|'
        << static_cast<int>(e.direction);

    // Formation type and progress are intentionally NOT part of the key.
    return oss.str();
}

double reaction_rate_pct(const Stats& s) {
    if (s.resolved() == 0) return 0.0;
    return 100.0 *
        static_cast<double>(s.reaction) /
        static_cast<double>(s.resolved());
}

std::pair<double,double> wilson95_pct(const Stats& s) {
    const double n = static_cast<double>(s.resolved());
    if (n <= 0.0) return {0.0, 0.0};

    const double p = static_cast<double>(s.reaction) / n;
    const double z = 1.959963984540054;
    const double z2 = z * z;
    const double denom = 1.0 + z2 / n;
    const double center = (p + z2 / (2.0 * n)) / denom;
    const double half =
        z * std::sqrt((p * (1.0 - p) / n) + (z2 / (4.0 * n * n))) / denom;

    return {
        100.0 * std::max(0.0, center - half),
        100.0 * std::min(1.0, center + half)
    };
}

void update_stats(Stats& s, LifecycleResult r) {
    ++s.rows;

    if (r == LifecycleResult::REACTION_FIRST) {
        ++s.reaction;
    } else if (r == LifecycleResult::BREAKOUT_FIRST) {
        ++s.breakout;
    } else {
        ++s.other;
    }
}

void count_lifecycle(Totals& t, LifecycleResult r) {
    switch (r) {
        case LifecycleResult::NO_OUTSIDE_BEFORE_END:
            ++t.no_outside;
            break;
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            ++t.broken_before_departure;
            break;
        case LifecycleResult::NO_RETURN_BEFORE_END:
            ++t.no_return;
            break;
        case LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH:
            ++t.direct_breakout;
            break;
        case LifecycleResult::TOUCH_UNRESOLVED_AT_END:
            ++t.touch_unresolved;
            break;
        case LifecycleResult::REACTION_FIRST:
        case LifecycleResult::BREAKOUT_FIRST:
            break;
    }
}

class RangeIndex {
public:
    explicit RangeIndex(const std::vector<Bar>& bars) {
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
        if (begin >= end) return NPOS;
        return find_outside(1, 0, size_, begin, end, low, high);
    }

    std::size_t first_le(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return NPOS;
        return find_le(1, 0, size_, begin, end, threshold);
    }

    std::size_t first_ge(
        std::size_t begin,
        std::size_t end,
        double threshold) const
    {
        if (begin >= end) return NPOS;
        return find_ge(1, 0, size_, begin, end, threshold);
    }

private:
    std::size_t size_ = 1;
    std::vector<double> mins_;
    std::vector<double> maxs_;

    std::size_t find_outside(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr,
        double low,
        double high) const
    {
        if (nr <= ql || qr <= nl) return NPOS;
        if (maxs_[node] <= high && mins_[node] >= low) return NPOS;
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_outside(node * 2, nl, mid, ql, qr, low, high);
        if (left != NPOS) return left;

        return find_outside(
            node * 2 + 1, mid, nr, ql, qr, low, high);
    }

    std::size_t find_le(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return NPOS;
        if (mins_[node] > threshold) return NPOS;
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_le(node * 2, nl, mid, ql, qr, threshold);
        if (left != NPOS) return left;

        return find_le(
            node * 2 + 1, mid, nr, ql, qr, threshold);
    }

    std::size_t find_ge(
        std::size_t node,
        std::size_t nl,
        std::size_t nr,
        std::size_t ql,
        std::size_t qr,
        double threshold) const
    {
        if (nr <= ql || qr <= nl) return NPOS;
        if (maxs_[node] < threshold) return NPOS;
        if (nr - nl == 1) return nl;

        const std::size_t mid = nl + (nr - nl) / 2;
        const auto left =
            find_ge(node * 2, nl, mid, ql, qr, threshold);
        if (left != NPOS) return left;

        return find_ge(
            node * 2 + 1, mid, nr, ql, qr, threshold);
    }
};

std::vector<std::string> find_bin_files(const fs::path& root) {
    std::vector<std::string> files;
    std::error_code ec;

    for (fs::recursive_directory_iterator it(
             root,
             fs::directory_options::skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        if (it->path().extension() == ".bin") {
            files.push_back(it->path().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

LifecycleResult evaluate_lifecycle(
    const XfbarData& data,
    const RangeIndex& index,
    const FormationEvent& e)
{
    const std::size_t end = data.bars.size();
    const std::size_t search_begin = e.confirmation_index + 1;
    const double low = e.zone_low;
    const double high = e.zone_high;
    const bool bullish = e.direction == Direction::BULLISH;

    const std::size_t first_out =
        index.first_outside(search_begin, end, low, high);

    if (first_out == NPOS) {
        return LifecycleResult::NO_OUTSIDE_BEFORE_END;
    }

    const double first_close = data.bars[first_out].close;
    const bool expected_side =
        bullish ? (first_close > high) : (first_close < low);

    if (!expected_side) {
        return LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE;
    }

    const std::size_t return_idx =
        bullish
            ? index.first_le(first_out + 1, end, high)
            : index.first_ge(first_out + 1, end, low);

    if (return_idx == NPOS) {
        return LifecycleResult::NO_RETURN_BEFORE_END;
    }

    const double return_close = data.bars[return_idx].close;
    const bool inside =
        return_close >= low && return_close <= high;

    if (!inside) {
        return LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH;
    }

    const std::size_t out_after_touch =
        index.first_outside(return_idx + 1, end, low, high);

    if (out_after_touch == NPOS) {
        return LifecycleResult::TOUCH_UNRESOLVED_AT_END;
    }

    const double outcome_close = data.bars[out_after_touch].close;
    const bool reaction_side =
        bullish ? (outcome_close > high) : (outcome_close < low);

    return reaction_side
        ? LifecycleResult::REACTION_FIRST
        : LifecycleResult::BREAKOUT_FIRST;
}

void write_stats_line(
    std::ostream& os,
    const std::string& label,
    const Stats& s)
{
    const auto ci = wilson95_pct(s);

    os
        << label
        << " | rows=" << s.rows
        << " | resolved=" << s.resolved()
        << " | reaction=" << s.reaction
        << " | breakout=" << s.breakout
        << " | other=" << s.other;

    if (s.resolved() > 0) {
        os
            << std::fixed << std::setprecision(6)
            << " | reaction_pct=" << reaction_rate_pct(s)
            << " | CI95=[" << ci.first << ',' << ci.second << ']';
    } else {
        os << " | reaction_pct=NA | CI95=[NA,NA]";
    }

    os << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const fs::path data_root =
        argc >= 2
            ? fs::path(argv[1])
            : fs::path("D:/AHexaTrader/1DataFiles/raw");

    const fs::path out_root =
        argc >= 3
            ? fs::path(argv[2])
            : fs::path("../05out");

    std::error_code ec;

    if (!fs::exists(data_root, ec) || !fs::is_directory(data_root, ec)) {
        std::cerr << "BLOCK05 FAIL - DATA_ROOT_NOT_FOUND\n";
        return 2;
    }

    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr << "BLOCK05 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 3;
    }

    const fs::path summary_path = out_root / "05_OOS_SUMMARY.txt";
    const fs::path failures_path = out_root / "05_FAILURES.csv";

    std::ofstream failures(failures_path, std::ios::binary);
    if (!failures) {
        std::cerr << "BLOCK05 FAIL - CANNOT_OPEN_FAILURES\n";
        return 4;
    }
    failures << "File;Reason\n";

    const auto files = find_bin_files(data_root);
    if (files.empty()) {
        std::cerr << "BLOCK05 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    Totals total;
    total.files_found = files.size();

    std::cout << "============================================================\n";
    std::cout << "RZA CANONICAL BLOCK 05 - FINAL OOS\n";
    std::cout << "OOS_START=2024-01-01T00:00:00Z\n";
    std::cout << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n";
    std::cout << "FORMATION_SPLIT=OFF\n";
    std::cout << "PROGRESS=OFF\n";
    std::cout << "SAMPLE=FNV1A64 %% 64 == 0\n";
    std::cout << "OUTPUT=SUMMARY_ONLY\n";
    std::cout << "============================================================\n";

    for (std::size_t file_idx = 0; file_idx < files.size(); ++file_idx) {
        const auto data = read_xfbar(files[file_idx]);

        if (!data.success) {
            if (data.error == "bad_magic") {
                ++total.skipped_non_xfbar;
            } else {
                ++total.files_failed;
                failures
                    << files[file_idx] << ';'
                    << data.error << '\n';
            }
            continue;
        }

        ++total.files_passed;

        if (data.bars.size() < 3) {
            continue;
        }

        const std::int64_t earliest_confirm_open =
            OOS_START_UTC - static_cast<std::int64_t>(data.period_seconds);

        auto it = std::lower_bound(
            data.bars.begin(),
            data.bars.end(),
            earliest_confirm_open,
            [](const Bar& bar, std::int64_t value) {
                return bar.time < value;
            });

        std::size_t start_idx =
            static_cast<std::size_t>(std::distance(data.bars.begin(), it));

        if (start_idx >= data.bars.size()) {
            continue;
        }

        const RangeIndex index(data.bars);

        std::string relative_file;
        {
            std::error_code rel_ec;
            relative_file =
                fs::relative(files[file_idx], data_root, rel_ec).generic_string();
            if (rel_ec) {
                relative_file =
                    fs::path(files[file_idx]).filename().generic_string();
            }
        }

        for (std::size_t confirm_idx = start_idx;
             confirm_idx < data.bars.size();
             ++confirm_idx)
        {
            const std::int64_t available_at =
                data.bars[confirm_idx].time + data.period_seconds;

            if (available_at < OOS_START_UTC) {
                continue;
            }

            const auto e_opt = detect_at(data.bars, confirm_idx);
            if (!e_opt.has_value()) {
                continue;
            }

            const FormationEvent& e = *e_opt;
            ++total.formations_oos;

            const std::string key = event_key(relative_file, data, e);
            if ((fnv1a64(key) % SAMPLE_MODULUS) != SAMPLE_FOLD) {
                continue;
            }

            ++total.selected;

            const LifecycleResult result =
                evaluate_lifecycle(data, index, e);

            count_lifecycle(total, result);
            update_stats(total.overall, result);
            update_stats(
                total.by_direction[direction_name(e.direction)],
                result);
            update_stats(
                total.by_timeframe[timeframe_name(data.period_seconds)],
                result);
        }

        if ((file_idx + 1) % 25 == 0 || file_idx + 1 == files.size()) {
            std::cout
                << '[' << (file_idx + 1) << '/' << files.size() << "] "
                << data.symbol << '_' << timeframe_name(data.period_seconds)
                << " selected=" << total.selected
                << " resolved=" << total.overall.resolved()
                << " reaction=" << total.overall.reaction
                << " breakout=" << total.overall.breakout
                << " failed=" << total.files_failed
                << "\n";
        }
    }

    failures.close();

    std::ofstream summary(summary_path, std::ios::binary);
    if (!summary) {
        std::cerr << "BLOCK05 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 6;
    }

    const auto overall_ci = wilson95_pct(total.overall);

    summary << "RZA CANONICAL BLOCK 05 - FINAL OOS\n";
    summary << "OOS_START_UTC=2024-01-01T00:00:00Z\n";
    summary << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n";
    summary << "FORMATION_TYPE_USED_FOR_SPLIT=0\n";
    summary << "PROGRESS_USED=0\n";
    summary << "SAMPLE_MODULUS=" << SAMPLE_MODULUS << "\n";
    summary << "SAMPLE_FOLD=" << SAMPLE_FOLD << "\n";
    summary << "FILES_FOUND=" << total.files_found << "\n";
    summary << "FILES_PASSED=" << total.files_passed << "\n";
    summary << "FILES_FAILED=" << total.files_failed << "\n";
    summary << "SKIPPED_NON_XFBAR=" << total.skipped_non_xfbar << "\n";
    summary << "FORMATIONS_OOS=" << total.formations_oos << "\n";
    summary << "SELECTED=" << total.selected << "\n";
    summary << "NO_OUTSIDE_BEFORE_END=" << total.no_outside << "\n";
    summary << "BROKEN_BEFORE_EXPECTED_DEPARTURE="
            << total.broken_before_departure << "\n";
    summary << "NO_RETURN_BEFORE_END=" << total.no_return << "\n";
    summary << "DIRECT_BREAKOUT_NO_CLOSE_TOUCH="
            << total.direct_breakout << "\n";
    summary << "TOUCH_UNRESOLVED_AT_END="
            << total.touch_unresolved << "\n";

    summary << "\n[OVERALL]\n";
    write_stats_line(summary, "ALL", total.overall);

    summary << "\n[DIRECTION]\n";
    for (const auto& kv : total.by_direction) {
        write_stats_line(summary, kv.first, kv.second);
    }

    summary << "\n[TIMEFRAME]\n";
    for (const auto& kv : total.by_timeframe) {
        write_stats_line(summary, kv.first, kv.second);
    }

    const auto bear_it = total.by_direction.find("BEARISH");
    const auto bull_it = total.by_direction.find("BULLISH");

    const bool overall_confirmed =
        total.overall.resolved() > 0 && overall_ci.first > 50.0;

    const bool bearish_confirmed =
        bear_it != total.by_direction.end() &&
        bear_it->second.resolved() > 0 &&
        wilson95_pct(bear_it->second).first > 50.0;

    const bool bullish_confirmed =
        bull_it != total.by_direction.end() &&
        bull_it->second.resolved() > 0 &&
        wilson95_pct(bull_it->second).first > 50.0;

    const bool structural_gate =
        overall_confirmed &&
        bearish_confirmed &&
        bullish_confirmed;

    summary << "\n[FINAL_GATE]\n";
    summary << "GATE_RULE=LOWER_WILSON95_REACTION_PCT_GT_50_OVERALL_AND_BOTH_DIRECTIONS\n";
    summary << "OVERALL_CONFIRMED=" << (overall_confirmed ? 1 : 0) << "\n";
    summary << "BEARISH_CONFIRMED=" << (bearish_confirmed ? 1 : 0) << "\n";
    summary << "BULLISH_CONFIRMED=" << (bullish_confirmed ? 1 : 0) << "\n";
    summary << "STRUCTURAL_HYPOTHESIS_GATE="
            << (structural_gate ? "PASS" : "FAIL") << "\n";

    summary << "\nCONTRACT:\n";
    summary << "- OOS begins at 2024-01-01T00:00:00Z.\n";
    summary << "- Formation type is not used for grouping, filtering or sampling.\n";
    summary << "- Progress is not used.\n";
    summary << "- No event CSV is written; summary-only mode saves disk and I/O.\n";
    summary << "- Touch/outcome semantics are unchanged from Block03.\n";
    summary << "- This is a structural reaction/breakout test, not a trading PnL test.\n";
    summary.close();

    std::cout << "------------------------------------------------------------\n";
    std::cout << "FILES_FOUND=" << total.files_found << "\n";
    std::cout << "FILES_PASSED=" << total.files_passed << "\n";
    std::cout << "FILES_FAILED=" << total.files_failed << "\n";
    std::cout << "SKIPPED_NON_XFBAR=" << total.skipped_non_xfbar << "\n";
    std::cout << "FORMATIONS_OOS=" << total.formations_oos << "\n";
    std::cout << "SELECTED=" << total.selected << "\n";
    std::cout << "RESOLVED=" << total.overall.resolved() << "\n";
    std::cout << "REACTION=" << total.overall.reaction << "\n";
    std::cout << "BREAKOUT=" << total.overall.breakout << "\n";
    std::cout << std::fixed << std::setprecision(6)
              << "REACTION_PCT=" << reaction_rate_pct(total.overall) << "\n";
    std::cout << "STRUCTURAL_HYPOTHESIS_GATE="
              << (structural_gate ? "PASS" : "FAIL") << "\n";
    std::cout << "SUMMARY=" << summary_path.string() << "\n";

    if (total.files_failed != 0) {
        std::cout << "BLOCK05 FAIL - INVALID_XFBAR_FILES="
                  << total.files_failed << "\n";
        return 7;
    }

    if (total.selected == 0) {
        std::cout << "BLOCK05 FAIL - ZERO_SELECTED_OOS_EVENTS\n";
        return 8;
    }

    std::cout << "BLOCK05 PASS\n";
    return 0;
}
