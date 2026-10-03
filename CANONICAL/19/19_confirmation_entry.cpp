#include "../01/formation_detector.h"
#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;

namespace {

constexpr std::int32_t H1_SECONDS = 3600;
constexpr std::int32_t M5_SECONDS = 300;
constexpr int TP_POINTS = 89;
constexpr int SL_POINTS = 233;

const std::array<std::string,10> BASKET{{
    "DJ30","NAS100","GER40","SP500","US2000",
    "XAUUSD","XAUAUD","XAUJPY","XPDUSD","XPTUSD"
}};

enum class Outcome {
    TP,
    SL,
    AMBIGUOUS_AS_SL,
    CENSORED,
    INVALID
};

const char* outcome_name(Outcome x) {
    switch (x) {
        case Outcome::TP: return "TP";
        case Outcome::SL: return "SL";
        case Outcome::AMBIGUOUS_AS_SL: return "SL_AMBIGUOUS";
        case Outcome::CENSORED: return "CENSORED";
        default: return "INVALID";
    }
}

const char* formation_name(FormationType x) {
    return x == FormationType::ENGULF_2
        ? "ENGULF_2"
        : "ENGULF_3";
}

struct Trade {
    std::string symbol;
    FormationType formation = FormationType::ENGULF_2;
    Direction direction = Direction::BULLISH;

    std::int64_t confirm_time = 0;
    std::int64_t signal_time = 0;
    std::int64_t entry_time = 0;
    std::int64_t exit_time = 0;

    double confirm_close = 0.0;
    double zone_low = 0.0;
    double zone_high = 0.0;
    double point = 0.0;

    double entry = 0.0;
    double tp = 0.0;
    double sl = 0.0;
    double exit = 0.0;

    double entry_gap_points = 0.0;
    double pnl_points = 0.0;
    double r = 0.0;

    int entry_spread_points = 0;
    int exit_spread_points = 0;

    Outcome outcome = Outcome::INVALID;
};

struct Stats {
    std::uint64_t accepted = 0;
    std::uint64_t entered = 0;
    std::uint64_t closed = 0;
    std::uint64_t wins = 0;
    std::uint64_t losses = 0;
    std::uint64_t ambiguous = 0;
    std::uint64_t censored = 0;
    std::uint64_t invalid = 0;

    long double sum_r = 0.0L;
    long double gp_r = 0.0L;
    long double gl_r = 0.0L;
    long double sum_abs_gap_points = 0.0L;

    void add(const Trade& t) {
        if (t.outcome == Outcome::INVALID) {
            ++invalid;
            return;
        }

        ++entered;
        sum_abs_gap_points +=
            std::abs(t.entry_gap_points);

        if (t.outcome == Outcome::CENSORED) {
            ++censored;
            return;
        }

        ++closed;
        sum_r += t.r;

        if (t.outcome == Outcome::TP) {
            ++wins;
            if (t.r > 0.0) gp_r += t.r;
        } else {
            ++losses;
            if (t.outcome == Outcome::AMBIGUOUS_AS_SL)
                ++ambiguous;
            if (t.r < 0.0) gl_r += -t.r;
        }
    }

    double win_rate() const {
        return closed
            ? 100.0 * static_cast<double>(wins) /
                  static_cast<double>(closed)
            : std::numeric_limits<double>::quiet_NaN();
    }

    double avg_r() const {
        return closed
            ? static_cast<double>(
                  sum_r /
                  static_cast<long double>(closed))
            : std::numeric_limits<double>::quiet_NaN();
    }

    double pf_r() const {
        if (gl_r > 0.0L)
            return static_cast<double>(gp_r / gl_r);
        if (gp_r > 0.0L)
            return std::numeric_limits<double>::infinity();
        return std::numeric_limits<double>::quiet_NaN();
    }

    double avg_abs_gap_points() const {
        return entered
            ? static_cast<double>(
                  sum_abs_gap_points /
                  static_cast<long double>(entered))
            : std::numeric_limits<double>::quiet_NaN();
    }
};

std::size_t lower_bound_time(
    const std::vector<Bar>& bars,
    std::int64_t t)
{
    return static_cast<std::size_t>(
        std::lower_bound(
            bars.begin(),
            bars.end(),
            t,
            [](const Bar& b, std::int64_t x) {
                return b.time < x;
            }) - bars.begin());
}

int utc_year(std::int64_t ts) {
    std::time_t tt =
        static_cast<std::time_t>(ts);
    const std::tm* tm =
        std::gmtime(&tt);
    return tm ? tm->tm_year + 1900 : 0;
}

Trade replay_from_confirmation(
    const std::string& symbol,
    FormationType formation,
    Direction direction,
    std::int64_t confirm_time,
    std::int64_t signal_time,
    double confirm_close,
    double zone_low,
    double zone_high,
    const XfbarData& m5)
{
    Trade t;
    t.symbol = symbol;
    t.formation = formation;
    t.direction = direction;
    t.confirm_time = confirm_time;
    t.signal_time = signal_time;
    t.confirm_close = confirm_close;
    t.zone_low = zone_low;
    t.zone_high = zone_high;
    t.point = m5.point;

    if (!(m5.point > 0.0))
        return t;

    const std::size_t entry_index =
        lower_bound_time(
            m5.bars,
            signal_time);

    if (entry_index >= m5.bars.size())
        return t;

    const Bar& eb =
        m5.bars[entry_index];

    if (eb.time >= signal_time + H1_SECONDS)
        return t;

    const bool buy =
        direction == Direction::BULLISH;

    const int entry_spread =
        std::max(0, eb.spread);

    const double entry_spr =
        static_cast<double>(entry_spread) *
        m5.point;

    t.entry_time = eb.time;
    t.entry_spread_points = entry_spread;

    if (buy) {
        t.entry = eb.open + entry_spr;
    } else {
        t.entry = eb.open;
    }

    t.entry_gap_points =
        (t.entry - confirm_close) /
        m5.point;

    t.tp = buy
        ? t.entry +
              static_cast<double>(TP_POINTS) *
                  m5.point
        : t.entry -
              static_cast<double>(TP_POINTS) *
                  m5.point;

    t.sl = buy
        ? t.entry -
              static_cast<double>(SL_POINTS) *
                  m5.point
        : t.entry +
              static_cast<double>(SL_POINTS) *
                  m5.point;

    for (std::size_t i = entry_index;
         i < m5.bars.size();
         ++i)
    {
        const Bar& b =
            m5.bars[i];

        const int spread =
            std::max(0, b.spread);

        const double spr =
            static_cast<double>(spread) *
            m5.point;

        Outcome out =
            Outcome::INVALID;

        double exit_price = 0.0;

        if (buy) {
            if (i > entry_index) {
                if (b.open <= t.sl) {
                    out = Outcome::SL;
                    exit_price = b.open;
                } else if (b.open >= t.tp) {
                    out = Outcome::TP;
                    exit_price = b.open;
                }
            }

            if (out == Outcome::INVALID) {
                const bool hit_tp =
                    b.high >= t.tp;
                const bool hit_sl =
                    b.low <= t.sl;

                if (hit_tp && hit_sl) {
                    out = Outcome::AMBIGUOUS_AS_SL;
                    exit_price = t.sl;
                } else if (hit_sl) {
                    out = Outcome::SL;
                    exit_price = t.sl;
                } else if (hit_tp) {
                    out = Outcome::TP;
                    exit_price = t.tp;
                }
            }
        } else {
            const double ask_open =
                b.open + spr;
            const double ask_high =
                b.high + spr;
            const double ask_low =
                b.low + spr;

            if (i > entry_index) {
                if (ask_open >= t.sl) {
                    out = Outcome::SL;
                    exit_price = ask_open;
                } else if (ask_open <= t.tp) {
                    out = Outcome::TP;
                    exit_price = ask_open;
                }
            }

            if (out == Outcome::INVALID) {
                const bool hit_tp =
                    ask_low <= t.tp;
                const bool hit_sl =
                    ask_high >= t.sl;

                if (hit_tp && hit_sl) {
                    out = Outcome::AMBIGUOUS_AS_SL;
                    exit_price = t.sl;
                } else if (hit_sl) {
                    out = Outcome::SL;
                    exit_price = t.sl;
                } else if (hit_tp) {
                    out = Outcome::TP;
                    exit_price = t.tp;
                }
            }
        }

        if (out != Outcome::INVALID) {
            t.outcome = out;
            t.exit_time = b.time;
            t.exit_spread_points = spread;
            t.exit = exit_price;

            const double pnl_price =
                buy
                    ? t.exit - t.entry
                    : t.entry - t.exit;

            t.pnl_points =
                pnl_price / m5.point;

            t.r =
                pnl_price /
                (static_cast<double>(SL_POINTS) *
                 m5.point);

            return t;
        }
    }

    t.outcome =
        Outcome::CENSORED;

    t.exit_time =
        m5.bars.back().time;

    return t;
}

void write_header(
    std::ostream& out,
    const char* first)
{
    out
        << first
        << ";Accepted"
        << ";Entered"
        << ";Closed"
        << ";Wins"
        << ";Losses"
        << ";WinRatePct"
        << ";AmbiguousAsSL"
        << ";Censored"
        << ";Invalid"
        << ";AvgAbsEntryGapPoints"
        << ";AvgR"
        << ";ProfitFactorR"
        << ";SumR\n";
}

void write_row(
    std::ostream& out,
    const std::string& key,
    const Stats& s)
{
    out
        << key
        << ';'
        << s.accepted
        << ';'
        << s.entered
        << ';'
        << s.closed
        << ';'
        << s.wins
        << ';'
        << s.losses
        << ';'
        << std::setprecision(12)
        << s.win_rate()
        << ';'
        << s.ambiguous
        << ';'
        << s.censored
        << ';'
        << s.invalid
        << ';'
        << s.avg_abs_gap_points()
        << ';'
        << s.avg_r()
        << ';'
        << s.pf_r()
        << ';'
        << static_cast<double>(s.sum_r)
        << '\n';
}

int selftest() {
    int failed = 0;

    auto check =
        [&](bool ok, const char* name)
    {
        std::cout
            << (ok ? "[OK]   " : "[FAIL] ")
            << name
            << '\n';

        if (!ok)
            ++failed;
    };

    XfbarData buy_m5;
    buy_m5.success = true;
    buy_m5.symbol = "XAUUSD";
    buy_m5.period_seconds = M5_SECONDS;
    buy_m5.point = 0.01;
    buy_m5.bars = {
        {3600,100.01,101.10,99.95,100.80,2}
    };

    Trade buy =
        replay_from_confirmation(
            "XAUUSD",
            FormationType::ENGULF_2,
            Direction::BULLISH,
            0,
            3600,
            100.00,
            99.00,
            100.00,
            buy_m5);

    check(
        std::abs(buy.entry - 100.03) < 1e-9,
        "BUY executes next M5 open at Ask");

    check(
        std::abs(buy.entry_gap_points - 3.0) < 1e-9,
        "BUY gap from confirmation close is recorded");

    check(
        buy.outcome == Outcome::TP,
        "BUY TP89 can close from confirmation entry");

    XfbarData sell_m5;
    sell_m5.success = true;
    sell_m5.symbol = "DJ30";
    sell_m5.period_seconds = M5_SECONDS;
    sell_m5.point = 1.0;
    sell_m5.bars = {
        {3600,999,1001,900,910,2}
    };

    Trade sell =
        replay_from_confirmation(
            "DJ30",
            FormationType::ENGULF_3,
            Direction::BEARISH,
            0,
            3600,
            1000,
            990,
            1010,
            sell_m5);

    check(
        sell.entry == 999.0,
        "SELL executes next M5 open at Bid");

    check(
        sell.outcome == Outcome::TP,
        "SELL TP89 is evaluated on Ask");

    XfbarData amb;
    amb.success = true;
    amb.symbol = "XAUUSD";
    amb.period_seconds = M5_SECONDS;
    amb.point = 0.01;
    amb.bars = {
        {3600,100.00,101.50,97.00,100.00,0}
    };

    Trade q =
        replay_from_confirmation(
            "XAUUSD",
            FormationType::ENGULF_2,
            Direction::BULLISH,
            0,
            3600,
            100.00,
            99.00,
            100.00,
            amb);

    check(
        q.outcome ==
            Outcome::AMBIGUOUS_AS_SL,
        "same M5 TP+SL is conservative SL");

    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 &&
        std::string(argv[1]) == "--selftest")
        return selftest();

    const fs::path raw =
        argc >= 2
            ? fs::path(argv[1])
            : fs::path(
                  "D:/AHexaTrader/1DataFiles/raw");

    const fs::path out =
        argc >= 3
            ? fs::path(argv[2])
            : fs::path("../19out");

    std::error_code ec;
    fs::create_directories(out, ec);

    if (ec) {
        std::cerr
            << "BLOCK19 FAIL - "
            << "CANNOT_CREATE_OUTDIR\n";
        return 2;
    }

    std::ofstream trades(
        out / "19_TRADES.csv",
        std::ios::binary);

    std::ofstream global(
        out / "19_GLOBAL.csv",
        std::ios::binary);

    std::ofstream by_symbol(
        out / "19_BY_SYMBOL.csv",
        std::ios::binary);

    std::ofstream by_year(
        out / "19_BY_YEAR.csv",
        std::ios::binary);

    std::ofstream by_type(
        out / "19_BY_TYPE.csv",
        std::ios::binary);

    std::ofstream summary(
        out / "19_SUMMARY.txt",
        std::ios::binary);

    if (!trades ||
        !global ||
        !by_symbol ||
        !by_year ||
        !by_type ||
        !summary)
    {
        std::cerr
            << "BLOCK19 FAIL - OUTPUT_OPEN\n";
        return 3;
    }

    trades
        << "Symbol;Formation;Direction;"
        << "ConfirmTime;SignalTime;"
        << "ConfirmClose;ZoneLow;ZoneHigh;"
        << "EntryTime;EntryPrice;"
        << "EntryGapPoints;"
        << "TPPrice;SLPrice;"
        << "ExitTime;ExitPrice;Outcome;"
        << "EntrySpreadPoints;"
        << "ExitSpreadPoints;"
        << "PnLPoints;R\n";

    Stats all;
    std::map<std::string,Stats> sym_stats;
    std::map<int,Stats> year_stats;
    std::map<std::string,Stats> type_stats;

    std::cout
        << "============================================================\n"
        << "RZA BLOCK 19 - CONFIRMATION CLOSE ENTRY\n"
        << "ENTRY=FIRST_M5_OPEN_AFTER_ENGULFING_H1_CLOSE\n"
        << "TP=89 / SL=233\n"
        << "POPULATION=ALL_ACCEPTED_ABS_TRACK_V2_CORE10\n"
        << "============================================================\n";

    for (const std::string& symbol : BASKET) {
        const fs::path h1_path =
            raw / symbol /
            (symbol + "_H1.bin");

        const fs::path m5_path =
            raw / symbol /
            (symbol + "_M5.bin");

        XfbarData h1 =
            read_xfbar(h1_path.string());

        XfbarData m5 =
            read_xfbar(m5_path.string());

        if (!h1.success ||
            !m5.success ||
            h1.symbol != symbol ||
            m5.symbol != symbol ||
            h1.period_seconds != H1_SECONDS ||
            m5.period_seconds != M5_SECONDS ||
            h1.digits != m5.digits ||
            !(h1.point > 0.0) ||
            !(m5.point > 0.0) ||
            std::abs(h1.point - m5.point) >
                std::max(h1.point,m5.point) *
                    1e-9)
        {
            std::cerr
                << "BLOCK19 FAIL - DATA_CONTRACT "
                << symbol
                << " H1=" << h1.error
                << " M5=" << m5.error
                << '\n';
            return 4;
        }

        ap::AtrLookup atr(
            m5.bars,
            ap::params().atr_period);

        if (!atr.available()) {
            std::cerr
                << "BLOCK19 FAIL - ATR_UNAVAILABLE "
                << symbol
                << '\n';
            return 5;
        }

        const ap::CloseIndex close_index(
            h1.bars);

        ap::ActiveZones active;

        Stats& ss =
            sym_stats[symbol];

        for (std::size_t ci = 0;
             ci + 1 < h1.bars.size();
             ++ci)
        {
            const auto e_opt =
                detect_at(
                    h1.bars,
                    ci);

            if (!e_opt.has_value())
                continue;

            FormationEvent e =
                *e_opt;

            const auto z =
                ap::calculate_zone_bounds(
                    h1.bars[e.source_index],
                    e.direction,
                    h1.point);

            e.zone_low = z.low;
            e.zone_high = z.high;

            const std::int64_t confirm_time =
                h1.bars[ci].time;

            const std::int64_t signal_time =
                h1.bars[ci + 1].time;

            const double confirm_close =
                h1.bars[ci].close;

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
                continue;

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

            Stats& ys =
                year_stats[
                    utc_year(signal_time)];

            Stats& ts =
                type_stats[
                    formation_name(e.type)];

            ++all.accepted;
            ++ss.accepted;
            ++ys.accepted;
            ++ts.accepted;

            Trade t =
                replay_from_confirmation(
                    symbol,
                    e.type,
                    e.direction,
                    confirm_time,
                    signal_time,
                    confirm_close,
                    e.zone_low,
                    e.zone_high,
                    m5);

            all.add(t);
            ss.add(t);
            ys.add(t);
            ts.add(t);

            if (t.outcome != Outcome::INVALID) {
                trades
                    << symbol
                    << ';'
                    << formation_name(e.type)
                    << ';'
                    << (e.direction ==
                                Direction::BULLISH
                            ? "BULLISH"
                            : "BEARISH")
                    << ';'
                    << confirm_time
                    << ';'
                    << signal_time
                    << ';'
                    << std::setprecision(12)
                    << confirm_close
                    << ';'
                    << e.zone_low
                    << ';'
                    << e.zone_high
                    << ';'
                    << t.entry_time
                    << ';'
                    << t.entry
                    << ';'
                    << t.entry_gap_points
                    << ';'
                    << t.tp
                    << ';'
                    << t.sl
                    << ';'
                    << t.exit_time
                    << ';'
                    << t.exit
                    << ';'
                    << outcome_name(t.outcome)
                    << ';'
                    << t.entry_spread_points
                    << ';'
                    << t.exit_spread_points
                    << ';'
                    << t.pnl_points
                    << ';'
                    << t.r
                    << '\n';
            }
        }

        std::cout
            << symbol
            << " accepted=" << ss.accepted
            << " entered=" << ss.entered
            << " closed=" << ss.closed
            << " win="
            << std::setprecision(6)
            << ss.win_rate()
            << "% PF_R="
            << ss.pf_r()
            << " AvgR="
            << ss.avg_r()
            << '\n';
    }

    write_header(
        global,
        "Scope");

    write_row(
        global,
        "CORE10",
        all);

    write_header(
        by_symbol,
        "Symbol");

    for (const auto& symbol : BASKET)
        write_row(
            by_symbol,
            symbol,
            sym_stats[symbol]);

    write_header(
        by_year,
        "Year");

    for (const auto& kv : year_stats)
        write_row(
            by_year,
            std::to_string(kv.first),
            kv.second);

    write_header(
        by_type,
        "Formation");

    write_row(
        by_type,
        "ENGULF_2",
        type_stats["ENGULF_2"]);

    write_row(
        by_type,
        "ENGULF_3",
        type_stats["ENGULF_3"]);

    summary
        << "RZA BLOCK 19 - CONFIRMATION CLOSE ENTRY\n"
        << "PARENT_POLICY=ABS_TRACK_V2_EXACT\n"
        << "POPULATION=ALL_ACCEPTED_ZONES_CORE10\n"
        << "DECISION=AFTER_CONFIRMATION_H1_CLOSE\n"
        << "EXECUTION=FIRST_AVAILABLE_M5_OPEN_AT_NEXT_H1\n"
        << "REFERENCE_PRICE=CONFIRMATION_H1_CLOSE\n"
        << "BUY_EXECUTION=ASK_OPEN_USING_XFBAR_SPREAD\n"
        << "SELL_EXECUTION=BID_OPEN\n"
        << "TP_POINTS_FROM_ACTUAL_ENTRY=89\n"
        << "SL_POINTS_FROM_ACTUAL_ENTRY=233\n"
        << "SAME_M5_TP_AND_SL=CONSERVATIVE_SL\n"
        << "SLIPPAGE_POINTS=0_NOT_INVENTED\n"
        << "COMMISSION=NOT_INCLUDED_NO_BROKER_SPEC\n"
        << "START_DEPOSIT_USD=10000\n"
        << "ACCOUNT_LEVERAGE=1:500\n"
        << "MIN_MARGIN_LEVEL_PCT=500\n"
        << "USD_PNL_AND_MARGIN=DEFERRED_UNTIL_BROKER_SYMBOL_SPECS\n"
        << "ACCEPTED="
        << all.accepted
        << '\n'
        << "ENTERED="
        << all.entered
        << '\n'
        << "CLOSED="
        << all.closed
        << '\n'
        << "WINS="
        << all.wins
        << '\n'
        << "LOSSES="
        << all.losses
        << '\n'
        << "AMBIGUOUS_AS_SL="
        << all.ambiguous
        << '\n'
        << "CENSORED="
        << all.censored
        << '\n'
        << "INVALID="
        << all.invalid
        << '\n'
        << std::setprecision(12)
        << "WIN_RATE_PCT="
        << all.win_rate()
        << '\n'
        << "AVG_ABS_ENTRY_GAP_POINTS="
        << all.avg_abs_gap_points()
        << '\n'
        << "AVG_R="
        << all.avg_r()
        << '\n'
        << "PROFIT_FACTOR_R="
        << all.pf_r()
        << '\n'
        << "SUM_R="
        << static_cast<double>(all.sum_r)
        << '\n';

    std::cout
        << "============================================================\n"
        << "RZA BLOCK 19 PASS\n"
        << "ACCEPTED=" << all.accepted
        << " ENTERED=" << all.entered
        << " CLOSED=" << all.closed
        << " WINS=" << all.wins
        << " LOSSES=" << all.losses
        << '\n'
        << std::setprecision(8)
        << "WIN_RATE="
        << all.win_rate()
        << "% AVG_R="
        << all.avg_r()
        << " PF_R="
        << all.pf_r()
        << '\n'
        << "AVG_ABS_ENTRY_GAP_POINTS="
        << all.avg_abs_gap_points()
        << '\n'
        << "============================================================\n";

    return 0;
}
