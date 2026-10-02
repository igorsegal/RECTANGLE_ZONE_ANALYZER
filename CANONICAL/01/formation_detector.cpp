#include "formation_detector.h"

#include <algorithm>

namespace rza::canonical {

bool is_bullish(const Bar& bar) noexcept {
    return bar.close > bar.open;
}

bool is_bearish(const Bar& bar) noexcept {
    return bar.close < bar.open;
}

static double clamp_0_100(double value) noexcept {
    return std::max(0.0, std::min(100.0, value));
}

static FormationEvent make_two_bar(
    const Bar& source,
    std::size_t source_index,
    std::size_t confirmation_index,
    Direction direction)
{
    FormationEvent event;
    event.type = FormationType::ENGULF_2;
    event.direction = direction;
    event.source_index = source_index;
    event.confirmation_index = confirmation_index;
    event.zone_low = source.low;
    event.zone_high = source.high;
    return event;
}

static FormationEvent make_three_bar(
    const Bar& source,
    const Bar& intermediate,
    std::size_t source_index,
    std::size_t intermediate_index,
    std::size_t confirmation_index,
    Direction direction)
{
    FormationEvent event;
    event.type = FormationType::ENGULF_3;
    event.direction = direction;
    event.source_index = source_index;
    event.confirmation_index = confirmation_index;
    event.has_intermediate = true;
    event.intermediate_index = intermediate_index;
    event.zone_low = source.low;
    event.zone_high = source.high;

    // Legacy diagnostic fields are retained for compatibility with old
    // Block02 artifacts, but current research does not use progress.
    event.has_first_bar_progress = true;

    double raw_progress = 0.0;

    if (direction == Direction::BULLISH) {
        const double source_body = source.open - source.close;
        if (source_body > 0.0) {
            raw_progress =
                100.0 * (intermediate.close - source.close) / source_body;
        }
    } else {
        const double source_body = source.close - source.open;
        if (source_body > 0.0) {
            raw_progress =
                100.0 * (source.close - intermediate.close) / source_body;
        }
    }

    event.first_bar_progress_raw_pct = raw_progress;
    event.first_bar_progress_pct = clamp_0_100(raw_progress);
    return event;
}

std::optional<FormationEvent> detect_at(
    const std::vector<Bar>& bars,
    std::size_t confirmation_index)
{
    if (confirmation_index >= bars.size()) {
        return std::nullopt;
    }

    // ABS_TRACK priority:
    // 1) first test the latest two closed bars;
    // 2) only if that test finds nothing, test the three-bar construction.
    if (confirmation_index >= 1) {
        const std::size_t source_index = confirmation_index - 1;
        const Bar& source = bars[source_index];
        const Bar& confirm = bars[confirmation_index];

        const bool bullish_complete =
            is_bearish(source) &&
            is_bullish(confirm) &&
            confirm.close > source.open;

        if (bullish_complete) {
            return make_two_bar(
                source,
                source_index,
                confirmation_index,
                Direction::BULLISH);
        }

        const bool bearish_complete =
            is_bullish(source) &&
            is_bearish(confirm) &&
            confirm.close < source.open;

        if (bearish_complete) {
            return make_two_bar(
                source,
                source_index,
                confirmation_index,
                Direction::BEARISH);
        }
    }

    // Exact ABS_TRACK three-bar candidate logic.
    // There is deliberately NO extra condition that the intermediate bar
    // must still be inside the source body. If the same source produced an
    // earlier live zone, the active-zone distance rule rejects the duplicate.
    if (confirmation_index >= 2) {
        const std::size_t source_index = confirmation_index - 2;
        const std::size_t intermediate_index = confirmation_index - 1;

        const Bar& source = bars[source_index];
        const Bar& intermediate = bars[intermediate_index];
        const Bar& confirm = bars[confirmation_index];

        const bool bullish_three =
            is_bearish(source) &&
            is_bullish(intermediate) &&
            is_bullish(confirm) &&
            confirm.close > source.open;

        if (bullish_three) {
            return make_three_bar(
                source,
                intermediate,
                source_index,
                intermediate_index,
                confirmation_index,
                Direction::BULLISH);
        }

        const bool bearish_three =
            is_bullish(source) &&
            is_bearish(intermediate) &&
            is_bearish(confirm) &&
            confirm.close < source.open;

        if (bearish_three) {
            return make_three_bar(
                source,
                intermediate,
                source_index,
                intermediate_index,
                confirmation_index,
                Direction::BEARISH);
        }
    }

    return std::nullopt;
}

std::vector<FormationEvent> detect_all(const std::vector<Bar>& bars) {
    std::vector<FormationEvent> events;
    events.reserve(bars.size() / 8 + 1);

    for (std::size_t i = 0; i < bars.size(); ++i) {
        auto event = detect_at(bars, i);
        if (event.has_value()) {
            events.push_back(*event);
        }
    }

    return events;
}

} // namespace rza::canonical
