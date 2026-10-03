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
    SL_ENTRY_BAR_AMBIGUOUS,
    CENSORED,
    INVALID
};

const char* outcome_name(Outcome x) {
    switch (x) {
        case Outcome::TP: return "TP";
        case Outcome::SL: return "SL";
        case Outcome::SL_ENTRY_BAR_AMBIGUOUS:
            return "SL_ENTRY_BAR_AMBIGUOUS";
        case Outcome::CENSORED: return "CENSORED";
        default: return "INVALID";
    }
}

struct Trade {
    std::string symbol;
    Direction direction = Direction::BULLISH;
    std::int64_t signal_time = 0;
    std::int64_t entry_time = 0;
    std::int64_t exit_time = 0;
    double zone_low = 0.0;
    double zone_high = 0.0;
    double point = 0.0;
    double entry = 0.0;
    double tp = 0.0;
    double sl = 0.0;
    double exit = 0.0;
    double pnl_points = 0.0;
    double r = 0.0;
    int entry_spread_points = 0;
    int exit_spread_points = 0;
    bool gap_entry = false;
    Outcome outcome = Outcome::INVALID;
};

struct Stats {
    std::uint64_t accepted = 0;
    std::uint64_t triggered = 0;
    std::uint64_t closed = 0;
    std::uint64_t wins = 0;
    std::uint64_t losses = 0;
    std::uint64_t entry_bar_ambiguous = 0;
    std::uint64_t censored = 0;
    std::uint64_t invalid = 0;
    std::uint64_t gap_entries = 0;
    long double sum_r = 0.0L;
    long double gross_profit_r = 0.0L;
    long double gross_loss_r = 0.0L;

    void add(const Trade& t) {
        if (t.outcome == Outcome::INVALID) {
            ++invalid;
            return;
        }
        ++triggered;
        if (t.gap_entry) ++gap_entries;

        if (t.outcome == Outcome::CENSORED) {
            ++censored;
            return;
        }

        ++closed;
        sum_r += t.r;

        if (t.outcome == Outcome::TP) {
            ++wins;
            if (t.r > 0.0) gross_profit_r += t.r;
        } else {
            ++losses;
            if (t.outcome == Outcome::SL_ENTRY_BAR_AMBIGUOUS)
                ++entry_bar_ambiguous;
            if (t.r < 0.0) gross_loss_r += -t.r;
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
            ? static_cast<double>(sum_r /
                  static_cast<long double>(closed))
            : std::numeric_limits<double>::quiet_NaN();
    }

    double pf_r() const {
        if (gross_loss_r > 0.0L)
            return static_cast<double>(
                gross_profit_r / gross_loss_r);
        if (gross_profit_r > 0.0L)
            return std::numeric_limits<double>::infinity();
        return std::numeric_limits<double>::quiet_NaN();
    }
};

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

int year_utc(std::int64_t ts) {
    std::time_t tt = static_cast<std::time_t>(ts);
    const std::tm* tm = std::gmtime(&tt);
    return tm ? tm->tm_year + 1900 : 0;
}

Trade replay_pending(
    const std::string& symbol,
    Direction direction,
    std::int64_t signal_time,
    double zone_low,
    double zone_high,
    const XfbarData& m5,
    std::size_t begin,
    std::size_t end)
{
    Trade t;
    t.symbol = symbol;
    t.direction = direction;
    t.signal_time = signal_time;
    t.zone_low = zone_low;
    t.zone_high = zone_high;
    t.point = m5.point;

    if (!(m5.point > 0.0) ||
        begin >= end ||
        end > m5.bars.size())
        return t;

    const bool buy = direction == Direction::BULLISH;
    std::size_t entry_index = m5.bars.size();

    for (std::size_t i = begin; i < end; ++i) {
        const Bar& b = m5.bars[i];
        const int spread = std::max(0, b.spread);
        const double spr =
            static_cast<double>(spread) * m5.point;

        if (buy) {
            if (b.open >= zone_high) {
                t.gap_entry =
                    b.open > zone_high + 1e-12;
                t.entry = b.open + spr;
                entry_index = i;
                t.entry_spread_points = spread;
                break;
            }
            if (b.high >= zone_high) {
                t.entry = zone_high + spr;
                entry_index = i;
                t.entry_spread_points = spread;
                break;
            }
        } else {
            if (b.open <= zone_low) {
                t.gap_entry =
                    b.open < zone_low - 1e-12;
                t.entry = b.open;
                entry_index = i;
                t.entry_spread_points = spread;
                break;
            }
            if (b.low <= zone_low) {
                t.entry = zone_low;
                entry_index = i;
                t.entry_spread_points = spread;
                break;
            }
        }
    }

    if (entry_index == m5.bars.size())
        return t;

    t.entry_time = m5.bars[entry_index].time;
    t.tp = buy
        ? t.entry + static_cast<double>(TP_POINTS) * m5.point
        : t.entry - static_cast<double>(TP_POINTS) * m5.point;
    t.sl = buy
        ? t.entry - static_cast<double>(SL_POINTS) * m5.point
        : t.entry + static_cast<double>(SL_POINTS) * m5.point;

    for (std::size_t i = entry_index; i < end; ++i) {
        const Bar& b = m5.bars[i];
        const int spread = std::max(0, b.spread);
        const double spr =
            static_cast<double>(spread) * m5.point;

        Outcome out = Outcome::INVALID;
        double exit_price = 0.0;

        if (buy) {
            if (i > entry_index || t.gap_entry) {
                if (b.open <= t.sl) {
                    out = Outcome::SL;
                    exit_price = b.open;
                } else if (b.open >= t.tp) {
                    out = Outcome::TP;
                    exit_price = b.open;
                }
            }

            if (out == Outcome::INVALID) {
                const bool hit_tp = b.high >= t.tp;
                const bool hit_sl = b.low <= t.sl;

                if (i == entry_index &&
                    !t.gap_entry &&
                    hit_sl)
                {
                    out = Outcome::SL_ENTRY_BAR_AMBIGUOUS;
                    exit_price = t.sl;
                } else if (hit_tp && hit_sl) {
                    out = Outcome::SL;
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
            const double ask_open = b.open + spr;
            const double ask_high = b.high + spr;
            const double ask_low = b.low + spr;

            if (i > entry_index || t.gap_entry) {
                if (ask_open >= t.sl) {
                    out = Outcome::SL;
                    exit_price = ask_open;
                } else if (ask_open <= t.tp) {
                    out = Outcome::TP;
                    exit_price = ask_open;
                }
            }

            if (out == Outcome::INVALID) {
                const bool hit_tp = ask_low <= t.tp;
                const bool hit_sl = ask_high >= t.sl;

                if (i == entry_index &&
                    !t.gap_entry &&
                    hit_sl)
                {
                    out = Outcome::SL_ENTRY_BAR_AMBIGUOUS;
                    exit_price = t.sl;
                } else if (hit_tp && hit_sl) {
                    out = Outcome::SL;
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

            t.pnl_points = pnl_price / m5.point;
            t.r = pnl_price /
                (static_cast<double>(SL_POINTS) * m5.point);
            return t;
        }
    }

    t.outcome = Outcome::CENSORED;
    t.exit_time = m5.bars[end - 1].time;
    return t;
}

void write_header(std::ostream& o, const char* first) {
    o << first
      << ";Accepted;Triggered;Closed;Wins;Losses;WinRatePct"
      << ";EntryBarAmbiguousAsSL;Censored;Invalid;GapEntries"
      << ";AvgR;ProfitFactorR;SumR\n";
}

void write_row(
    std::ostream& o,
    const std::string& key,
    const Stats& s)
{
    o << key << ';'
      << s.accepted << ';'
      << s.triggered << ';'
      << s.closed << ';'
      << s.wins << ';'
      << s.losses << ';'
      << std::setprecision(12)
      << s.win_rate() << ';'
      << s.entry_bar_ambiguous << ';'
      << s.censored << ';'
      << s.invalid << ';'
      << s.gap_entries << ';'
      << s.avg_r() << ';'
      << s.pf_r() << ';'
      << static_cast<double>(s.sum_r)
      << '\n';
}

int selftest() {
    int failed = 0;

    auto check = [&](bool ok, const char* name) {
        std::cout
            << (ok ? "[OK]   " : "[FAIL] ")
            << name << '\n';
        if (!ok) ++failed;
    };

    XfbarData m;
    m.success = true;
    m.symbol = "XAUUSD";
    m.period_seconds = 300;
    m.point = 0.01;
    m.bars = {
        {1000,100.00,100.20,99.90,100.10,2},
        {1300,100.10,101.60,100.00,101.20,2}
    };

    Trade a = replay_pending(
        "XAUUSD",
        Direction::BULLISH,
        1000,
        100.50,
        100.70,
        m,
        0,
        m.bars.size());

    check(a.outcome == Outcome::TP,
          "bull pending edge entry reaches TP89");
    check(std::abs(a.entry - 100.72) < 1e-9,
          "bull entry is edge Bid plus spread");
    check(std::abs(a.sl - 98.39) < 1e-9,
          "bull fixed SL233 from actual entry");

    XfbarData amb = m;
    amb.bars = {
        {1000,100.00,100.20,99.90,100.10,0},
        {1300,100.10,101.80,97.00,100.00,0}
    };

    Trade q = replay_pending(
        "XAUUSD",
        Direction::BULLISH,
        1000,
        100.50,
        100.70,
        amb,
        0,
        amb.bars.size());

    check(q.outcome ==
              Outcome::SL_ENTRY_BAR_AMBIGUOUS,
          "entry-bar adverse ambiguity is fail-closed SL");

    XfbarData sell;
    sell.success = true;
    sell.symbol = "DJ30";
    sell.period_seconds = 300;
    sell.point = 1.0;
    sell.bars = {
        {1000,1000,1005,995,1000,2},
        {1300,995,998,890,900,2}
    };

    Trade z = replay_pending(
        "DJ30",
        Direction::BEARISH,
        1000,
        990,
        1010,
        sell,
        0,
        sell.bars.size());

    check(z.entry == 990.0,
          "sell pending enters on Bid edge");
    check(z.outcome == Outcome::TP,
          "sell TP evaluated on Ask");

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
            : fs::path("D:/AHexaTrader/1DataFiles/raw");

    const fs::path out =
        argc >= 3
            ? fs::path(argv[2])
            : fs::path("../18out");

    std::error_code ec;
    fs::create_directories(out, ec);

    if (ec) {
        std::cerr
            << "BLOCK18 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 2;
    }

    std::ofstream trades(
        out / "18_TRADES.csv",
        std::ios::binary);
    std::ofstream global(
        out / "18_GLOBAL.csv",
        std::ios::binary);
    std::ofstream by_symbol(
        out / "18_BY_SYMBOL.csv",
        std::ios::binary);
    std::ofstream by_year(
        out / "18_BY_YEAR.csv",
        std::ios::binary);
    std::ofstream summary(
        out / "18_SUMMARY.txt",
        std::ios::binary);

    if (!trades ||
        !global ||
        !by_symbol ||
        !by_year ||
        !summary)
    {
        std::cerr
            << "BLOCK18 FAIL - OUTPUT_OPEN\n";
        return 3;
    }

    trades
        << "Symbol;Direction;SignalTime;"
        << "ZoneLow;ZoneHigh;"
        << "EntryTime;EntryPrice;TPPrice;SLPrice;"
        << "ExitTime;ExitPrice;Outcome;"
        << "EntrySpreadPoints;ExitSpreadPoints;"
        << "GapEntry;PnLPoints;R\n";

    Stats all;
    std::map<std::string,Stats> sym_stats;
    std::map<int,Stats> year_stats;

    std::cout
        << "============================================================\n"
        << "RZA BLOCK 18 - CAUSAL EDGE PENDING REPLAY\n"
        << "ENTRY=FIRST_TOUCH_OF_PARENT_EDGE_AFTER_SIGNAL_KNOWN\n"
        << "TP=89 / SL=233\n"
        << "BASKET=CORE10\n"
        << "============================================================\n";

    for (const std::string& symbol : BASKET) {
        const fs::path h1_path =
            raw / symbol / (symbol + "_H1.bin");
        const fs::path m5_path =
            raw / symbol / (symbol + "_M5.bin");

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
                std::max(h1.point,m5.point) * 1e-9)
        {
            std::cerr
                << "BLOCK18 FAIL - DATA_CONTRACT "
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
                << "BLOCK18 FAIL - ATR_UNAVAILABLE "
                << symbol
                << '\n';
            return 5;
        }

        const ap::CloseIndex close_index(h1.bars);
        ap::ActiveZones active;
        Stats& ss = sym_stats[symbol];

        for (std::size_t ci = 0;
             ci + 1 < h1.bars.size();
             ++ci)
        {
            const auto e_opt =
                detect_at(h1.bars, ci);

            if (!e_opt.has_value())
                continue;

            FormationEvent e = *e_opt;

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
                    atr.at_decision(signal_time));

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

            ++all.accepted;
            ++ss.accepted;

            Stats& ys =
                year_stats[year_utc(signal_time)];
            ++ys.accepted;

            const std::int64_t lifecycle_end_time =
                break_idx != ap::CloseIndex::npos()
                    ? h1.bars[break_idx].time +
                          H1_SECONDS
                    : m5.bars.back().time +
                          M5_SECONDS;

            const std::size_t begin =
                lower_bound_time(
                    m5.bars,
                    signal_time);

            const std::size_t end =
                std::min(
                    m5.bars.size(),
                    lower_bound_time(
                        m5.bars,
                        lifecycle_end_time));

            if (begin >= end) {
                Trade invalid;
                invalid.symbol = symbol;
                invalid.direction = e.direction;
                invalid.signal_time = signal_time;
                invalid.zone_low = e.zone_low;
                invalid.zone_high = e.zone_high;

                all.add(invalid);
                ss.add(invalid);
                ys.add(invalid);
                continue;
            }

            Trade t =
                replay_pending(
                    symbol,
                    e.direction,
                    signal_time,
                    e.zone_low,
                    e.zone_high,
                    m5,
                    begin,
                    end);

            all.add(t);
            ss.add(t);
            ys.add(t);

            if (t.outcome != Outcome::INVALID) {
                trades
                    << symbol
                    << ';'
                    << (e.direction ==
                                Direction::BULLISH
                            ? "BULLISH"
                            : "BEARISH")
                    << ';'
                    << signal_time
                    << ';'
                    << std::setprecision(12)
                    << e.zone_low
                    << ';'
                    << e.zone_high
                    << ';'
                    << t.entry_time
                    << ';'
                    << t.entry
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
                    << (t.gap_entry ? 1 : 0)
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
            << " triggered=" << ss.triggered
            << " closed=" << ss.closed
            << " win="
            << std::setprecision(6)
            << ss.win_rate()
            << "% PF_R="
            << ss.pf_r()
            << '\n';
    }

    write_header(global, "Scope");
    write_row(global, "CORE10", all);

    write_header(by_symbol, "Symbol");

    for (const auto& symbol : BASKET)
        write_row(
            by_symbol,
            symbol,
            sym_stats[symbol]);

    write_header(by_year, "Year");

    for (const auto& kv : year_stats)
        write_row(
            by_year,
            std::to_string(kv.first),
            kv.second);

    summary
        << "RZA BLOCK 18 - CAUSAL EDGE PENDING REPLAY\n"
        << "PARENT_POLICY=ABS_TRACK_V2_EXACT\n"
        << "POPULATION=ALL_ACCEPTED_ZONES_CORE10\n"
        << "SIGNAL_KNOWN_AT=NEXT_H1_OPEN_AFTER_CONFIRMATION_CLOSE\n"
        << "PENDING_ACTIVE_FROM=SIGNAL_TIME\n"
        << "BULL_TRIGGER=BID_TOUCH_ZONE_HIGH\n"
        << "BULL_EXECUTION=EDGE_OR_WORSE_GAP_BID_PLUS_XFBAR_SPREAD\n"
        << "BEAR_TRIGGER=BID_TOUCH_ZONE_LOW\n"
        << "BEAR_EXECUTION=EDGE_OR_WORSE_GAP_BID\n"
        << "TP_POINTS_FROM_ACTUAL_ENTRY=89\n"
        << "SL_POINTS_FROM_ACTUAL_ENTRY=233\n"
        << "SELL_EXIT_TRIGGER=ASK_APPROX_BID_PLUS_BAR_SPREAD\n"
        << "ENTRY_BAR_ADVERSE_AMBIGUITY=CONSERVATIVE_SL\n"
        << "SLIPPAGE_POINTS=0_NOT_INVENTED\n"
        << "COMMISSION=NOT_INCLUDED_NO_BROKER_SPEC\n"
        << "START_DEPOSIT_USD=10000\n"
        << "ACCOUNT_LEVERAGE=1:500\n"
        << "MIN_MARGIN_LEVEL_PCT=500\n"
        << "USD_PNL_AND_MARGIN=DEFERRED_UNTIL_BROKER_SYMBOL_SPECS\n"
        << "ACCEPTED_ZONES="
        << all.accepted
        << '\n'
        << "TRIGGERED_TRADES="
        << all.triggered
        << '\n'
        << "CLOSED_TRADES="
        << all.closed
        << '\n'
        << "WINS="
        << all.wins
        << '\n'
        << "LOSSES="
        << all.losses
        << '\n'
        << "ENTRY_BAR_AMBIGUOUS_AS_SL="
        << all.entry_bar_ambiguous
        << '\n'
        << "CENSORED="
        << all.censored
        << '\n'
        << "INVALID="
        << all.invalid
        << '\n'
        << "GAP_ENTRIES="
        << all.gap_entries
        << '\n'
        << std::setprecision(12)
        << "WIN_RATE_PCT="
        << all.win_rate()
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
        << "RZA BLOCK 18 PASS\n"
        << "ACCEPTED=" << all.accepted
        << " TRIGGERED=" << all.triggered
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
        << "ENTRY_BAR_AMBIGUOUS_AS_SL="
        << all.entry_bar_ambiguous
        << " GAP_ENTRIES="
        << all.gap_entries
        << '\n'
        << "============================================================\n";

    return 0;
}
