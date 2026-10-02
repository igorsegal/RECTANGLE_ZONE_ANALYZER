#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"
#include "../trade_emulator.h"
#include "../reaction_levels.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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
namespace te = rza::canonical::trade_emulation;
namespace rl = rza::canonical::reaction_levels;

namespace {

constexpr std::int64_t OOS_START_UTC = 1704067200LL; // 2024-01-01T00:00:00Z

enum class LifecycleResult {
    NO_OUTSIDE_BEFORE_END,
    BROKEN_BEFORE_EXPECTED_DEPARTURE,
    NO_RETURN_BEFORE_END,
    DIRECT_BREAKOUT_NO_CLOSE_TOUCH,
    REACTION_FIRST,
    BREAKOUT_FIRST,
    TOUCH_UNRESOLVED_AT_END
};

struct Evaluation {
    LifecycleResult result;
    std::size_t touch_index = ap::CloseIndex::npos();
    std::size_t outcome_index = ap::CloseIndex::npos();
};

struct Stats {
    std::uint64_t accepted = 0;
    std::uint64_t reaction = 0;
    std::uint64_t breakout = 0;
    std::uint64_t no_outside = 0;
    std::uint64_t broken_before_departure = 0;
    std::uint64_t no_return = 0;
    std::uint64_t direct_breakout = 0;
    std::uint64_t unresolved = 0;

    std::uint64_t resolved() const {
        return reaction + breakout;
    }

    std::uint64_t other() const {
        return no_outside +
               broken_before_departure +
               no_return +
               direct_breakout +
               unresolved;
    }
};

struct Totals {
    std::uint64_t bin_files_scanned = 0;
    std::uint64_t xfbar_files_passed = 0;
    std::uint64_t xfbar_files_failed = 0;
    std::uint64_t skipped_non_xfbar = 0;

    std::uint64_t series_replayed = 0;
    std::uint64_t series_with_oos = 0;
    std::uint64_t series_excluded_no_m5_atr = 0;
    std::uint64_t duplicate_identical_series = 0;
    std::uint64_t duplicate_conflict_series = 0;

    std::uint64_t candidate_total = 0;
    std::uint64_t accepted_total = 0;
    std::uint64_t rejected_gap_total = 0;

    std::uint64_t candidate_oos = 0;
    std::uint64_t accepted_oos = 0;
    std::uint64_t rejected_gap_oos = 0;

    Stats oos;
};

struct SeenSeries {
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

double reaction_rate_pct(const Stats& s) {
    if (s.resolved() == 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return 100.0 *
        static_cast<double>(s.reaction) /
        static_cast<double>(s.resolved());
}

std::pair<double,double> wilson95_pct(const Stats& s) {
    const double n =
        static_cast<double>(s.resolved());

    if (n <= 0.0) {
        return {
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN()
        };
    }

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

void count_result(Stats& s, LifecycleResult r) {
    switch (r) {
        case LifecycleResult::NO_OUTSIDE_BEFORE_END:
            ++s.no_outside;
            break;
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            ++s.broken_before_departure;
            break;
        case LifecycleResult::NO_RETURN_BEFORE_END:
            ++s.no_return;
            break;
        case LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH:
            ++s.direct_breakout;
            break;
        case LifecycleResult::REACTION_FIRST:
            ++s.reaction;
            break;
        case LifecycleResult::BREAKOUT_FIRST:
            ++s.breakout;
            break;
        case LifecycleResult::TOUCH_UNRESOLVED_AT_END:
            ++s.unresolved;
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

std::uint64_t mix_u64(std::uint64_t h, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        const unsigned char b =
            static_cast<unsigned char>((v >> (i * 8)) & 0xffu);

        h ^= static_cast<std::uint64_t>(b);
        h *= 1099511628211ULL;
    }

    return h;
}

std::uint64_t double_bits(double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "double must be 64-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::uint64_t series_fingerprint(const XfbarData& data) {
    std::uint64_t h = 14695981039346656037ULL;

    for (unsigned char ch : data.symbol) {
        h ^= static_cast<std::uint64_t>(ch);
        h *= 1099511628211ULL;
    }

    h = mix_u64(
        h,
        static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(data.period_seconds)));

    h = mix_u64(
        h,
        double_bits(data.point));

    h = mix_u64(
        h,
        static_cast<std::uint64_t>(data.bars.size()));

    for (const Bar& b : data.bars) {
        h = mix_u64(h, static_cast<std::uint64_t>(b.time));
        h = mix_u64(h, double_bits(b.open));
        h = mix_u64(h, double_bits(b.high));
        h = mix_u64(h, double_bits(b.low));
        h = mix_u64(h, double_bits(b.close));
    }

    return h;
}

bool load_symbol_m5_atr(
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

    const auto m5_data =
        read_xfbar(m5.string());

    if (!m5_data.success ||
        m5_data.period_seconds != p.atr_timeframe_seconds)
    {
        out = ap::AtrLookup{};
        return false;
    }

    out.build(
        m5_data.bars,
        p.atr_period);

    return out.available();
}

Evaluation evaluate_lifecycle(
    const XfbarData& data,
    const ap::CloseIndex& index,
    const FormationEvent& e)
{
    const std::size_t end =
        data.bars.size();

    const std::size_t begin =
        e.confirmation_index + 1;

    const bool bullish =
        e.direction == Direction::BULLISH;

    const std::size_t first_out =
        index.first_outside(
            begin,
            end,
            e.zone_low,
            e.zone_high);

    if (first_out == ap::CloseIndex::npos()) {
        return {LifecycleResult::NO_OUTSIDE_BEFORE_END};
    }

    const double first_close =
        data.bars[first_out].close;

    const bool expected_side =
        bullish
            ? first_close > e.zone_high
            : first_close < e.zone_low;

    if (!expected_side) {
        return {LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE};
    }

    const std::size_t return_idx =
        bullish
            ? index.first_le(
                  first_out + 1,
                  end,
                  e.zone_high)
            : index.first_ge(
                  first_out + 1,
                  end,
                  e.zone_low);

    if (return_idx == ap::CloseIndex::npos()) {
        return {LifecycleResult::NO_RETURN_BEFORE_END};
    }

    const double return_close =
        data.bars[return_idx].close;

    if (return_close < e.zone_low ||
        return_close > e.zone_high)
    {
        return {LifecycleResult::DIRECT_BREAKOUT_NO_CLOSE_TOUCH};
    }

    const std::size_t outcome =
        index.first_outside(
            return_idx + 1,
            end,
            e.zone_low,
            e.zone_high);

    if (outcome == ap::CloseIndex::npos()) {
        return {LifecycleResult::TOUCH_UNRESOLVED_AT_END, return_idx, ap::CloseIndex::npos()};
    }

    const double outcome_close =
        data.bars[outcome].close;

    const bool reaction_side =
        bullish
            ? outcome_close > e.zone_high
            : outcome_close < e.zone_low;

    return {
        reaction_side
            ? LifecycleResult::REACTION_FIRST
            : LifecycleResult::BREAKOUT_FIRST,
        return_idx,
        outcome
    };
}

void write_rate_fields(
    std::ostream& out,
    const Stats& s)
{
    if (s.resolved() == 0) {
        out << ";;";
        return;
    }

    const auto ci =
        wilson95_pct(s);

    out
        << std::fixed << std::setprecision(9)
        << reaction_rate_pct(s) << ';'
        << ci.first << ';'
        << ci.second;
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

    const fs::path instrument_path =
        out_root / "05_OOS_INSTRUMENT_STATS.csv";

    const fs::path trade_path =
        out_root / "05_OOS_TRADE_EMULATION.csv";

    const fs::path reaction_levels_path =
        out_root / "05_OOS_REACTION_LEVELS.csv";

    const fs::path oos_reaction_path_path =
        out_root / "05_OOS_REACTION_PATH.csv";

    const fs::path failures_path =
        out_root / "05_FAILURES.csv";

    const fs::path summary_path =
        out_root / "05_OOS_SUMMARY.txt";

    std::ofstream instrument(
        instrument_path,
        std::ios::binary);

    std::ofstream trade(
        trade_path,
        std::ios::binary);

    std::ofstream reaction_levels(
        reaction_levels_path,
        std::ios::binary);

    std::ofstream oos_reaction_path(
        oos_reaction_path_path,
        std::ios::binary);

    std::ofstream failures(
        failures_path,
        std::ios::binary);

    if (!instrument || !trade || !reaction_levels || !oos_reaction_path || !failures) {
        std::cerr
            << "BLOCK05 FAIL - CANNOT_OPEN_OUTPUTS\n";
        return 4;
    }

    instrument
        << "Symbol;Timeframe;OOSCandidateFormations;OOSAcceptedZones;"
        << "OOSRejectedGap;Resolved;Reaction;Breakout;ReactionPct;"
        << "CI95LowPct;CI95HighPct;OtherLifecycle\n";

    trade
        << "Symbol;Timeframe;ClosedTrades;NetPositive;NetNegative;Breakeven;"
        << "NetPositivePct;GrossProfitPoints;GrossLossPointsAbs;NetPoints;"
        << "ProfitFactorPoints;AvgNetPoints;AvgReturnPct\n";

    reaction_levels
        << "Symbol;Timeframe;TouchesMeasured"
        << ";Hit100;Pct100"
        << ";Hit150;Pct150"
        << ";Hit200;Pct200"
        << ";Hit250;Pct250"
        << ";Hit300;Pct300"
        << ";Hit350;Pct350"
        << ";Hit400;Pct400"
        << ";Hit500;Pct500\n";

    oos_reaction_path
        << "Symbol;Timeframe;TargetPoints;TouchesMeasured;Reached;ReachedPct;"
        << "OneBar;OneBarPctOfReached;AvgBarsToTarget;AvgMAEPoints;"
        << "MAE0;MAE0Pct;MAE1_50;MAE1_50Pct;MAE51_100;MAE51_100Pct;"
        << "MAE101_150;MAE101_150Pct;MAE151_200;MAE151_200Pct;"
        << "MAE201_300;MAE201_300Pct;MAEGT300;MAEGT300Pct\n";

    failures << "File;Reason\n";

    const auto files =
        find_bin_files(data_root);

    if (files.empty()) {
        std::cerr
            << "BLOCK05 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    Totals total;
    total.bin_files_scanned = files.size();
    te::TradeStats global_trade;
    rl::Stats global_reaction_levels;
    rl::PathStats global_reaction_path;

    std::cout
        << "============================================================\n"
        << "RZA CANONICAL BLOCK 05 - FINAL OOS 2024+\n"
        << "OOS_START=2024-01-01T00:00:00Z\n"
        << "TEST_UNIT=SYMBOL+TIMEFRAME\n"
        << "FORMATION_LOGIC=ABS_TRACK_EXACT_PRIORITY\n"
        << "ZONE_POLICY=ABS_TRACK_V2\n"
        << "SAMPLING=OFF\n"
        << "FULL_PRE_OOS_REPLAY=ON\n"
        << "EVENT_CSV=OFF\n"
        << "============================================================\n";

    std::string cached_symbol;
    ap::AtrLookup cached_atr;
    bool cached_atr_available = false;

    std::map<std::string, SeenSeries> seen_series;

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
                ++total.xfbar_files_failed;
                failures
                    << csv_field(files[file_idx]) << ';'
                    << csv_field(data.error) << '\n';
            }
            continue;
        }

        ++total.xfbar_files_passed;

        if (!(data.point > 0.0)) {
            ++total.xfbar_files_failed;
            failures
                << csv_field(files[file_idx]) << ';'
                << "nonpositive_point\n";
            continue;
        }

        const std::string series_key =
            data.symbol + "|" +
            std::to_string(data.period_seconds);

        const std::uint64_t fingerprint =
            series_fingerprint(data);

        const auto seen_it =
            seen_series.find(series_key);

        if (seen_it != seen_series.end()) {
            if (seen_it->second.fingerprint == fingerprint) {
                ++total.duplicate_identical_series;
                continue;
            }

            ++total.duplicate_conflict_series;
            failures
                << csv_field(files[file_idx]) << ';'
                << "duplicate_symbol_timeframe_conflict_with="
                << csv_field(seen_it->second.file) << '\n';
            continue;
        }

        seen_series.emplace(
            series_key,
            SeenSeries{fingerprint, files[file_idx]});

        if (data.bars.size() < 4) {
            continue;
        }

        if (data.symbol != cached_symbol) {
            cached_symbol = data.symbol;
            cached_atr = ap::AtrLookup{};
            cached_atr_available =
                load_symbol_m5_atr(
                    files[file_idx],
                    data,
                    cached_atr);
        }

        if (!cached_atr_available) {
            ++total.series_excluded_no_m5_atr;
            failures
                << csv_field(files[file_idx]) << ';'
                << "excluded_no_m5_atr_for_abs_track_gap\n";
            continue;
        }

        const ap::CloseIndex close_index(
            data.bars);

        const rl::HighLowIndex high_low_index(
            data.bars);

        ap::ActiveZones active;

        std::uint64_t oos_candidates = 0;
        std::uint64_t oos_accepted = 0;
        std::uint64_t oos_rejected_gap = 0;
        Stats stats;
        te::TradeStats trade_stats;
        rl::Stats reaction_level_stats;
        rl::PathStats reaction_path_stats;

        bool has_oos_calendar = false;

        for (std::size_t confirm_idx = 0;
             confirm_idx + 1 < data.bars.size();
             ++confirm_idx)
        {
            const std::int64_t decision_time =
                data.bars[confirm_idx + 1].time;

            const bool is_oos =
                decision_time >= OOS_START_UTC;

            if (is_oos) {
                has_oos_calendar = true;
            }

            active.expire(confirm_idx);

            const auto e_opt =
                detect_at(
                    data.bars,
                    confirm_idx);

            if (!e_opt.has_value()) {
                continue;
            }

            FormationEvent e = *e_opt;
            ++total.candidate_total;

            if (is_oos) {
                ++oos_candidates;
                ++total.candidate_oos;
            }

            const ap::ZoneBounds z =
                ap::calculate_zone_bounds(
                    data.bars[e.source_index],
                    e.direction,
                    data.point);

            e.zone_low = z.low;
            e.zone_high = z.high;

            const double atr_value =
                cached_atr.at_decision(
                    decision_time);

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
                    ++oos_rejected_gap;
                    ++total.rejected_gap_oos;
                }
                continue;
            }

            const std::size_t break_idx =
                ap::zone_break_index(
                    close_index,
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

            ++total.accepted_total;

            if (!is_oos) {
                continue;
            }

            ++oos_accepted;
            ++stats.accepted;
            ++total.accepted_oos;
            ++total.oos.accepted;

            const Evaluation eval =
                evaluate_lifecycle(
                    data,
                    close_index,
                    e);

            count_result(stats, eval.result);
            count_result(total.oos, eval.result);

            if (eval.touch_index != ap::CloseIndex::npos()) {
                reaction_level_stats.add(
                    rl::max_reaction_points_after_touch(
                        high_low_index,
                        eval.touch_index,
                        break_idx,
                        data.bars.size(),
                        e.direction,
                        e.zone_low,
                        e.zone_high,
                        data.point));

                rl::measure_target_paths_after_touch(
                    high_low_index,
                    eval.touch_index,
                    break_idx,
                    data.bars.size(),
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    data.point,
                    reaction_path_stats);
            }

            if (eval.outcome_index != ap::CloseIndex::npos()) {
                trade_stats.add(
                    te::execute_after_close_signals(
                        data.bars,
                        eval.touch_index,
                        eval.outcome_index,
                        e.direction,
                        data.point));
            }
        }

        global_trade.closed += trade_stats.closed;
        global_trade.positive += trade_stats.positive;
        global_trade.negative += trade_stats.negative;
        global_trade.breakeven += trade_stats.breakeven;
        global_trade.sum_return_pct += trade_stats.sum_return_pct;
        global_reaction_levels.merge(reaction_level_stats);
        global_reaction_path.merge(reaction_path_stats);

        ++total.series_replayed;

        if (!has_oos_calendar) {
            continue;
        }

        ++total.series_with_oos;

        reaction_levels
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << reaction_level_stats.touches;

        for (std::size_t i = 0;
             i < rl::kLevelsPoints.size();
             ++i)
        {
            reaction_levels
                << ';'
                << reaction_level_stats.reached[i]
                << ';';

            if (reaction_level_stats.touches > 0) {
                reaction_levels
                    << std::fixed << std::setprecision(9)
                    << reaction_level_stats.pct(i);
            }
        }

        reaction_levels << '\n';

        for (std::size_t target_i = 0;
             target_i < rl::kLevelsPoints.size();
             ++target_i)
        {
            const auto& ps =
                reaction_path_stats.target[target_i];

            oos_reaction_path
                << csv_field(data.symbol) << ';'
                << timeframe_name(data.period_seconds) << ';'
                << rl::kLevelsPoints[target_i] << ';'
                << reaction_path_stats.touches << ';'
                << ps.reached << ';';

            if (reaction_path_stats.touches > 0) {
                oos_reaction_path
                    << std::fixed << std::setprecision(9)
                    << reaction_path_stats.reached_pct(target_i);
            }

            oos_reaction_path
                << ';'
                << ps.one_bar
                << ';';

            if (ps.reached > 0) {
                oos_reaction_path
                    << std::fixed << std::setprecision(9)
                    << ps.one_bar_pct()
                    << ';'
                    << ps.avg_bars()
                    << ';'
                    << ps.avg_mae();

                for (std::size_t bucket_i = 0;
                     bucket_i < rl::kMaeBucketCount;
                     ++bucket_i)
                {
                    oos_reaction_path
                        << ';'
                        << ps.mae_buckets[bucket_i]
                        << ';'
                        << ps.mae_bucket_pct(bucket_i);
                }
            } else {
                oos_reaction_path << ";;;;;;;;;;;;;;;;";
            }

            oos_reaction_path << '\n';
        }

        trade
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << trade_stats.closed << ';'
            << trade_stats.positive << ';'
            << trade_stats.negative << ';'
            << trade_stats.breakeven << ';';

        if (trade_stats.closed > 0) {
            trade
                << std::fixed << std::setprecision(9)
                << trade_stats.positive_pct() << ';'
                << static_cast<double>(trade_stats.gross_profit_points) << ';'
                << static_cast<double>(trade_stats.gross_loss_points_abs) << ';'
                << static_cast<double>(trade_stats.net_points) << ';'
                << trade_stats.profit_factor_points() << ';'
                << trade_stats.avg_net_points() << ';'
                << trade_stats.avg_return_pct();
        } else {
            trade << ";;;;;;";
        }
        trade << '\n';

        instrument
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << oos_candidates << ';'
            << oos_accepted << ';'
            << oos_rejected_gap << ';'
            << stats.resolved() << ';'
            << stats.reaction << ';'
            << stats.breakout << ';';

        write_rate_fields(
            instrument,
            stats);

        instrument
            << ';'
            << stats.other()
            << '\n';

        if ((file_idx + 1) % 25 == 0 ||
            file_idx + 1 == files.size())
        {
            std::cout
                << '[' << (file_idx + 1)
                << '/' << files.size() << "] "
                << data.symbol << '_'
                << timeframe_name(data.period_seconds)
                << " oos_accepted="
                << oos_accepted
                << " resolved="
                << stats.resolved()
                << " reaction="
                << stats.reaction
                << " breakout="
                << stats.breakout
                << '\n';
        }
    }

    instrument.close();
    trade.close();
    reaction_levels.close();
    oos_reaction_path.close();
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
        wilson95_pct(total.oos);

    summary
        << "RZA CANONICAL BLOCK 05 - FINAL OOS 2024+\n"
        << "OOS_START_UTC=2024-01-01T00:00:00Z\n"
        << "TEST_UNIT=SYMBOL+TIMEFRAME\n"
        << "FORMATION_LOGIC=ABS_TRACK_EXACT_PRIORITY\n"
        << "ZONE_POLICY=ABS_TRACK_V2\n"
        << "SAMPLING=OFF\n"
        << "FULL_PRE_OOS_REPLAY=1\n"
        << "EVENT_CSV=OFF\n"
        << "FORMATION_TYPE_USED_FOR_SPLIT=0\n"
        << "PROGRESS_USED=0\n"
        << "MIN_ZONE_HEIGHT_POINTS=225\n"
        << "MIN_GAP_POINTS=20\n"
        << "MIN_GAP_ATR=0.30\n"
        << "DISTANCE_ATR_TIMEFRAME=M5\n"
        << "DISTANCE_ATR_PERIOD=14\n"
        << "DELETION_THRESHOLD_POINTS=10\n"
        << "HISTORICAL_SPREAD_POINTS=30\n"
        << "BIN_FILES_SCANNED="
        << total.bin_files_scanned << '\n'
        << "XFBAR_FILES_PASSED="
        << total.xfbar_files_passed << '\n'
        << "XFBAR_FILES_FAILED="
        << total.xfbar_files_failed << '\n'
        << "SKIPPED_NON_XFBAR="
        << total.skipped_non_xfbar << '\n'
        << "SERIES_REPLAYED="
        << total.series_replayed << '\n'
        << "SERIES_WITH_OOS="
        << total.series_with_oos << '\n'
        << "SERIES_EXCLUDED_NO_M5_ATR="
        << total.series_excluded_no_m5_atr << '\n'
        << "DUPLICATE_IDENTICAL_SERIES="
        << total.duplicate_identical_series << '\n'
        << "DUPLICATE_CONFLICT_SERIES="
        << total.duplicate_conflict_series << '\n'
        << "CANDIDATE_TOTAL_REPLAYED="
        << total.candidate_total << '\n'
        << "ACCEPTED_TOTAL_REPLAYED="
        << total.accepted_total << '\n'
        << "REJECTED_GAP_TOTAL_REPLAYED="
        << total.rejected_gap_total << '\n'
        << "OOS_CANDIDATE_FORMATIONS="
        << total.candidate_oos << '\n'
        << "OOS_ACCEPTED_ZONES="
        << total.accepted_oos << '\n'
        << "OOS_REJECTED_GAP="
        << total.rejected_gap_oos << '\n'
        << "OOS_RESOLVED_TOUCH_OUTCOMES="
        << total.oos.resolved() << '\n'
        << "OOS_REACTION_FIRST="
        << total.oos.reaction << '\n'
        << "OOS_BREAKOUT_FIRST="
        << total.oos.breakout << '\n'
        << "OOS_OTHER_LIFECYCLE="
        << total.oos.other() << '\n'
        << "TRADE_EMULATION=ON\n"
        << "TRADE_ENTRY=NEXT_BAR_OPEN_AFTER_TOUCH_CLOSE\n"
        << "TRADE_EXIT=NEXT_BAR_OPEN_AFTER_OUTCOME_CLOSE\n"
        << "TRADE_SPREAD=HISTORICAL_XFBAR\n"
        << "TRADE_COMMISSION=0\n"
        << "TRADE_SLIPPAGE=0\n"
        << "TRADE_SWAP=0\n"
        << "OOS_TRADE_CLOSED=" << global_trade.closed << '\n'
        << "OOS_TRADE_NET_POSITIVE=" << global_trade.positive << '\n'
        << "OOS_TRADE_NET_NEGATIVE=" << global_trade.negative << '\n'
        << "OOS_TRADE_BREAKEVEN=" << global_trade.breakeven << '\n'
        << "REACTION_LEVEL_MEASURE=MAX_FAVORABLE_EXCURSION_FROM_OUTER_ZONE_EDGE\n"
        << "REACTION_LEVEL_START=NEXT_BAR_AFTER_TOUCH_CLOSE\n"
        << "REACTION_LEVEL_END=ABS_TRACK_ZONE_DELETION_OR_DATA_END\n"
        << "OOS_REACTION_LEVEL_TOUCHES=" << global_reaction_levels.touches << '\n';

    for (std::size_t i = 0;
         i < rl::kLevelsPoints.size();
         ++i)
    {
        summary
            << "OOS_REACTION_REACHED_"
            << rl::kLevelsPoints[i]
            << "_POINTS="
            << global_reaction_levels.reached[i]
            << '\n';

        if (global_reaction_levels.touches > 0) {
            summary
                << std::fixed << std::setprecision(9)
                << "OOS_REACTION_REACHED_"
                << rl::kLevelsPoints[i]
                << "_PCT="
                << global_reaction_levels.pct(i)
                << '\n';
        }
    }

    summary
        << "REACTION_PATH_MAE_REFERENCE=OUTER_ZONE_EDGE\n"
        << "REACTION_PATH_MAE_WINDOW=FULLY_COMPLETED_BARS_BEFORE_FIRST_TARGET_HIT_BAR\n"
        << "REACTION_PATH_HIT_BAR_EXCLUDED_FROM_MAE=1\n";

    for (std::size_t target_i = 0;
         target_i < rl::kLevelsPoints.size();
         ++target_i)
    {
        const auto& ps =
            global_reaction_path.target[target_i];

        summary
            << "REACTION_PATH_TARGET_"
            << rl::kLevelsPoints[target_i]
            << "_REACHED="
            << ps.reached
            << '\n';

        if (ps.reached > 0) {
            summary
                << std::fixed << std::setprecision(9)
                << "REACTION_PATH_TARGET_"
                << rl::kLevelsPoints[target_i]
                << "_ONE_BAR_PCT="
                << ps.one_bar_pct()
                << '\n'
                << "REACTION_PATH_TARGET_"
                << rl::kLevelsPoints[target_i]
                << "_AVG_BARS="
                << ps.avg_bars()
                << '\n'
                << "REACTION_PATH_TARGET_"
                << rl::kLevelsPoints[target_i]
                << "_AVG_MAE_POINTS="
                << ps.avg_mae()
                << '\n';
        }
    }

    if (global_trade.closed > 0) {
        summary
            << std::fixed << std::setprecision(9)
            << "OOS_TRADE_NET_POSITIVE_PCT="
            << global_trade.positive_pct() << '\n'
            << "OOS_TRADE_AVG_RETURN_PCT="
            << global_trade.avg_return_pct() << '\n';
    }

    if (total.oos.resolved() > 0) {
        summary
            << std::fixed << std::setprecision(9)
            << "OOS_REACTION_PCT="
            << reaction_rate_pct(total.oos) << '\n'
            << "OOS_REACTION_CI95_LOW_PCT="
            << overall_ci.first << '\n'
            << "OOS_REACTION_CI95_HIGH_PCT="
            << overall_ci.second << '\n';
    } else {
        summary
            << "OOS_REACTION_PCT=NA\n"
            << "OOS_REACTION_CI95_LOW_PCT=NA\n"
            << "OOS_REACTION_CI95_HIGH_PCT=NA\n";
    }

    summary
        << "\nCONTRACT:\n"
        << "- Full history is replayed causally so pre-2024 live zones seed the 2024+ state.\n"
        << "- Every unique symbol+timeframe series is tested separately.\n"
        << "- Exact duplicate series are counted once; conflicting duplicates make the block fail.\n"
        << "- A series without M5 ATR is excluded rather than tested with a reduced rule.\n"
        << "- Every accepted OOS zone is evaluated; there is no 1/64 sampling.\n"
        << "- OOS instrument statistics are written to 05_OOS_INSTRUMENT_STATS.csv.\n"
        << "- No parameter is changed or optimized using OOS data.\n"
        << "- This is a structural reaction/breakout test, not a PnL test.\n";

    summary.close();

    std::cout
        << "------------------------------------------------------------\n"
        << "SERIES_REPLAYED="
        << total.series_replayed << '\n'
        << "SERIES_WITH_OOS="
        << total.series_with_oos << '\n'
        << "SERIES_EXCLUDED_NO_M5_ATR="
        << total.series_excluded_no_m5_atr << '\n'
        << "DUPLICATE_IDENTICAL_SERIES="
        << total.duplicate_identical_series << '\n'
        << "DUPLICATE_CONFLICT_SERIES="
        << total.duplicate_conflict_series << '\n'
        << "OOS_CANDIDATE_FORMATIONS="
        << total.candidate_oos << '\n'
        << "OOS_ACCEPTED_ZONES="
        << total.accepted_oos << '\n'
        << "OOS_REJECTED_GAP="
        << total.rejected_gap_oos << '\n'
        << "OOS_RESOLVED_TOUCH_OUTCOMES="
        << total.oos.resolved() << '\n'
        << "OOS_REACTION_FIRST="
        << total.oos.reaction << '\n'
        << "OOS_BREAKOUT_FIRST="
        << total.oos.breakout << '\n';

    if (total.oos.resolved() > 0) {
        std::cout
            << std::fixed << std::setprecision(6)
            << "OOS_REACTION_PCT="
            << reaction_rate_pct(total.oos)
            << '\n';
    }

    std::cout
        << "OOS_INSTRUMENT_STATS="
        << instrument_path.string() << '\n'
        << "OOS_TRADE_EMULATION="
        << trade_path.string() << '\n'
        << "OOS_REACTION_LEVELS="
        << reaction_levels_path.string() << '\n'
        << "OOS_REACTION_PATH="
        << oos_reaction_path_path.string() << '\n'
        << "SUMMARY="
        << summary_path.string() << '\n';

    if (total.xfbar_files_failed != 0) {
        std::cout
            << "BLOCK05 FAIL - INVALID_XFBAR_FILES="
            << total.xfbar_files_failed << '\n';
        return 7;
    }

    if (total.duplicate_conflict_series != 0) {
        std::cout
            << "BLOCK05 FAIL - DUPLICATE_SERIES_CONFLICT="
            << total.duplicate_conflict_series << '\n';
        return 8;
    }

    if (total.series_with_oos == 0 ||
        total.accepted_oos == 0 ||
        total.oos.resolved() == 0)
    {
        std::cout
            << "BLOCK05 FAIL - ZERO_TESTABLE_OOS_RESULT\n";
        return 9;
    }

    std::cout << "BLOCK05 PASS\n";
    return 0;
}
