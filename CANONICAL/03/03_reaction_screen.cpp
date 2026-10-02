#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;

namespace {

constexpr std::int64_t DEV_CUTOFF_UTC = 1704067200LL;
constexpr std::uint64_t SAMPLE_MODULUS = 64;
constexpr std::uint64_t SAMPLE_FOLD = 0;

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

const char* direction_name(Direction d) {
    return d == Direction::BULLISH ? "BULLISH" : "BEARISH";
}

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

    return static_cast<std::size_t>(
        std::distance(data.bars.begin(), it));
}

LifecycleResult evaluate_lifecycle(
    const XfbarData& data,
    const ap::CloseIndex& index,
    const FormationEvent& e,
    std::size_t dev_end)
{
    const std::size_t begin = e.confirmation_index + 1;
    const double low = e.zone_low;
    const double high = e.zone_high;
    const bool bullish = e.direction == Direction::BULLISH;

    const std::size_t first_out =
        index.first_outside(begin, dev_end, low, high);

    if (first_out == ap::CloseIndex::npos()) {
        return LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF;
    }

    const double first_close = data.bars[first_out].close;
    const bool expected_side =
        bullish ? (first_close > high) : (first_close < low);

    if (!expected_side) {
        return LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE;
    }

    const std::size_t return_idx =
        bullish
            ? index.first_le(first_out + 1, dev_end, high)
            : index.first_ge(first_out + 1, dev_end, low);

    if (return_idx == ap::CloseIndex::npos()) {
        return LifecycleResult::NO_RETURN_BEFORE_CUTOFF;
    }

    const double return_close = data.bars[return_idx].close;
    const bool inside =
        return_close >= low && return_close <= high;

    if (!inside) {
        return LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH;
    }

    const std::size_t out_after_touch =
        index.first_outside(return_idx + 1, dev_end, low, high);

    if (out_after_touch == ap::CloseIndex::npos()) {
        return LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF;
    }

    const double outcome_close = data.bars[out_after_touch].close;
    const bool reaction_side =
        bullish ? (outcome_close > high) : (outcome_close < low);

    return reaction_side
        ? LifecycleResult::REACTION_FIRST
        : LifecycleResult::BREAKOUT_FIRST;
}

struct Totals {
    std::uint64_t files_found = 0;
    std::uint64_t files_passed = 0;
    std::uint64_t files_failed = 0;
    std::uint64_t skipped_non_xfbar = 0;
    std::uint64_t m5_atr_missing_files = 0;

    std::uint64_t formations_dev = 0;
    std::uint64_t accepted_zones_dev = 0;
    std::uint64_t rejected_gap_dev = 0;
    std::uint64_t selected = 0;

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
        case LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF:
            ++t.no_outside; break;
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            ++t.broken_before_departure; break;
        case LifecycleResult::NO_RETURN_BEFORE_CUTOFF:
            ++t.no_return; break;
        case LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH:
            ++t.direct_breakout; break;
        case LifecycleResult::REACTION_FIRST:
            ++t.reaction_first; break;
        case LifecycleResult::BREAKOUT_FIRST:
            ++t.breakout_first; break;
        case LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF:
            ++t.touch_unresolved; break;
    }
}

bool build_atr_lookup(
    const std::string& current_file,
    const XfbarData& data,
    ap::AtrLookup& out)
{
    const auto& p = ap::params();

    if (data.period_seconds == p.atr_timeframe_seconds) {
        out.build(data.bars, p.atr_period);
        return out.available();
    }

    const fs::path m5 =
        ap::find_m5_sibling(fs::path(current_file), data.symbol);

    if (m5.empty()) {
        out = ap::AtrLookup{};
        return false;
    }

    const auto m5_data = read_xfbar(m5.string());

    if (!m5_data.success ||
        m5_data.period_seconds != p.atr_timeframe_seconds)
    {
        out = ap::AtrLookup{};
        return false;
    }

    out.build(m5_data.bars, p.atr_period);
    return out.available();
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
            : fs::path("../03out");

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
        << "EventKey;File;Symbol;Timeframe;Direction;"
        << "SourceTime;ConfirmBarTime;AvailableAt;ZoneLow;ZoneHigh;"
        << "RequiredGap;DepartureTime;TouchTime;OutcomeTime;"
        << "LifecycleResult\n";

    file_summary
        << "File;Symbol;Timeframe;DevFormations;AcceptedZones;"
        << "RejectedGap;Selected;ReactionFirst;BreakoutFirst;OtherLifecycle;"
        << "M5AtrAvailable\n";

    failures << "File;Reason\n";

    std::cout << "============================================================\n";
    std::cout << "RZA CANONICAL BLOCK 03 - ABS_TRACK ZONE POLICY\n";
    std::cout << "DEV_CUTOFF=2024-01-01T00:00:00Z\n";
    std::cout << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n";
    std::cout << "MIN_ZONE_HEIGHT_POINTS=225\n";
    std::cout << "MIN_GAP=max(20 points, 0.30 * ATR(M5,14))\n";
    std::cout << "DELETE=10 points beyond opposite boundary by close\n";
    std::cout << "HISTORICAL_SPREAD_FLOOR=30 points\n";
    std::cout << "SAMPLE=FNV1A64 %% 64 == 0 AFTER ZONE ACCEPTANCE\n";
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

        if (!(data.point > 0.0)) {
            ++total.files_failed;
            failures
                << csv_field(files[file_idx]) << ';'
                << "nonpositive_point\n";
            continue;
        }

        const std::size_t dev_end =
            development_end_exclusive(data);

        if (dev_end < 3) {
            continue;
        }

        ap::AtrLookup atr;
        const bool atr_available =
            build_atr_lookup(files[file_idx], data, atr);

        if (!atr_available) {
            ++total.m5_atr_missing_files;
        }

        const ap::CloseIndex index(data.bars);
        ap::ActiveZones active;

        std::uint64_t one_formations = 0;
        std::uint64_t one_accepted = 0;
        std::uint64_t one_rejected_gap = 0;
        std::uint64_t one_selected = 0;
        std::uint64_t one_reaction = 0;
        std::uint64_t one_breakout = 0;
        std::uint64_t one_other = 0;

        std::string relative_file;
        {
            std::error_code rel_ec;
            relative_file =
                fs::relative(
                    files[file_idx],
                    data_root,
                    rel_ec).generic_string();

            if (rel_ec) {
                relative_file =
                    fs::path(files[file_idx]).filename().generic_string();
            }
        }

        for (std::size_t confirm_idx = 0;
             confirm_idx < dev_end;
             ++confirm_idx)
        {
            const std::int64_t available_at =
                data.bars[confirm_idx].time + data.period_seconds;

            if (available_at >= DEV_CUTOFF_UTC) {
                break;
            }

            active.expire(confirm_idx);

            const auto e_opt =
                detect_at(data.bars, confirm_idx);

            if (!e_opt.has_value()) {
                continue;
            }

            FormationEvent e = *e_opt;
            ++total.formations_dev;
            ++one_formations;

            const ap::ZoneBounds z =
                ap::calculate_zone_bounds(
                    data.bars[e.source_index],
                    e.direction,
                    data.point);

            e.zone_low = z.low;
            e.zone_high = z.high;

            const double atr_value =
                atr.at_decision(available_at);

            const double req_gap =
                ap::required_gap(data.point, atr_value);

            if (!active.can_accept(
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    req_gap))
            {
                ++total.rejected_gap_dev;
                ++one_rejected_gap;
                continue;
            }

            const std::size_t break_idx =
                ap::zone_break_index(
                    index,
                    data.bars,
                    confirm_idx,
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    data.point);

            active.insert(
                e.direction,
                e.zone_low,
                e.zone_high,
                break_idx);

            ++total.accepted_zones_dev;
            ++one_accepted;

            const std::string key =
                event_key(relative_file, data, e);

            if ((fnv1a64(key) % SAMPLE_MODULUS) != SAMPLE_FOLD) {
                continue;
            }

            ++total.selected;
            ++one_selected;

            const LifecycleResult result =
                evaluate_lifecycle(
                    data,
                    index,
                    e,
                    dev_end);

            count_result(total, result);

            if (result == LifecycleResult::REACTION_FIRST) {
                ++one_reaction;
            } else if (result == LifecycleResult::BREAKOUT_FIRST) {
                ++one_breakout;
            } else {
                ++one_other;
            }

            std::size_t departure = ap::CloseIndex::npos();
            std::size_t touch = ap::CloseIndex::npos();
            std::size_t outcome = ap::CloseIndex::npos();

            const bool bullish =
                e.direction == Direction::BULLISH;

            departure =
                index.first_outside(
                    e.confirmation_index + 1,
                    dev_end,
                    e.zone_low,
                    e.zone_high);

            if (departure != ap::CloseIndex::npos()) {
                const double dc = data.bars[departure].close;
                const bool expected =
                    bullish
                        ? dc > e.zone_high
                        : dc < e.zone_low;

                if (expected) {
                    const std::size_t r =
                        bullish
                            ? index.first_le(
                                  departure + 1,
                                  dev_end,
                                  e.zone_high)
                            : index.first_ge(
                                  departure + 1,
                                  dev_end,
                                  e.zone_low);

                    if (r != ap::CloseIndex::npos()) {
                        const double rc = data.bars[r].close;

                        if (rc >= e.zone_low &&
                            rc <= e.zone_high)
                        {
                            touch = r;
                            outcome =
                                index.first_outside(
                                    touch + 1,
                                    dev_end,
                                    e.zone_low,
                                    e.zone_high);
                        }
                    }
                }
            }

            events
                << csv_field(key) << ';'
                << csv_field(relative_file) << ';'
                << csv_field(data.symbol) << ';'
                << timeframe_name(data.period_seconds) << ';'
                << direction_name(e.direction) << ';'
                << data.bars[e.source_index].time << ';'
                << data.bars[e.confirmation_index].time << ';'
                << available_at << ';'
                << std::setprecision(17)
                << e.zone_low << ';'
                << e.zone_high << ';'
                << req_gap << ';';

            if (departure != ap::CloseIndex::npos()) {
                events << data.bars[departure].time;
            }
            events << ';';

            if (touch != ap::CloseIndex::npos()) {
                events << data.bars[touch].time;
            }
            events << ';';

            if (outcome != ap::CloseIndex::npos()) {
                events << data.bars[outcome].time;
            }
            events << ';';

            events << lifecycle_name(result) << '\n';
        }

        file_summary
            << csv_field(relative_file) << ';'
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << one_formations << ';'
            << one_accepted << ';'
            << one_rejected_gap << ';'
            << one_selected << ';'
            << one_reaction << ';'
            << one_breakout << ';'
            << one_other << ';'
            << (atr_available ? 1 : 0)
            << '\n';

        if ((file_idx + 1) % 25 == 0 ||
            file_idx + 1 == files.size())
        {
            std::cout
                << '[' << (file_idx + 1)
                << '/' << files.size() << "] "
                << data.symbol << '_'
                << timeframe_name(data.period_seconds)
                << " accepted=" << total.accepted_zones_dev
                << " rejected_gap=" << total.rejected_gap_dev
                << " selected=" << total.selected
                << " reaction=" << total.reaction_first
                << " breakout=" << total.breakout_first
                << " failed=" << total.files_failed
                << '\n';
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

    summary << "RZA CANONICAL BLOCK 03 - ABS_TRACK ZONE POLICY\n";
    summary << "DEV_CUTOFF_UTC=2024-01-01T00:00:00Z\n";
    summary << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n";
    summary << "FORMATION_TYPE_USED_FOR_SPLIT=0\n";
    summary << "PROGRESS_USED=0\n";
    summary << "MIN_ZONE_HEIGHT_POINTS=225\n";
    summary << "MIN_GAP_POINTS=20\n";
    summary << "MIN_GAP_ATR=0.30\n";
    summary << "DISTANCE_ATR_TIMEFRAME=M5\n";
    summary << "DISTANCE_ATR_PERIOD=14\n";
    summary << "DELETION_THRESHOLD_POINTS=10\n";
    summary << "HISTORICAL_SPREAD_POINTS=30\n";
    summary << "SAMPLE_MODULUS=" << SAMPLE_MODULUS << "\n";
    summary << "SAMPLE_FOLD=" << SAMPLE_FOLD << "\n";
    summary << "FILES_FOUND=" << total.files_found << "\n";
    summary << "FILES_PASSED=" << total.files_passed << "\n";
    summary << "FILES_FAILED=" << total.files_failed << "\n";
    summary << "SKIPPED_NON_XFBAR=" << total.skipped_non_xfbar << "\n";
    summary << "M5_ATR_MISSING_FILES=" << total.m5_atr_missing_files << "\n";
    summary << "FORMATIONS_DEV=" << total.formations_dev << "\n";
    summary << "ACCEPTED_ZONES_DEV=" << total.accepted_zones_dev << "\n";
    summary << "REJECTED_GAP_DEV=" << total.rejected_gap_dev << "\n";
    summary << "SELECTED=" << total.selected << "\n";
    summary << "NO_OUTSIDE_BEFORE_CUTOFF=" << total.no_outside << "\n";
    summary << "BROKEN_BEFORE_EXPECTED_DEPARTURE="
            << total.broken_before_departure << "\n";
    summary << "NO_RETURN_BEFORE_CUTOFF=" << total.no_return << "\n";
    summary << "DIRECT_BREAKOUT_NO_CLOSE_TOUCH="
            << total.direct_breakout << "\n";
    summary << "REACTION_FIRST=" << total.reaction_first << "\n";
    summary << "BREAKOUT_FIRST=" << total.breakout_first << "\n";
    summary << "TOUCH_UNRESOLVED_AT_CUTOFF="
            << total.touch_unresolved << "\n";
    summary.close();

    std::cout << "------------------------------------------------------------\n";
    std::cout << "FILES_FOUND=" << total.files_found << "\n";
    std::cout << "FILES_PASSED=" << total.files_passed << "\n";
    std::cout << "FILES_FAILED=" << total.files_failed << "\n";
    std::cout << "SKIPPED_NON_XFBAR=" << total.skipped_non_xfbar << "\n";
    std::cout << "M5_ATR_MISSING_FILES="
              << total.m5_atr_missing_files << "\n";
    std::cout << "FORMATIONS_DEV=" << total.formations_dev << "\n";
    std::cout << "ACCEPTED_ZONES_DEV="
              << total.accepted_zones_dev << "\n";
    std::cout << "REJECTED_GAP_DEV="
              << total.rejected_gap_dev << "\n";
    std::cout << "SELECTED=" << total.selected << "\n";
    std::cout << "REACTION_FIRST=" << total.reaction_first << "\n";
    std::cout << "BREAKOUT_FIRST=" << total.breakout_first << "\n";
    std::cout << "SUMMARY=" << summary_path.string() << "\n";

    if (total.files_failed != 0) {
        std::cout << "BLOCK03 FAIL - INVALID_XFBAR_FILES="
                  << total.files_failed << "\n";
        return 7;
    }

    if (total.accepted_zones_dev == 0 ||
        total.selected == 0)
    {
        std::cout << "BLOCK03 FAIL - ZERO_ACCEPTED_OR_SELECTED\n";
        return 8;
    }

    std::cout << "BLOCK03 PASS\n";
    return 0;
}
