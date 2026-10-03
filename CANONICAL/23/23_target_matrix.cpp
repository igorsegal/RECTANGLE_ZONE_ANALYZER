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
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;

namespace {

constexpr std::int32_t M5_SECONDS = 300;
constexpr double FIB_ENTRY = 0.382;
constexpr double FIB_TP_EXT = 1.618;
constexpr std::size_t LIMIT_LIFE_M5 = 3;

const std::array<std::int32_t,3> SIGNAL_TFS{{
    5400, 7200, 10800
}};

struct TargetSpec {
    const char* name;
    int fixed_points;
    bool geometric;
};

const std::array<TargetSpec,5> TARGETS{{
    {"TP150",150,false},
    {"TP200",200,false},
    {"TP300",300,false},
    {"TP400",400,false},
    {"GEOMETRIC_1618",0,true}
}};

enum class Outcome {
    TP,
    SL,
    AMBIGUOUS_AS_SL,
    CENSORED,
    NO_FILL,
    INVALID
};

const char* outcome_name(Outcome x) {
    switch (x) {
        case Outcome::TP: return "TP";
        case Outcome::SL: return "SL";
        case Outcome::AMBIGUOUS_AS_SL: return "SL_AMBIGUOUS";
        case Outcome::CENSORED: return "CENSORED";
        case Outcome::NO_FILL: return "NO_FILL";
        default: return "INVALID";
    }
}

const char* formation_name(FormationType x) {
    return x == FormationType::ENGULF_2
        ? "ENGULF_2"
        : "ENGULF_3";
}

std::string tf_label(std::int32_t sec) {
    switch (sec) {
        case 5400: return "M90";
        case 7200: return "M120";
        case 10800: return "M180";
        default: return std::to_string(sec) + "s";
    }
}

int utc_year(std::int64_t ts) {
    std::time_t tt = static_cast<std::time_t>(ts);
    const std::tm* tm = std::gmtime(&tt);
    return tm ? tm->tm_year + 1900 : 0;
}

std::size_t lower_bound_time(
    const std::vector<Bar>& bars,
    std::int64_t t)
{
    return static_cast<std::size_t>(
        std::lower_bound(
            bars.begin(), bars.end(), t,
            [](const Bar& b, std::int64_t x) {
                return b.time < x;
            }) - bars.begin());
}

std::vector<Bar> aggregate_complete(
    const std::vector<Bar>& m5,
    std::int32_t tf_seconds)
{
    std::vector<Bar> out;

    if (tf_seconds < M5_SECONDS ||
        tf_seconds % M5_SECONDS != 0 ||
        m5.empty())
        return out;

    const std::size_t need =
        static_cast<std::size_t>(
            tf_seconds / M5_SECONDS);

    std::size_t i = 0;

    while (i < m5.size()) {
        const std::int64_t bucket =
            (m5[i].time / tf_seconds) *
            static_cast<std::int64_t>(tf_seconds);

        std::size_t j = i + 1;

        while (j < m5.size()) {
            const std::int64_t bj =
                (m5[j].time / tf_seconds) *
                static_cast<std::int64_t>(tf_seconds);

            if (bj != bucket)
                break;

            ++j;
        }

        bool complete = (j - i) == need;

        if (complete) {
            if (m5[i].time != bucket)
                complete = false;

            for (std::size_t k = i; complete && k < j; ++k) {
                const std::int64_t expected =
                    bucket +
                    static_cast<std::int64_t>(k - i) *
                    M5_SECONDS;

                if (m5[k].time != expected)
                    complete = false;
            }
        }

        if (complete) {
            Bar b;
            b.time = bucket;
            b.open = m5[i].open;
            b.high = m5[i].high;
            b.low = m5[i].low;
            b.close = m5[j - 1].close;
            b.spread = m5[j - 1].spread;

            for (std::size_t k = i + 1; k < j; ++k) {
                b.high = std::max(b.high, m5[k].high);
                b.low = std::min(b.low, m5[k].low);
            }

            out.push_back(b);
        }

        i = j;
    }

    return out;
}

double entry_level(
    Direction direction,
    double high,
    double low)
{
    const double range = high - low;

    if (!(range > 0.0))
        return std::numeric_limits<double>::quiet_NaN();

    if (direction == Direction::BULLISH)
        return high - FIB_ENTRY * range;

    return low + FIB_ENTRY * range;
}

double stop_level(
    Direction direction,
    double high,
    double low)
{
    return direction == Direction::BULLISH
        ? low
        : high;
}

double geometric_target(
    Direction direction,
    double high,
    double low)
{
    const double range = high - low;

    if (!(range > 0.0))
        return std::numeric_limits<double>::quiet_NaN();

    if (direction == Direction::BULLISH)
        return low + FIB_TP_EXT * range;

    return high - FIB_TP_EXT * range;
}

struct Trade {
    std::int32_t tf_seconds = 0;
    std::string target_name;
    FormationType formation = FormationType::ENGULF_2;
    Direction direction = Direction::BULLISH;

    std::int64_t confirm_time = 0;
    std::int64_t signal_time = 0;
    std::int64_t entry_time = 0;
    std::int64_t exit_time = 0;

    double confirm_high = 0.0;
    double confirm_low = 0.0;
    double confirm_close = 0.0;

    double limit = 0.0;
    double entry = 0.0;
    double sl = 0.0;
    double tp = 0.0;
    double exit = 0.0;

    double risk_points = 0.0;
    double reward_points = 0.0;
    double pnl_points = 0.0;
    double r = 0.0;

    int entry_spread_points = 0;
    int exit_spread_points = 0;

    bool gap_fill = false;
    Outcome outcome = Outcome::INVALID;
};

struct Stats {
    std::uint64_t accepted = 0;
    std::uint64_t filled = 0;
    std::uint64_t no_fill = 0;
    std::uint64_t closed = 0;
    std::uint64_t wins = 0;
    std::uint64_t losses = 0;
    std::uint64_t ambiguous = 0;
    std::uint64_t censored = 0;
    std::uint64_t invalid = 0;
    std::uint64_t gap_fills = 0;

    long double sum_r = 0.0L;
    long double gp_r = 0.0L;
    long double gl_r = 0.0L;
    long double sum_risk_points = 0.0L;
    long double sum_reward_points = 0.0L;

    void add(const Trade& t) {
        if (t.outcome == Outcome::INVALID) {
            ++invalid;
            return;
        }

        if (t.outcome == Outcome::NO_FILL) {
            ++no_fill;
            return;
        }

        ++filled;

        if (t.gap_fill)
            ++gap_fills;

        if (t.risk_points > 0.0)
            sum_risk_points += t.risk_points;

        if (t.reward_points > 0.0)
            sum_reward_points += t.reward_points;

        if (t.outcome == Outcome::CENSORED) {
            ++censored;
            return;
        }

        ++closed;
        sum_r += t.r;

        if (t.outcome == Outcome::TP) {
            ++wins;
            if (t.r > 0.0)
                gp_r += t.r;
        } else {
            ++losses;

            if (t.outcome == Outcome::AMBIGUOUS_AS_SL)
                ++ambiguous;

            if (t.r < 0.0)
                gl_r += -t.r;
        }
    }

    double fill_rate() const {
        return accepted
            ? 100.0 * static_cast<double>(filled) /
                  static_cast<double>(accepted)
            : std::numeric_limits<double>::quiet_NaN();
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

    double avg_risk_points() const {
        return filled
            ? static_cast<double>(
                  sum_risk_points /
                  static_cast<long double>(filled))
            : std::numeric_limits<double>::quiet_NaN();
    }

    double avg_reward_points() const {
        return filled
            ? static_cast<double>(
                  sum_reward_points /
                  static_cast<long double>(filled))
            : std::numeric_limits<double>::quiet_NaN();
    }
};

Trade replay_trade(
    std::int32_t tf_seconds,
    const TargetSpec& target,
    FormationType formation,
    Direction direction,
    std::int64_t confirm_time,
    std::int64_t signal_time,
    double high,
    double low,
    double close,
    const XfbarData& m5)
{
    Trade t;
    t.tf_seconds = tf_seconds;
    t.target_name = target.name;
    t.formation = formation;
    t.direction = direction;
    t.confirm_time = confirm_time;
    t.signal_time = signal_time;
    t.confirm_high = high;
    t.confirm_low = low;
    t.confirm_close = close;

    t.limit = entry_level(direction, high, low);
    t.sl = stop_level(direction, high, low);

    if (!(m5.point > 0.0) ||
        !std::isfinite(t.limit) ||
        !std::isfinite(t.sl))
        return t;

    const std::size_t begin =
        lower_bound_time(
            m5.bars,
            signal_time);

    if (begin >= m5.bars.size())
        return t;

    const std::size_t fill_end =
        std::min(
            m5.bars.size(),
            begin + LIMIT_LIFE_M5);

    const bool buy =
        direction == Direction::BULLISH;

    std::size_t entry_index =
        m5.bars.size();

    for (std::size_t i = begin;
         i < fill_end;
         ++i)
    {
        const Bar& b = m5.bars[i];

        const int spread =
            std::max(0, b.spread);

        const double spr =
            static_cast<double>(spread) *
            m5.point;

        if (buy) {
            const double ask_open =
                b.open + spr;

            const double ask_low =
                b.low + spr;

            if (ask_open <= t.limit) {
                t.gap_fill =
                    ask_open <
                    t.limit - 1e-12;

                t.entry = ask_open;
                t.entry_spread_points = spread;
                entry_index = i;
                break;
            }

            if (ask_low <= t.limit) {
                t.entry = t.limit;
                t.entry_spread_points = spread;
                entry_index = i;
                break;
            }
        } else {
            if (b.open >= t.limit) {
                t.gap_fill =
                    b.open >
                    t.limit + 1e-12;

                t.entry = b.open;
                t.entry_spread_points = spread;
                entry_index = i;
                break;
            }

            if (b.high >= t.limit) {
                t.entry = t.limit;
                t.entry_spread_points = spread;
                entry_index = i;
                break;
            }
        }
    }

    if (entry_index == m5.bars.size()) {
        t.outcome = Outcome::NO_FILL;
        return t;
    }

    t.entry_time =
        m5.bars[entry_index].time;

    if (target.geometric) {
        t.tp =
            geometric_target(
                direction,
                high,
                low);
    } else {
        const double dist =
            static_cast<double>(
                target.fixed_points) *
            m5.point;

        t.tp =
            buy
                ? t.entry + dist
                : t.entry - dist;
    }

    if (!std::isfinite(t.tp)) {
        t.outcome = Outcome::INVALID;
        return t;
    }

    const double risk_price =
        buy
            ? t.entry - t.sl
            : t.sl - t.entry;

    const double reward_price =
        buy
            ? t.tp - t.entry
            : t.entry - t.tp;

    if (!(risk_price > 0.0) ||
        !(reward_price > 0.0))
    {
        t.outcome = Outcome::INVALID;
        return t;
    }

    t.risk_points =
        risk_price / m5.point;

    t.reward_points =
        reward_price / m5.point;

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
            if (i == entry_index &&
                !t.gap_fill)
            {
                if (b.low <= t.sl) {
                    out =
                        Outcome::AMBIGUOUS_AS_SL;
                    exit_price = t.sl;
                }
            } else {
                if (b.open <= t.sl) {
                    out = Outcome::SL;
                    exit_price = b.open;
                } else if (b.open >= t.tp) {
                    out = Outcome::TP;
                    exit_price = b.open;
                }

                if (out == Outcome::INVALID) {
                    const bool hit_tp =
                        b.high >= t.tp;

                    const bool hit_sl =
                        b.low <= t.sl;

                    if (hit_tp && hit_sl) {
                        out =
                            Outcome::AMBIGUOUS_AS_SL;
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
        } else {
            const double ask_open =
                b.open + spr;

            const double ask_high =
                b.high + spr;

            const double ask_low =
                b.low + spr;

            if (i == entry_index &&
                !t.gap_fill)
            {
                if (ask_high >= t.sl) {
                    out =
                        Outcome::AMBIGUOUS_AS_SL;
                    exit_price = t.sl;
                }
            } else {
                if (ask_open >= t.sl) {
                    out = Outcome::SL;
                    exit_price = ask_open;
                } else if (ask_open <= t.tp) {
                    out = Outcome::TP;
                    exit_price = ask_open;
                }

                if (out == Outcome::INVALID) {
                    const bool hit_tp =
                        ask_low <= t.tp;

                    const bool hit_sl =
                        ask_high >= t.sl;

                    if (hit_tp && hit_sl) {
                        out =
                            Outcome::AMBIGUOUS_AS_SL;
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
                risk_price;

            return t;
        }
    }

    t.outcome = Outcome::CENSORED;
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
        << ";Filled"
        << ";NoFill"
        << ";FillRatePct"
        << ";Closed"
        << ";Wins"
        << ";Losses"
        << ";WinRatePct"
        << ";AmbiguousAsSL"
        << ";Censored"
        << ";Invalid"
        << ";GapFills"
        << ";AvgRiskPoints"
        << ";AvgRewardPoints"
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
        << s.filled
        << ';'
        << s.no_fill
        << ';'
        << std::setprecision(12)
        << s.fill_rate()
        << ';'
        << s.closed
        << ';'
        << s.wins
        << ';'
        << s.losses
        << ';'
        << s.win_rate()
        << ';'
        << s.ambiguous
        << ';'
        << s.censored
        << ';'
        << s.invalid
        << ';'
        << s.gap_fills
        << ';'
        << s.avg_risk_points()
        << ';'
        << s.avg_reward_points()
        << ';'
        << s.avg_r()
        << ';'
        << s.pf_r()
        << ';'
        << static_cast<double>(
            s.sum_r)
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

    std::vector<Bar> m5 = {
        {0,100,102,99,101,2},
        {300,101,103,100,102,2},
        {600,102,104,101,103,2},
        {900,103,105,102,104,2},
        {1200,104,106,103,105,2},
        {1500,105,107,104,106,2}
    };

    const auto m15 =
        aggregate_complete(
            m5,
            900);

    check(
        m15.size() == 2,
        "aggregation builds complete bars");

    check(
        std::abs(
            entry_level(
                Direction::BULLISH,
                110,
                100) -
            106.18) < 1e-12,
        "Fib38.2 entry unchanged");

    XfbarData path;
    path.success = true;
    path.symbol = "XAUUSD";
    path.period_seconds = M5_SECONDS;
    path.point = 0.01;
    path.bars = {
        {3600,107.00,107.10,106.16,106.30,2},
        {3900,106.30,110.00,106.20,109.90,2}
    };

    Trade fixed =
        replay_trade(
            5400,
            TARGETS[2],
            FormationType::ENGULF_2,
            Direction::BULLISH,
            0,
            3600,
            110,
            100,
            109,
            path);

    check(
        std::abs(
            fixed.reward_points -
            300.0) < 1e-9,
        "TP300 is exactly 300 points from actual entry");

    check(
        fixed.outcome ==
            Outcome::TP,
        "TP300 scenario reaches target");

    XfbarData geo_path;
    geo_path.success = true;
    geo_path.symbol = "XAUUSD";
    geo_path.period_seconds = M5_SECONDS;
    geo_path.point = 0.01;
    geo_path.bars = {
        {3600,107.00,107.10,106.16,106.30,2},
        {3900,106.30,116.30,106.20,116.20,2}
    };

    Trade geo =
        replay_trade(
            10800,
            TARGETS[4],
            FormationType::ENGULF_3,
            Direction::BULLISH,
            0,
            3600,
            110,
            100,
            109,
            geo_path);

    check(
        std::abs(
            geo.tp -
            116.18) < 1e-9,
        "geometric control remains Fib161.8");

    check(
        geo.outcome ==
            Outcome::TP,
        "geometric control reaches target");

    XfbarData nofill = path;
    nofill.bars = {
        {3600,109,110,108,109,2},
        {3900,109,110,108,109,2},
        {4200,109,110,108,109,2},
        {4500,107,108,106,107,2}
    };

    Trade n =
        replay_trade(
            7200,
            TARGETS[0],
            FormationType::ENGULF_2,
            Direction::BULLISH,
            0,
            3600,
            110,
            100,
            109,
            nofill);

    check(
        n.outcome ==
            Outcome::NO_FILL,
        "fourth M5 touch rejected");

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
            : fs::path("../23out");

    std::error_code ec;
    fs::create_directories(out, ec);

    if (ec) {
        std::cerr
            << "BLOCK23 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 2;
    }

    const std::string symbol =
        "XAUUSD";

    const fs::path m5_path =
        raw /
        symbol /
        (symbol + "_M5.bin");

    XfbarData m5 =
        read_xfbar(
            m5_path.string());

    if (!m5.success ||
        m5.symbol != symbol ||
        m5.period_seconds != M5_SECONDS ||
        !(m5.point > 0.0))
    {
        std::cerr
            << "BLOCK23 FAIL - XAUUSD_M5_DATA_CONTRACT "
            << m5.error
            << '\n';
        return 3;
    }

    ap::AtrLookup atr(
        m5.bars,
        ap::params().atr_period);

    if (!atr.available()) {
        std::cerr
            << "BLOCK23 FAIL - ATR_UNAVAILABLE\n";
        return 4;
    }

    std::ofstream trades(
        out / "23_TRADES.csv",
        std::ios::binary);

    std::ofstream by_tf_target(
        out / "23_BY_TF_TARGET.csv",
        std::ios::binary);

    std::ofstream by_type(
        out / "23_BY_TF_TARGET_TYPE.csv",
        std::ios::binary);

    std::ofstream by_year(
        out / "23_BY_TF_TARGET_YEAR.csv",
        std::ios::binary);

    std::ofstream summary(
        out / "23_SUMMARY.txt",
        std::ios::binary);

    if (!trades ||
        !by_tf_target ||
        !by_type ||
        !by_year ||
        !summary)
    {
        std::cerr
            << "BLOCK23 FAIL - OUTPUT_OPEN\n";
        return 5;
    }

    trades
        << "Symbol;SignalTF;Target;Formation;Direction;"
        << "ConfirmTime;SignalTime;"
        << "ConfirmHigh;ConfirmLow;ConfirmClose;"
        << "Fib382Limit;StopExtreme;TargetPrice;"
        << "EntryTime;EntryPrice;ExitTime;ExitPrice;"
        << "Outcome;EntrySpreadPoints;ExitSpreadPoints;"
        << "GapFill;RiskPoints;RewardPoints;PnLPoints;R\n";

    write_header(
        by_tf_target,
        "SignalTF_Target");

    write_header(
        by_type,
        "SignalTF_Target_Formation");

    write_header(
        by_year,
        "SignalTF_Target_Year");

    std::map<std::string,Stats>
        main_stats;

    std::map<std::string,Stats>
        type_stats;

    std::map<std::string,Stats>
        year_stats;

    std::cout
        << "============================================================\n"
        << "RZA BLOCK 23 - XAUUSD LARGE-TF TARGET MATRIX\n"
        << "SIGNAL_TF=M90/M120/M180\n"
        << "ENTRY=FIB38.2 HIGH-LOW\n"
        << "LIMIT_LIFETIME=3 M5\n"
        << "SL=OPPOSITE CONFIRMATION EXTREME\n"
        << "TARGETS=150/200/300/400 + GEOMETRIC_1618 CONTROL\n"
        << "============================================================\n";

    for (const std::int32_t tf : SIGNAL_TFS) {
        const std::vector<Bar> sig =
            aggregate_complete(
                m5.bars,
                tf);

        if (sig.size() < 20) {
            std::cerr
                << "BLOCK23 FAIL - INSUFFICIENT_AGGREGATED_BARS "
                << tf_label(tf)
                << '\n';
            return 6;
        }

        const ap::CloseIndex
            close_index(sig);

        ap::ActiveZones active;

        for (std::size_t ci = 0;
             ci < sig.size();
             ++ci)
        {
            const auto e_opt =
                detect_at(
                    sig,
                    ci);

            if (!e_opt.has_value())
                continue;

            FormationEvent e =
                *e_opt;

            const auto z =
                ap::calculate_zone_bounds(
                    sig[e.source_index],
                    e.direction,
                    m5.point);

            e.zone_low = z.low;
            e.zone_high = z.high;

            const Bar& confirm =
                sig[ci];

            const std::int64_t signal_time =
                confirm.time + tf;

            active.expire(ci);

            const double req_gap =
                ap::required_gap(
                    m5.point,
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
                    sig,
                    ci,
                    e.direction,
                    e.zone_low,
                    e.zone_high,
                    m5.point);

            active.insert(
                e.direction,
                e.zone_low,
                e.zone_high,
                break_idx);

            for (const TargetSpec& target :
                 TARGETS)
            {
                const std::string main_key =
                    tf_label(tf) +
                    "|" +
                    target.name;

                const std::string type_key =
                    main_key +
                    "|" +
                    formation_name(e.type);

                const std::string year_key =
                    main_key +
                    "|" +
                    std::to_string(
                        utc_year(
                            signal_time));

                ++main_stats[main_key].accepted;
                ++type_stats[type_key].accepted;
                ++year_stats[year_key].accepted;

                Trade t =
                    replay_trade(
                        tf,
                        target,
                        e.type,
                        e.direction,
                        confirm.time,
                        signal_time,
                        confirm.high,
                        confirm.low,
                        confirm.close,
                        m5);

                main_stats[main_key].add(t);
                type_stats[type_key].add(t);
                year_stats[year_key].add(t);

                trades
                    << symbol
                    << ';'
                    << tf_label(tf)
                    << ';'
                    << target.name
                    << ';'
                    << formation_name(e.type)
                    << ';'
                    << (e.direction ==
                                Direction::BULLISH
                            ? "BULLISH"
                            : "BEARISH")
                    << ';'
                    << confirm.time
                    << ';'
                    << signal_time
                    << ';'
                    << std::setprecision(12)
                    << confirm.high
                    << ';'
                    << confirm.low
                    << ';'
                    << confirm.close
                    << ';'
                    << t.limit
                    << ';'
                    << t.sl
                    << ';'
                    << t.tp
                    << ';'
                    << t.entry_time
                    << ';'
                    << t.entry
                    << ';'
                    << t.exit_time
                    << ';'
                    << t.exit
                    << ';'
                    << outcome_name(
                        t.outcome)
                    << ';'
                    << t.entry_spread_points
                    << ';'
                    << t.exit_spread_points
                    << ';'
                    << (t.gap_fill ? 1 : 0)
                    << ';'
                    << t.risk_points
                    << ';'
                    << t.reward_points
                    << ';'
                    << t.pnl_points
                    << ';'
                    << t.r
                    << '\n';
            }
        }

        for (const TargetSpec& target :
             TARGETS)
        {
            const std::string key =
                tf_label(tf) +
                "|" +
                target.name;

            const Stats& s =
                main_stats[key];

            write_row(
                by_tf_target,
                key,
                s);

            std::cout
                << key
                << " accepted=" << s.accepted
                << " filled=" << s.filled
                << " fill="
                << std::setprecision(6)
                << s.fill_rate()
                << "% closed=" << s.closed
                << " win=" << s.win_rate()
                << "% PF_R=" << s.pf_r()
                << " AvgR=" << s.avg_r()
                << " AvgRiskPts="
                << s.avg_risk_points()
                << " AvgRewardPts="
                << s.avg_reward_points()
                << '\n';
        }
    }

    for (const auto& kv : type_stats)
        write_row(
            by_type,
            kv.first,
            kv.second);

    for (const auto& kv : year_stats)
        write_row(
            by_year,
            kv.first,
            kv.second);

    summary
        << "RZA BLOCK 23 - XAUUSD LARGE-TF TARGET MATRIX\n"
        << "SYMBOL=XAUUSD_ONLY\n"
        << "SOURCE_DATA=M5_XFBAR\n"
        << "SIGNAL_TF=M90,M120,M180\n"
        << "AGGREGATION=NON_OVERLAPPING_COMPLETE_M5_BUCKETS_ONLY\n"
        << "PARENT_POLICY=ABS_TRACK_V2_EXACT\n"
        << "ENTRY=FIB38.2_FULL_CONFIRMATION_HIGH_LOW\n"
        << "LIMIT_LIFETIME_M5_BARS=3\n"
        << "BUY_LIMIT_PRICE_IS_ASK\n"
        << "SELL_LIMIT_PRICE_IS_BID\n"
        << "SL=OPPOSITE_CONFIRMATION_EXTREME\n"
        << "FIXED_TARGETS_POINTS=150,200,300,400\n"
        << "CONTROL_TARGET=FIB161.8_CONFIRMATION_RANGE\n"
        << "FIXED_TP_IS_FROM_ACTUAL_ENTRY\n"
        << "SAME_M5_UNCERTAINTY=CONSERVATIVE_SL\n"
        << "SLIPPAGE=0_NOT_INVENTED\n"
        << "COMMISSION=NOT_INCLUDED_NO_BROKER_SPEC\n"
        << "USER_REQUESTED_TARGET_COMPARISON=1\n";

    for (const std::int32_t tf :
         SIGNAL_TFS)
    {
        for (const TargetSpec& target :
             TARGETS)
        {
            const std::string key =
                tf_label(tf) +
                "|" +
                target.name;

            const Stats& s =
                main_stats[key];

            summary
                << '\n'
                << "SCENARIO="
                << key
                << '\n'
                << "ACCEPTED="
                << s.accepted
                << '\n'
                << "FILLED="
                << s.filled
                << '\n'
                << "NO_FILL="
                << s.no_fill
                << '\n'
                << std::setprecision(12)
                << "FILL_RATE_PCT="
                << s.fill_rate()
                << '\n'
                << "CLOSED="
                << s.closed
                << '\n'
                << "WINS="
                << s.wins
                << '\n'
                << "LOSSES="
                << s.losses
                << '\n'
                << "WIN_RATE_PCT="
                << s.win_rate()
                << '\n'
                << "AVG_RISK_POINTS="
                << s.avg_risk_points()
                << '\n'
                << "AVG_REWARD_POINTS="
                << s.avg_reward_points()
                << '\n'
                << "AVG_R="
                << s.avg_r()
                << '\n'
                << "PROFIT_FACTOR_R="
                << s.pf_r()
                << '\n'
                << "SUM_R="
                << static_cast<double>(
                    s.sum_r)
                << '\n';
        }
    }

    std::cout
        << "============================================================\n"
        << "RZA BLOCK 23 PASS\n"
        << "XAUUSD / M90 M120 M180 / FIVE TARGETS COMPLETE\n"
        << "============================================================\n";

    return 0;
}
