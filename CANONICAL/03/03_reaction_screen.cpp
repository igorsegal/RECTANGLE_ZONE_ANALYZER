#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"
#include "../trade_emulator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;
namespace te = rza::canonical::trade_emulation;

namespace {

constexpr std::int64_t DEV_CUTOFF_UTC = 1704067200LL;

enum class LifecycleResult {
    NO_OUTSIDE_BEFORE_CUTOFF,
    BROKEN_BEFORE_EXPECTED_DEPARTURE,
    NO_RETURN_BEFORE_CUTOFF,
    DIRECT_BREAKOUT_NO_CLOSE_TOUCH,
    REACTION_FIRST,
    BREAKOUT_FIRST,
    TOUCH_UNRESOLVED_AT_CUTOFF
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
    std::uint64_t series_tested = 0;
    std::uint64_t series_excluded_no_m5_atr = 0;
    std::uint64_t duplicate_identical_series = 0;
    std::uint64_t duplicate_conflict_series = 0;

    std::uint64_t candidate_formations = 0;
    std::uint64_t accepted_zones = 0;
    std::uint64_t rejected_gap = 0;

    Stats lifecycle;
};

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

double reaction_rate_pct(const Stats& s) {
    if (s.resolved() == 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return 100.0 *
        static_cast<double>(s.reaction) /
        static_cast<double>(s.resolved());
}

std::pair<double,double> wilson95_pct(const Stats& s) {
    const double n = static_cast<double>(s.resolved());

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
        case LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF:
            ++s.no_outside;
            break;
        case LifecycleResult::BROKEN_BEFORE_EXPECTED_DEPARTURE:
            ++s.broken_before_departure;
            break;
        case LifecycleResult::NO_RETURN_BEFORE_CUTOFF:
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
        case LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF:
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

    h = mix_u64(h, static_cast<std::uint64_t>(
        static_cast<std::uint32_t>(data.period_seconds)));
    h = mix_u64(h, double_bits(data.point));
    h = mix_u64(h, static_cast<std::uint64_t>(data.bars.size()));

    for (const Bar& b : data.bars) {
        h = mix_u64(h, static_cast<std::uint64_t>(b.time));
        h = mix_u64(h, double_bits(b.open));
        h = mix_u64(h, double_bits(b.high));
        h = mix_u64(h, double_bits(b.low));
        h = mix_u64(h, double_bits(b.close));
    }

    return h;
}

struct SeenSeries {
    std::uint64_t fingerprint = 0;
    std::string file;
};

std::size_t development_end_exclusive(const XfbarData& data) {
    const auto it = std::lower_bound(
        data.bars.begin(),
        data.bars.end(),
        DEV_CUTOFF_UTC,
        [](const Bar& bar, std::int64_t cutoff) {
            return bar.time < cutoff;
        });

    return static_cast<std::size_t>(
        std::distance(data.bars.begin(), it));
}

Evaluation evaluate_lifecycle(
    const XfbarData& data,
    const ap::CloseIndex& index,
    const FormationEvent& e,
    std::size_t dev_end)
{
    const std::size_t begin =
        e.confirmation_index + 1;

    const bool bullish =
        e.direction == Direction::BULLISH;

    const std::size_t first_out =
        index.first_outside(
            begin,
            dev_end,
            e.zone_low,
            e.zone_high);

    if (first_out == ap::CloseIndex::npos()) {
        return {LifecycleResult::NO_OUTSIDE_BEFORE_CUTOFF};
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
                  dev_end,
                  e.zone_high)
            : index.first_ge(
                  first_out + 1,
                  dev_end,
                  e.zone_low);

    if (return_idx == ap::CloseIndex::npos()) {
        return {LifecycleResult::NO_RETURN_BEFORE_CUTOFF};
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
            dev_end,
            e.zone_low,
            e.zone_high);

    if (outcome == ap::CloseIndex::npos()) {
        return {LifecycleResult::TOUCH_UNRESOLVED_AT_CUTOFF, return_idx, ap::CloseIndex::npos()};
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
            : fs::path("../03out");

    std::error_code ec;

    if (!fs::exists(data_root, ec) ||
        !fs::is_directory(data_root, ec))
    {
        std::cerr
            << "BLOCK03 FAIL - DATA_ROOT_NOT_FOUND\n";
        return 2;
    }

    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr
            << "BLOCK03 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 3;
    }

    const fs::path instrument_path =
        out_root / "03_INSTRUMENT_STATS.csv";

    const fs::path trade_path =
        out_root / "03_TRADE_EMULATION.csv";

    const fs::path failures_path =
        out_root / "03_FAILURES.csv";

    const fs::path summary_path =
        out_root / "03_SUMMARY.txt";

    std::ofstream instrument(
        instrument_path,
        std::ios::binary);

    std::ofstream trade(
        trade_path,
        std::ios::binary);

    std::ofstream failures(
        failures_path,
        std::ios::binary);

    if (!instrument || !trade || !failures) {
        std::cerr
            << "BLOCK03 FAIL - CANNOT_OPEN_OUTPUTS\n";
        return 4;
    }

    instrument
        << "Symbol;Timeframe;CandidateFormations;AcceptedZones;"
        << "RejectedGap;Resolved;Reaction;Breakout;ReactionPct;"
        << "CI95LowPct;CI95HighPct;OtherLifecycle;M5AtrAvailable\n";

    trade
        << "Symbol;Timeframe;ClosedTrades;NetPositive;NetNegative;Breakeven;"
        << "NetPositivePct;GrossProfitPoints;GrossLossPointsAbs;NetPoints;"
        << "ProfitFactorPoints;AvgNetPoints;AvgReturnPct\n";

    failures << "File;Reason\n";

    const auto files =
        find_bin_files(data_root);

    if (files.empty()) {
        std::cerr
            << "BLOCK03 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    Totals total;
    total.bin_files_scanned = files.size();
    te::TradeStats global_trade;

    std::cout
        << "============================================================\n"
        << "RZA CANONICAL BLOCK 03 - FULL PER-INSTRUMENT TEST\n"
        << "DEV_CUTOFF=2024-01-01T00:00:00Z\n"
        << "TEST_UNIT=SYMBOL+TIMEFRAME\n"
        << "FORMATION_LOGIC=ABS_TRACK_EXACT_PRIORITY\n"
        << "ZONE_POLICY=ABS_TRACK_V2\n"
        << "SAMPLING=OFF\n"
        << "EVENT_CSV=OFF\n"
        << "RESULT=REACTION_VS_BREAKOUT_ON_ALL_ACCEPTED_ZONES\n"
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
            data.symbol + "|" + std::to_string(data.period_seconds);

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

        const std::size_t dev_end =
            development_end_exclusive(data);

        if (dev_end < 4) {
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

        ap::ActiveZones active;

        std::uint64_t candidates = 0;
        std::uint64_t accepted = 0;
        std::uint64_t rejected_gap = 0;
        Stats stats;
        te::TradeStats trade_stats;

        for (std::size_t confirm_idx = 0;
             confirm_idx + 1 < dev_end;
             ++confirm_idx)
        {
            // Exact ABS_TRACK decision timing:
            // a formation on confirm_idx becomes actionable at the opening
            // time of the next real bar, not at synthetic time+period.
            const std::int64_t decision_time =
                data.bars[confirm_idx + 1].time;

            active.expire(confirm_idx);

            const auto e_opt =
                detect_at(
                    data.bars,
                    confirm_idx);

            if (!e_opt.has_value()) {
                continue;
            }

            FormationEvent e = *e_opt;
            ++candidates;
            ++total.candidate_formations;

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
                ++rejected_gap;
                ++total.rejected_gap;
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

            ++accepted;
            ++stats.accepted;
            ++total.accepted_zones;
            ++total.lifecycle.accepted;

            const Evaluation eval =
                evaluate_lifecycle(
                    data,
                    close_index,
                    e,
                    dev_end);

            count_result(stats, eval.result);
            count_result(total.lifecycle, eval.result);

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

        ++total.series_tested;

        instrument
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << candidates << ';'
            << accepted << ';'
            << rejected_gap << ';'
            << stats.resolved() << ';'
            << stats.reaction << ';'
            << stats.breakout << ';';

        write_rate_fields(
            instrument,
            stats);

        instrument
            << ';'
            << stats.other() << ';'
            << (cached_atr_available ? 1 : 0)
            << '\n';

        if ((file_idx + 1) % 25 == 0 ||
            file_idx + 1 == files.size())
        {
            std::cout
                << '[' << (file_idx + 1)
                << '/' << files.size() << "] "
                << data.symbol << '_'
                << timeframe_name(
                    data.period_seconds)
                << " accepted="
                << accepted
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
    failures.close();

    std::ofstream summary(
        summary_path,
        std::ios::binary);

    if (!summary) {
        std::cerr
            << "BLOCK03 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 6;
    }

    const auto overall_ci =
        wilson95_pct(total.lifecycle);

    summary
        << "RZA CANONICAL BLOCK 03 - FULL PER-INSTRUMENT TEST\n"
        << "DEV_CUTOFF_UTC=2024-01-01T00:00:00Z\n"
        << "TEST_UNIT=SYMBOL+TIMEFRAME\n"
        << "FORMATION_LOGIC=ABS_TRACK_EXACT_PRIORITY\n"
        << "ZONE_POLICY=ABS_TRACK_V2\n"
        << "SAMPLING=OFF\n"
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
        << "SERIES_TESTED="
        << total.series_tested << '\n'
        << "SERIES_EXCLUDED_NO_M5_ATR="
        << total.series_excluded_no_m5_atr << '\n'
        << "DUPLICATE_IDENTICAL_SERIES="
        << total.duplicate_identical_series << '\n'
        << "DUPLICATE_CONFLICT_SERIES="
        << total.duplicate_conflict_series << '\n'
        << "CANDIDATE_FORMATIONS="
        << total.candidate_formations << '\n'
        << "ACCEPTED_ZONES="
        << total.accepted_zones << '\n'
        << "REJECTED_GAP="
        << total.rejected_gap << '\n'
        << "RESOLVED_TOUCH_OUTCOMES="
        << total.lifecycle.resolved() << '\n'
        << "REACTION_FIRST="
        << total.lifecycle.reaction << '\n'
        << "BREAKOUT_FIRST="
        << total.lifecycle.breakout << '\n'
        << "OTHER_LIFECYCLE="
        << total.lifecycle.other() << '\n'
        << "TRADE_EMULATION=ON\n"
        << "TRADE_ENTRY=NEXT_BAR_OPEN_AFTER_TOUCH_CLOSE\n"
        << "TRADE_EXIT=NEXT_BAR_OPEN_AFTER_OUTCOME_CLOSE\n"
        << "TRADE_SPREAD=HISTORICAL_XFBAR\n"
        << "TRADE_COMMISSION=0\n"
        << "TRADE_SLIPPAGE=0\n"
        << "TRADE_SWAP=0\n"
        << "TRADE_CLOSED=" << global_trade.closed << '\n'
        << "TRADE_NET_POSITIVE=" << global_trade.positive << '\n'
        << "TRADE_NET_NEGATIVE=" << global_trade.negative << '\n'
        << "TRADE_BREAKEVEN=" << global_trade.breakeven << '\n';

    if (global_trade.closed > 0) {
        summary
            << std::fixed << std::setprecision(9)
            << "TRADE_NET_POSITIVE_PCT="
            << global_trade.positive_pct() << '\n'
            << "TRADE_AVG_RETURN_PCT="
            << global_trade.avg_return_pct() << '\n';
    }

    if (total.lifecycle.resolved() > 0) {
        summary
            << std::fixed << std::setprecision(9)
            << "REACTION_PCT="
            << reaction_rate_pct(total.lifecycle) << '\n'
            << "REACTION_CI95_LOW_PCT="
            << overall_ci.first << '\n'
            << "REACTION_CI95_HIGH_PCT="
            << overall_ci.second << '\n';
    } else {
        summary
            << "REACTION_PCT=NA\n"
            << "REACTION_CI95_LOW_PCT=NA\n"
            << "REACTION_CI95_HIGH_PCT=NA\n";
    }

    summary
        << "\nCONTRACT:\n"
        << "- 3149-style counts are database file metadata, not event counts.\n"
        << "- Every unique symbol+timeframe series is tested separately.\n"
        << "- Exact duplicate series are counted once; conflicting duplicates make the block fail.\n"
        << "- A series without M5 ATR cannot reproduce ABS_TRACK gap logic and is excluded explicitly.\n"
        << "- Only zones accepted by the ABS_TRACK_v2 bounds/gap/deletion policy enter the reaction test.\n"
        << "- All accepted development zones are evaluated; there is no 1/64 sample.\n"
        << "- Instrument statistics are written to 03_INSTRUMENT_STATS.csv.\n"
        << "- The old 61.01% result is obsolete and must not be used.\n"
        << "- 2024+ remains untouched for final OOS.\n";

    summary.close();

    std::cout
        << "------------------------------------------------------------\n"
        << "BIN_FILES_SCANNED="
        << total.bin_files_scanned << '\n'
        << "SERIES_TESTED="
        << total.series_tested << '\n'
        << "SERIES_EXCLUDED_NO_M5_ATR="
        << total.series_excluded_no_m5_atr << '\n'
        << "DUPLICATE_IDENTICAL_SERIES="
        << total.duplicate_identical_series << '\n'
        << "DUPLICATE_CONFLICT_SERIES="
        << total.duplicate_conflict_series << '\n'
        << "CANDIDATE_FORMATIONS="
        << total.candidate_formations << '\n'
        << "ACCEPTED_ZONES="
        << total.accepted_zones << '\n'
        << "REJECTED_GAP="
        << total.rejected_gap << '\n'
        << "RESOLVED_TOUCH_OUTCOMES="
        << total.lifecycle.resolved() << '\n'
        << "REACTION_FIRST="
        << total.lifecycle.reaction << '\n'
        << "BREAKOUT_FIRST="
        << total.lifecycle.breakout << '\n';

    if (total.lifecycle.resolved() > 0) {
        std::cout
            << std::fixed << std::setprecision(6)
            << "REACTION_PCT="
            << reaction_rate_pct(total.lifecycle)
            << '\n';
    }

    std::cout
        << "INSTRUMENT_STATS="
        << instrument_path.string() << '\n'
        << "TRADE_EMULATION="
        << trade_path.string() << '\n'
        << "SUMMARY="
        << summary_path.string() << '\n';

    if (total.xfbar_files_failed != 0) {
        std::cout
            << "BLOCK03 FAIL - INVALID_XFBAR_FILES="
            << total.xfbar_files_failed << '\n';
        return 7;
    }

    if (total.duplicate_conflict_series != 0) {
        std::cout
            << "BLOCK03 FAIL - DUPLICATE_SERIES_CONFLICT="
            << total.duplicate_conflict_series << '\n';
        return 8;
    }

    if (total.series_tested == 0 ||
        total.accepted_zones == 0 ||
        total.lifecycle.resolved() == 0)
    {
        std::cout
            << "BLOCK03 FAIL - ZERO_TESTABLE_RESULT\n";
        return 9;
    }

    std::cout << "BLOCK03 PASS\n";
    return 0;
}
