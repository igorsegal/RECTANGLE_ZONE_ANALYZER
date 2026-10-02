#pragma once

#include "01/formation_detector.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace rza::canonical::trade_emulation {

struct TradeResult {
    bool closed = false;
    double entry_exec_price = 0.0;
    double exit_exec_price = 0.0;
    double pnl_price = 0.0;
    double pnl_points = 0.0;
    double return_pct = 0.0;
    int entry_spread_points = 0;
    int exit_spread_points = 0;
};

struct TradeStats {
    std::uint64_t closed = 0;
    std::uint64_t positive = 0;
    std::uint64_t negative = 0;
    std::uint64_t breakeven = 0;

    long double gross_profit_points = 0.0L;
    long double gross_loss_points_abs = 0.0L;
    long double net_points = 0.0L;
    long double sum_return_pct = 0.0L;

    void add(const TradeResult& t) {
        if (!t.closed) return;

        ++closed;
        net_points += static_cast<long double>(t.pnl_points);
        sum_return_pct += static_cast<long double>(t.return_pct);

        constexpr double eps = 1e-12;

        if (t.pnl_points > eps) {
            ++positive;
            gross_profit_points += static_cast<long double>(t.pnl_points);
        } else if (t.pnl_points < -eps) {
            ++negative;
            gross_loss_points_abs +=
                static_cast<long double>(-t.pnl_points);
        } else {
            ++breakeven;
        }
    }

    double positive_pct() const {
        if (closed == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return 100.0 *
            static_cast<double>(positive) /
            static_cast<double>(closed);
    }

    double profit_factor_points() const {
        if (gross_loss_points_abs <= 0.0L) {
            if (gross_profit_points > 0.0L) {
                return std::numeric_limits<double>::infinity();
            }
            return std::numeric_limits<double>::quiet_NaN();
        }

        return static_cast<double>(
            gross_profit_points / gross_loss_points_abs);
    }

    double avg_net_points() const {
        if (closed == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return static_cast<double>(
            net_points / static_cast<long double>(closed));
    }

    double avg_return_pct() const {
        if (closed == 0) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        return static_cast<double>(
            sum_return_pct / static_cast<long double>(closed));
    }
};

inline TradeResult execute_after_close_signals(
    const std::vector<Bar>& bars,
    std::size_t touch_index,
    std::size_t outcome_index,
    Direction direction,
    double point)
{
    TradeResult out;

    // Touch and outcome become known only AFTER their bars close.
    // Therefore the first causally executable price is the OPEN
    // of the following real bar.
    if (touch_index >= bars.size() ||
        outcome_index >= bars.size() ||
        outcome_index <= touch_index ||
        touch_index + 1 >= bars.size() ||
        outcome_index + 1 >= bars.size() ||
        !(point > 0.0))
    {
        return out;
    }

    const Bar& entry_exec_bar = bars[touch_index + 1];
    const Bar& exit_exec_bar = bars[outcome_index + 1];

    const int entry_spread =
        entry_exec_bar.spread > 0 ? entry_exec_bar.spread : 0;

    const int exit_spread =
        exit_exec_bar.spread > 0 ? exit_exec_bar.spread : 0;

    const double entry_bid = entry_exec_bar.open;
    const double exit_bid = exit_exec_bar.open;

    out.entry_spread_points = entry_spread;
    out.exit_spread_points = exit_spread;

    if (direction == Direction::BULLISH) {
        // BUY: decision after touch close, execution next bar at Ask open.
        out.entry_exec_price =
            entry_bid + static_cast<double>(entry_spread) * point;

        // Exit decision after outcome close, execution next bar at Bid open.
        out.exit_exec_price =
            exit_bid;

        out.pnl_price =
            out.exit_exec_price - out.entry_exec_price;
    } else {
        // SELL: decision after touch close, execution next bar at Bid open.
        out.entry_exec_price =
            entry_bid;

        // Exit decision after outcome close, execution next bar at Ask open.
        out.exit_exec_price =
            exit_bid + static_cast<double>(exit_spread) * point;

        out.pnl_price =
            out.entry_exec_price - out.exit_exec_price;
    }

    out.pnl_points =
        out.pnl_price / point;

    if (out.entry_exec_price != 0.0) {
        out.return_pct =
            100.0 * out.pnl_price / std::abs(out.entry_exec_price);
    }

    out.closed = true;
    return out;
}

} // namespace rza::canonical::trade_emulation
