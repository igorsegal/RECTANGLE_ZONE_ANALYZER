#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;

namespace {

constexpr std::int64_t DEV_CUTOFF_UTC = 1704067200LL; // 2024-01-01T00:00:00Z
constexpr std::uint64_t SAMPLE_MODULUS = 64;
constexpr std::uint64_t SAMPLE_FOLD = 0;
constexpr std::size_t NPOS = std::numeric_limits<std::size_t>::max();

enum class LifecycleResult {
    NO_OUTSIDE_BEFORE_CUTOFF,
    BROKEN_BEFORE_EXPECTED_DEPARTURE,
    NO_RETURN_BEFORE_CUTOFF,
    DIRECT_BREAKOUT_NO_CLOSE_TOUCH,
    REACTION_FIRST,
    BREAKOUT_FIRST,
    TOUCH_UNRESOLVED_AT_CUTOFF
};

const char* lifecycle_name(LifecycleResult r) {
    switch (r) {
        case LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF:
            return "NO_OUTSIDE_BEFORE_CUTOFF";
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            return "BROKEN_BEFORE_EXPECTED_DEPARTURE";
        case LifecycleResult::NO_RETURN_BEFORE_CUTOFF:
            return "NO_RETURN_BEFORE_CUTOFF";
        case LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH:
            return "DIRECT_BREAKOUT_NO_CLOSE_TOUCH";
        case LifecycleResult::REACTION_FIRST:
            return "REACTION_FIRST";
        case LifecycleResult::BREAKOUT_FIRST:
            return "BREAKOUT_FIRST";
        case LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF:
            return "TOUCH_UNRESOLVED_AT_CUTOFF";
    }
    return "UNKNOWN";
}

const char* formation_name(FormationType t) {
    return t == FormationType::ENGULF_2 ? "ENGULF_2" : "ENGULF_3";
}

const char* direction_name(Direction d) {
    return d == Direction::BULLISH ? "BULLISH" : "BEARISH";
}

int progress_bucket(double p) {
    if (p < 25.0) return 0;
    if (p < 50.0) return 1;
    if (p < 75.0) return 2;
    if (p < 90.0) return 3;
    return 4;
}

const char* progress_bucket_name(int b) {
    static const char* names[5] = {
        "P00_25", "P25_50", "P50_75", "P75_90", "P90_100"
    };
    return names[b];
}

std::string csv_field(const std::string& s) {
    if (s.find_first_of(";\"\r\n") == std::string::npos) {
        return s;
    }
    std::string out = "\"";
    for (char c : s) {
        if (c == '\"') out += "\"\"";
        else out += c;
    }
    out += '\"';
    return out;
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
        const auto left = find_outside(node * 2, nl, mid, ql, qr, low, high);
        if (left != NPOS) return left;
        return find_outside(node * 2 + 1, mid, nr, ql, qr, low, high);
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
        const auto left = find_le(node * 2, nl, mid, ql, qr, threshold);
        if (left != NPOS) return left;
        return find_le(node * 2 + 1, mid, nr, ql, qr, threshold);
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
        const auto left = find_ge(node * 2, nl, mid, ql, qr, threshold);
        if (left != NPOS) return left;
        return find_ge(node * 2 + 1, mid, nr, ql, qr, threshold);
    }
};

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
        << static_cast<int>(e.type) << '|'
        << static_cast<int>(e.direction);
    return oss.str();
}

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

std::size_t development_end_exclusive(const XfbarData& data) {
    const std::int64_t latest_open_allowed =
        DEV_CUTOFF_UTC - static_cast<std::int64_t>(data.period_seconds);

    const auto it = std::lower_bound(
        data.bars.begin(),
        data.bars.end(),
        latest_open_allowed,
        [](const Bar& bar, std::int64_t value) {
            return bar.time < value;
        });

    return static_cast<std::size_t>(std::distance(data.bars.begin(), it));
}

struct Totals {
    std::uint64_t files_found = 0;
    std::uint64_t files_passed = 0;
    std::uint64_t files_failed = 0;
    std::uint64_t skipped_non_xfbar = 0;

    std::uint64_t formations_dev = 0;
    std::uint64_t selected = 0;
    std::uint64_t selected_e2 = 0;
    std::uint64_t selected_e3 = 0;

    std::uint64_t no_outside = 0;
    std::uint64_t broken_before_departure = 0;
    std::uint64_t no_return = 0;
    std::uint64_t direct_breakout = 0;
    std::uint64_t reaction_first = 0;
    std::uint64_t breakout_first = 0;
    std::uint64_t touch_unresolved = 0;
};

void count_result(Totals& t, LifecycleResult r) {
    switch (r) {
        case LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF: ++t.no_outside; break;
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            ++t.broken_before_departure; break;
        case LifecycleResult::NO_RETURN_BEFORE_CUTOFF: ++t.no_return; break;
        case LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH:
            ++t.direct_breakout; break;
        case LifecycleResult::REACTION_FIRST: ++t.reaction_first; break;
        case LifecycleResult::BREAKOUT_FIRST: ++t.breakout_first; break;
        case LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF:
            ++t.touch_unresolved; break;
    }
}

} // namespace

int main(int argc, char** argv) {
    const fs::path data_root =
        argc >= 2 ? fs::path(argv[1]) : fs::path("D:/AHexaTrader/1DataFiles/raw");
    const fs::path out_root =
        argc >= 3 ? fs::path(argv[2]) : fs::path("../03out");

    std::error_code ec;
    if (!fs::exists(data_root, ec) || !fs::is_directory(data_root, ec)) {
        std::cerr << "BLOCK03 FAIL - DATA_ROOT_NOT_FOUND\n";
        return 2;
    }

    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr << "BLOCK03 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 3;
    }

    const fs::path events_path = out_root / "03_REACTION_SCREEN.csv";
    const fs::path file_summary_path = out_root / "03_FILE_SUMMARY.csv";
    const fs::path failures_path = out_root / "03_FAILURES.csv";
    const fs::path summary_path = out_root / "03_SUMMARY.txt";

    std::ofstream events(events_path, std::ios::binary);
    std::ofstream file_summary(file_summary_path, std::ios::binary);
    std::ofstream failures(failures_path, std::ios::binary);

    if (!events || !file_summary || !failures) {
        std::cerr << "BLOCK03 FAIL - CANNOT_OPEN_OUTPUTS\n";
        return 4;
    }

    events
        << "EventKey;ResearchFold;File;Symbol;Timeframe;FormationType;Direction;"
        << "SourceTime;ConfirmBarTime;AvailableAt;ZoneLow;ZoneHigh;"
        << "ProgressPct;ProgressBucket;DepartureTime;TouchTime;OutcomeTime;"
        << "BarsToDeparture;BarsToTouch;BarsTouchToOutcome;LifecycleResult\n";

    file_summary
        << "File;Symbol;Timeframe;DevFormations;SelectedFold0;"
        << "ReactionFirst;BreakoutFirst;OtherLifecycle\n";

    failures << "File;Reason\n";

    std::cout << "============================================================\n";
    std::cout << "RZA CANONICAL BLOCK 03 - CAUSAL REACTION SCREEN\n";
    std::cout << "DEV_CUTOFF=2024-01-01T00:00:00Z\n";
    std::cout << "SAMPLE=FNV1A64 %% 64 == 0\n";
    std::cout << "TOUCH=CLOSE_REENTRY_AFTER_EXPECTED_DEPARTURE\n";
    std::cout << "OUTCOME=FIRST_CLOSE_OUTSIDE_RECTANGLE\n";
    std::cout << "============================================================\n";

    const auto files = find_bin_files(data_root);
    if (files.empty()) {
        std::cerr << "BLOCK03 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    Totals total;
    total.files_found = files.size();

    for (std::size_t file_idx = 0; file_idx < files.size(); ++file_idx) {
        const auto data = read_xfbar(files[file_idx]);

        if (!data.success) {
            if (data.error == "bad_magic") {
                ++total.skipped_non_xfbar;
            } else {
                ++total.files_failed;
                failures
                    << csv_field(files[file_idx]) << ';'
                    << csv_field(data.error) << '\n';
            }
            continue;
        }

        ++total.files_passed;

        const auto dev_end = development_end_exclusive(data);
        if (dev_end < 3) {
            file_summary
                << csv_field(files[file_idx]) << ';'
                << csv_field(data.symbol) << ';'
                << timeframe_name(data.period_seconds)
                << ";0;0;0;0;0\n";
            continue;
        }

        const RangeIndex index(data.bars);
        const auto formations = detect_all(data.bars);

        std::uint64_t one_dev = 0;
        std::uint64_t one_selected = 0;
        std::uint64_t one_reaction = 0;
        std::uint64_t one_breakout = 0;
        std::uint64_t one_other = 0;

        std::string relative_file;
        {
            std::error_code rel_ec;
            relative_file = fs::relative(files[file_idx], data_root, rel_ec).generic_string();
            if (rel_ec) relative_file = fs::path(files[file_idx]).filename().generic_string();
        }

        for (const auto& e : formations) {
            if (e.confirmation_index >= dev_end) continue;

            const auto available_at =
                data.bars[e.confirmation_index].time + data.period_seconds;
            if (available_at >= DEV_CUTOFF_UTC) continue;

            ++total.formations_dev;
            ++one_dev;

            const std::string key = event_key(relative_file, data, e);
            const std::uint64_t hash = fnv1a64(key);
            const std::uint64_t fold = hash % SAMPLE_MODULUS;
            if (fold != SAMPLE_FOLD) continue;

            ++total.selected;
            ++one_selected;
            if (e.type == FormationType::ENGULF_2) ++total.selected_e2;
            else ++total.selected_e3;

            const std::size_t search_begin = e.confirmation_index + 1;
            const double low = e.zone_low;
            const double high = e.zone_high;
            const bool bullish = e.direction == Direction::BULLISH;

            std::size_t departure = NPOS;
            std::size_t touch = NPOS;
            std::size_t outcome = NPOS;

            LifecycleResult result = LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF;

            const std::size_t first_out =
                index.first_outside(search_begin, dev_end, low, high);

            if (first_out == NPOS) {
                result = LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF;
            } else {
                const double c = data.bars[first_out].close;
                const bool expected_side =
                    bullish ? (c > high) : (c < low);

                if (!expected_side) {
                    result = LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE;
                } else {
                    departure = first_out;

                    const std::size_t return_idx =
                        bullish
                            ? index.first_le(departure + 1, dev_end, high)
                            : index.first_ge(departure + 1, dev_end, low);

                    if (return_idx == NPOS) {
                        result = LifecycleResult::NO_RETURN_BEFORE_CUTOFF;
                    } else {
                        const double rc = data.bars[return_idx].close;
                        const bool inside = rc >= low && rc <= high;

                        if (!inside) {
                            result = LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH;
                        } else {
                            touch = return_idx;

                            const std::size_t out_after_touch =
                                index.first_outside(touch + 1, dev_end, low, high);

                            if (out_after_touch == NPOS) {
                                result = LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF;
                            } else {
                                outcome = out_after_touch;
                                const double oc = data.bars[outcome].close;
                                const bool reaction_side =
                                    bullish ? (oc > high) : (oc < low);

                                result = reaction_side
                                    ? LifecycleResult::REACTION_FIRST
                                    : LifecycleResult::BREAKOUT_FIRST;
                            }
                        }
                    }
                }
            }

            count_result(total, result);
            if (result == LifecycleResult::REACTION_FIRST) {
                ++one_reaction;
            } else if (result == LifecycleResult::BREAKOUT_FIRST) {
                ++one_breakout;
            } else {
                ++one_other;
            }

            events
                << csv_field(key) << ';'
                << fold << ';'
                << csv_field(relative_file) << ';'
                << csv_field(data.symbol) << ';'
                << timeframe_name(data.period_seconds) << ';'
                << formation_name(e.type) << ';'
                << direction_name(e.direction) << ';'
                << data.bars[e.source_index].time << ';'
                << data.bars[e.confirmation_index].time << ';'
                << available_at << ';'
                << std::setprecision(17)
                << low << ';'
                << high << ';';

            if (e.has_first_bar_progress) {
                events
                    << e.first_bar_progress_pct << ';'
                    << progress_bucket_name(progress_bucket(e.first_bar_progress_pct));
            } else {
                events << ';';
            }

            events << ';';

            if (departure != NPOS) events << data.bars[departure].time;
            events << ';';
            if (touch != NPOS) events << data.bars[touch].time;
            events << ';';
            if (outcome != NPOS) events << data.bars[outcome].time;
            events << ';';

            if (departure != NPOS) {
                events << (departure - e.confirmation_index);
            }
            events << ';';

            if (touch != NPOS && departure != NPOS) {
                events << (touch - departure);
            }
            events << ';';

            if (outcome != NPOS && touch != NPOS) {
                events << (outcome - touch);
            }
            events << ';';

            events << lifecycle_name(result) << '\n';
        }

        file_summary
            << csv_field(relative_file) << ';'
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << one_dev << ';'
            << one_selected << ';'
            << one_reaction << ';'
            << one_breakout << ';'
            << one_other << '\n';

        if ((file_idx + 1) % 25 == 0 || file_idx + 1 == files.size()) {
            std::cout
                << '[' << (file_idx + 1) << '/' << files.size() << "] "
                << data.symbol << '_' << timeframe_name(data.period_seconds)
                << " selected=" << total.selected
                << " reaction=" << total.reaction_first
                << " breakout=" << total.breakout_first
                << " failed=" << total.files_failed
                << "\n";
        }
    }

    events.close();
    file_summary.close();
    failures.close();

    std::ofstream summary(summary_path, std::ios::binary);
    if (!summary) {
        std::cerr << "BLOCK03 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 6;
    }

    summary << "RZA CANONICAL BLOCK 03 - CAUSAL REACTION SCREEN\n";
    summary << "DEV_CUTOFF_UTC=2024-01-01T00:00:00Z\n";
    summary << "SAMPLE_MODULUS=" << SAMPLE_MODULUS << "\n";
    summary << "SAMPLE_FOLD=" << SAMPLE_FOLD << "\n";
    summary << "FILES_FOUND=" << total.files_found << "\n";
    summary << "FILES_PASSED=" << total.files_passed << "\n";
    summary << "FILES_FAILED=" << total.files_failed << "\n";
    summary << "SKIPPED_NON_XFBAR=" << total.skipped_non_xfbar << "\n";
    summary << "FORMATIONS_DEV=" << total.formations_dev << "\n";
    summary << "SELECTED=" << total.selected << "\n";
    summary << "SELECTED_ENGULF_2=" << total.selected_e2 << "\n";
    summary << "SELECTED_ENGULF_3=" << total.selected_e3 << "\n";
    summary << "NO_OUTSIDE_BEFORE_CUTOFF=" << total.no_outside << "\n";
    summary << "BROKEN_BEFORE_EXPECTED_DEPARTURE="
            << total.broken_before_departure << "\n";
    summary << "NO_RETURN_BEFORE_CUTOFF=" << total.no_return << "\n";
    summary << "DIRECT_BREAKOUT_NO_CLOSE_TOUCH=" << total.direct_breakout << "\n";
    summary << "REACTION_FIRST=" << total.reaction_first << "\n";
    summary << "BREAKOUT_FIRST=" << total.breakout_first << "\n";
    summary << "TOUCH_UNRESOLVED_AT_CUTOFF=" << total.touch_unresolved << "\n";
    summary << "TOUCH_DEFINITION=CLOSE_REENTRY_AFTER_EXPECTED_DEPARTURE\n";
    summary << "OUTCOME_DEFINITION=FIRST_CLOSE_OUTSIDE_RECTANGLE_AFTER_TOUCH\n";
    summary.close();

    std::cout << "------------------------------------------------------------\n";
    std::cout << "FILES_FOUND=" << total.files_found << "\n";
    std::cout << "FILES_PASSED=" << total.files_passed << "\n";
    std::cout << "FILES_FAILED=" << total.files_failed << "\n";
    std::cout << "SKIPPED_NON_XFBAR=" << total.skipped_non_xfbar << "\n";
    std::cout << "FORMATIONS_DEV=" << total.formations_dev << "\n";
    std::cout << "SELECTED=" << total.selected << "\n";
    std::cout << "REACTION_FIRST=" << total.reaction_first << "\n";
    std::cout << "BREAKOUT_FIRST=" << total.breakout_first << "\n";
    std::cout << "SUMMARY=" << summary_path.string() << "\n";

    if (total.files_failed != 0) {
        std::cout << "BLOCK03 FAIL - INVALID_XFBAR_FILES="
                  << total.files_failed << "\n";
        return 7;
    }

    if (total.selected == 0) {
        std::cout << "BLOCK03 FAIL - ZERO_SELECTED_EVENTS\n";
        return 8;
    }

    std::cout << "BLOCK03 PASS\n";
    return 0;
}
