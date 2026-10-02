#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rza::canonical {

struct Bar {
    std::int64_t time = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;

    // Historical XFBAR spread in broker points.
    // Synthetic/unit-test bars leave this at 0.
    std::int32_t spread = 0;
};

enum class FormationType {
    ENGULF_2,
    ENGULF_3
};

enum class Direction {
    BULLISH,
    BEARISH
};

struct FormationEvent {
    FormationType type = FormationType::ENGULF_2;
    Direction direction = Direction::BULLISH;

    std::size_t source_index = 0;
    std::size_t confirmation_index = 0;

    // Для ENGULF_2 отсутствует.
    bool has_intermediate = false;
    std::size_t intermediate_index = 0;

    // Каноническая зона всегда строится по полному диапазону source-бара.
    double zone_low = 0.0;
    double zone_high = 0.0;

    // Только для ENGULF_3.
    // raw: фактический прогресс B1 относительно тела B0.
    // clamped: та же величина, ограниченная диапазоном [0, 100].
    bool has_first_bar_progress = false;
    double first_bar_progress_raw_pct = 0.0;
    double first_bar_progress_pct = 0.0;
};

bool is_bullish(const Bar& bar) noexcept;
bool is_bearish(const Bar& bar) noexcept;

// Определяет событие, которое стало известно только после закрытия bars[confirmation_index].
// Семантика поглощения сохраняет исходное правило RZA:
// bullish complete  <=> confirm.close > source.open
// bearish complete  <=> confirm.close < source.open
std::optional<FormationEvent> detect_at(
    const std::vector<Bar>& bars,
    std::size_t confirmation_index);

std::vector<FormationEvent> detect_all(const std::vector<Bar>& bars);

} // namespace rza::canonical
