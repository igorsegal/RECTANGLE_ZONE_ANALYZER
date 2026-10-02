#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;

namespace {

constexpr std::int64_t OOS_START_UTC = 1704067200LL;

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
    std::uint64_t m5_atr_missing_files = 0;

    std::uint64_t formations_total_replayed = 0;
    std::uint64_t accepted_total_replayed = 0;
    std::uint64_t rejected_gap_total = 0;

    std::uint64_t formations_oos = 0;
    std::uint64_t accepted_oos = 0;
    std::uint64_t rejected_gap_oos = 0;

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

double reaction_rate_pct(const Stats& s) {
    if (s.resolved() == 0) return 0.0;

    return 100.0 *
        static_cast<double>(s.reaction) /
        static_cast<double>(s.resolved());
}

std::pair<double,double> wilson95_pct(const Stats& s) {
    const double n = static_cast<double>(s.resolved());
    if (n <= 0.0) return {0.0, 0.0};

    const double p =
        static_cast<double>(s.reaction) / n;
    const double z = 1.959963984540054;
    const double z2 = z * z;

    const double denom = 1.0 + z2 / n;
    const double center =
        (p + z2 / (2.0 * n)) / denom;

    const double half =
        z * std::sqrt(
            (p * (1.0 - p) / n) +
            (z2 / (4.0 * n * n))) /
        denom;

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
            ++t.no_outside; break;
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            ++t.broken_before_departure; break;
        case LifecycleResult::NO_RETURN_BEFORE_END:
            ++t.no_return; break;
        case LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH:
            ++t.direct_breakout; break;
        case LifecycleResult::TOUCH_UNRESOLVED_AT_END:
            ++t.touch_unresolved; break;
        case LifecycleResult::REACTION_FIRST:
        case LifecycleResult::BREAKOUT_FIRST:
            break;
    }
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
        ap::find_m5_sibling(
            fs::path(current_file),
            data.symbol);

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

LifecycleResult evaluate_lifecycle(
    const XfbarData& data,
    const ap::CloseIndex& index,
    const FormationEvent& e)
{
    const std::size_t end = data.bars.size();
    const std::size_t begin = e.confirmation_index + 1;
    const double low = e.zone_low;
    const double high = e.zone_high;
    const bool bullish =
        e.direction == Direction::BULLISH;

    const std::size_t first_out =
        index.first_outside(
            begin,
            end,
            low,
            high);

    if (first_out == ap::CloseIndex::npos()) {
        return LifecycleResult::NO_OUTSIDE_BEFORE_END;
    }

    const double first_close =
        data.bars[first_out].close;

    const bool expected_side =
        bullish
            ? first_close > high
            : first_close < low;

    if (!expected_side) {
        return LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE;
    }

    const std::size_t return_idx =
        bullish
            ? index.first_le(
                  first_out + 1,
                  end,
                  high)
            : index.first_ge(
                  first_out + 1,
                  end,
                  low);

    if (return_idx == ap::CloseIndex::npos()) {
        return LifecycleResult::NO_RETURN_BEFORE_END;
    }

    const double return_close =
        data.bars[return_idx].close;

    const bool inside =
        return_close >= low &&
        return_close <= high;

    if (!inside) {
        return LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH;
    }

    const std::size_t out_after_touch =
        index.first_outside(
            return_idx + 1,
            end,
            low,
            high);

    if (out_after_touch == ap::CloseIndex::npos()) {
        return LifecycleResult::TOUCH_UNRESOLVED_AT_END;
    }

    const double outcome_close =
        data.bars[out_after_touch].close;

    const bool reaction_side =
        bullish
            ? outcome_close > high
            : outcome_close < low;

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
            << " | CI95=[" << ci.first
            << ',' << ci.second << ']';
    } else {
        os
            << " | reaction_pct=NA"
            << " | CI95=[NA,NA]";
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

    if (!fs::exists(data_root, ec) ||
        !fs::is_directory(data_root, ec))
    {
        std::cerr
            << "BLOCK05 FAIL - DATA_ROOT_NOT_FOUND\n";
        return 2;
    }

    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr
            << "BLOCK05 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 3;
    }

    const fs::path summary_path =
        out_root / "05_OOS_SUMMARY.txt";

    const fs::path failures_path =
        out_root / "05_FAILURES.csv";

    std::ofstream failures(
        failures_path,
        std::ios::binary);

    if (!failures) {
        std::cerr
            << "BLOCK05 FAIL - CANNOT_OPEN_FAILURES\n";
        return 4;
    }

    failures << "File;Reason\n";

    const auto files =
        find_bin_files(data_root);

    if (files.empty()) {
        std::cerr
            << "BLOCK05 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    Totals total;
    total.files_found = files.size();

    std::cout
        << "============================================================\n"
        << "RZA CANONICAL BLOCK 05 - FINAL OOS WITH ABS_TRACK POLICY\n"
        << "OOS_START=2024-01-01T00:00:00Z\n"
        << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n"
        << "FORMATION_SPLIT=OFF\n"
        << "PROGRESS=OFF\n"
        << "MIN_ZONE_HEIGHT_POINTS=225\n"
        << "MIN_GAP=max(20 points, 0.30 * ATR(M5,14))\n"
        << "DELETE=10 points beyond opposite boundary by close\n"
        << "HISTORICAL_SPREAD_FLOOR=30 points\n"
        << "OOS_SAMPLE=ALL_ACCEPTED_ZONES\n"
        << "OUTPUT=SUMMARY_ONLY\n"
        << "============================================================\n";

    for (std::size_t file_idx = 0;
         file_idx < files.size();
         ++file_idx)
    {
        const auto data =
            read_xfbar(files[file_idx]);

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

        if (!(data.point > 0.0)) {
            ++total.files_failed;
            failures
                << files[file_idx] << ';'
                << "nonpositive_point\n";
            continue;
        }

        ap::AtrLookup atr;
        const bool atr_available =
            build_atr_lookup(
                files[file_idx],
                data,
                atr);

        if (!atr_available) {
            ++total.m5_atr_missing_files;
        }

        const ap::CloseIndex index(data.bars);
        ap::ActiveZones active;

        for (std::size_t confirm_idx = 0;
             confirm_idx < data.bars.size();
             ++confirm_idx)
        {
            const std::int64_t available_at =
                data.bars[confirm_idx].time +
                data.period_seconds;

            active.expire(confirm_idx);

            const auto e_opt =
                detect_at(data.bars, confirm_idx);

            if (!e_opt.has_value()) {
                continue;
            }

            FormationEvent e = *e_opt;

            ++total.formations_total_replayed;

            const bool is_oos =
                available_at >= OOS_START_UTC;

            if (is_oos) {
                ++total.formations_oos;
            }

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
                ap::required_gap(
                    data.point,
                    atr_value);

            if (!active.can_accept(
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    req_gap))
            {
                ++total.rejected_gap_total;

                if (is_oos) {
                    ++total.rejected_gap_oos;
                }
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

            ++total.accepted_total_replayed;

            if (!is_oos) {
                continue;
            }

            ++total.accepted_oos;

            const LifecycleResult result =
                evaluate_lifecycle(
                    data,
                    index,
                    e);

            count_lifecycle(total, result);
            update_stats(total.overall, result);

            update_stats(
                total.by_direction[
                    direction_name(e.direction)],
                result);

            update_stats(
                total.by_timeframe[
                    timeframe_name(
                        data.period_seconds)],
                result);
        }

        if ((file_idx + 1) % 25 == 0 ||
            file_idx + 1 == files.size())
        {
            std::cout
                << '[' << (file_idx + 1)
                << '/' << files.size() << "] "
                << data.symbol << '_'
                << timeframe_name(
                    data.period_seconds)
                << " accepted_oos="
                << total.accepted_oos
                << " resolved="
                << total.overall.resolved()
                << " reaction="
                << total.overall.reaction
                << " breakout="
                << total.overall.breakout
                << " failed="
                << total.files_failed
                << '\n';
        }
    }

    failures.close();

    std::ofstream summary(
        summary_path,
        std::ios::binary);

    if (!summary) {
        std::cerr
            << "BLOCK05 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 6;
    }

    const auto overall_ci =
        wilson95_pct(total.overall);

    summary
        << "RZA CANONICAL BLOCK 05 - FINAL OOS WITH ABS_TRACK POLICY\n"
        << "OOS_START_UTC=2024-01-01T00:00:00Z\n"
        << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n"
        << "FORMATION_TYPE_USED_FOR_SPLIT=0\n"
        << "PROGRESS_USED=0\n"
        << "MIN_ZONE_HEIGHT_POINTS=225\n"
        << "MIN_GAP_POINTS=20\n"
        << "MIN_GAP_ATR=0.30\n"
        << "DISTANCE_ATR_TIMEFRAME=M5\n"
        << "DISTANCE_ATR_PERIOD=14\n"
        << "DELETION_THRESHOLD_POINTS=10\n"
        << "HISTORICAL_SPREAD_POINTS=30\n"
        << "OOS_SAMPLE=ALL_ACCEPTED_ZONES\n"
        << "FILES_FOUND=" << total.files_found << '\n'
        << "FILES_PASSED=" << total.files_passed << '\n'
        << "FILES_FAILED=" << total.files_failed << '\n'
        << "SKIPPED_NON_XFBAR="
        << total.skipped_non_xfbar << '\n'
        << "M5_ATR_MISSING_FILES="
        << total.m5_atr_missing_files << '\n'
        << "FORMATIONS_TOTAL_REPLAYED="
        << total.formations_total_replayed << '\n'
        << "ACCEPTED_TOTAL_REPLAYED="
        << total.accepted_total_replayed << '\n'
        << "REJECTED_GAP_TOTAL="
        << total.rejected_gap_total << '\n'
        << "FORMATIONS_OOS="
        << total.formations_oos << '\n'
        << "ACCEPTED_OOS="
        << total.accepted_oos << '\n'
        << "REJECTED_GAP_OOS="
        << total.rejected_gap_oos << '\n'
        << "NO_OUTSIDE_BEFORE_END="
        << total.no_outside << '\n'
        << "BROKEN_BEFORE_EXPECTED_DEPARTURE="
        << total.broken_before_departure << '\n'
        << "NO_RETURN_BEFORE_END="
        << total.no_return << '\n'
        << "DIRECT_BREAKOUT_NO_CLOSE_TOUCH="
        << total.direct_breakout << '\n'
        << "TOUCH_UNRESOLVED_AT_END="
        << total.touch_unresolved << '\n';

    summary << "\n[OVERALL]\n";
    write_stats_line(
        summary,
        "ALL",
        total.overall);

    summary << "\n[DIRECTION]\n";
    for (const auto& kv : total.by_direction) {
        write_stats_line(
            summary,
            kv.first,
            kv.second);
    }

    summary << "\n[TIMEFRAME]\n";
    for (const auto& kv : total.by_timeframe) {
        write_stats_line(
            summary,
            kv.first,
            kv.second);
    }

    const auto bear_it =
        total.by_direction.find("BEARISH");

    const auto bull_it =
        total.by_direction.find("BULLISH");

    const bool overall_confirmed =
        total.overall.resolved() > 0 &&
        overall_ci.first > 50.0;

    const bool bearish_confirmed =
        bear_it != total.by_direction.end() &&
        bear_it->second.resolved() > 0 &&
        wilson95_pct(
            bear_it->second).first > 50.0;

    const bool bullish_confirmed =
        bull_it != total.by_direction.end() &&
        bull_it->second.resolved() > 0 &&
        wilson95_pct(
            bull_it->second).first > 50.0;

    const bool structural_gate =
        overall_confirmed &&
        bearish_confirmed &&
        bullish_confirmed;

    summary
        << "\n[FINAL_GATE]\n"
        << "GATE_RULE=LOWER_WILSON95_REACTION_PCT_GT_50_OVERALL_AND_BOTH_DIRECTIONS\n"
        << "OVERALL_CONFIRMED="
        << (overall_confirmed ? 1 : 0) << '\n'
        << "BEARISH_CONFIRMED="
        << (bearish_confirmed ? 1 : 0) << '\n'
        << "BULLISH_CONFIRMED="
        << (bullish_confirmed ? 1 : 0) << '\n'
        << "STRUCTURAL_HYPOTHESIS_GATE="
        << (structural_gate ? "PASS" : "FAIL")
        << '\n';

    summary
        << "\nCONTRACT:\n"
        << "- Full history is replayed causally to seed active zones before OOS.\n"
        << "- OOS statistics use every accepted 2024+ zone; no 1/64 sampling.\n"
        << "- Formation type is not used for grouping or filtering.\n"
        << "- Progress is not used.\n"
        << "- Zone bounds, distance filter and deletion threshold reproduce ABS_TRACK_v2 policy.\n"
        << "- This is a structural reaction/breakout test, not a trading PnL test.\n";

    summary.close();

    std::cout
        << "------------------------------------------------------------\n"
        << "FILES_FOUND=" << total.files_found << '\n'
        << "FILES_PASSED=" << total.files_passed << '\n'
        << "FILES_FAILED=" << total.files_failed << '\n'
        << "SKIPPED_NON_XFBAR="
        << total.skipped_non_xfbar << '\n'
        << "M5_ATR_MISSING_FILES="
        << total.m5_atr_missing_files << '\n'
        << "FORMATIONS_OOS="
        << total.formations_oos << '\n'
        << "ACCEPTED_OOS="
        << total.accepted_oos << '\n'
        << "REJECTED_GAP_OOS="
        << total.rejected_gap_oos << '\n'
        << "RESOLVED="
        << total.overall.resolved() << '\n'
        << "REACTION="
        << total.overall.reaction << '\n'
        << "BREAKOUT="
        << total.overall.breakout << '\n'
        << std::fixed << std::setprecision(6)
        << "REACTION_PCT="
        << reaction_rate_pct(total.overall) << '\n'
        << "STRUCTURAL_HYPOTHESIS_GATE="
        << (structural_gate ? "PASS" : "FAIL")
        << '\n'
        << "SUMMARY="
        << summary_path.string()
        << '\n';

    if (total.files_failed != 0) {
        std::cout
            << "BLOCK05 FAIL - INVALID_XFBAR_FILES="
            << total.files_failed << '\n';
        return 7;
    }

    if (total.accepted_oos == 0) {
        std::cout
            << "BLOCK05 FAIL - ZERO_ACCEPTED_OOS_ZONES\n";
        return 8;
    }

    std::cout << "BLOCK05 PASS\n";
    return 0;
}
