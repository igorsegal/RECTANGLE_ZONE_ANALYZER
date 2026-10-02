#include "formation_detector.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using rza::canonical::Bar;
using rza::canonical::Direction;
using rza::canonical::FormationType;
using rza::canonical::detect_at;

namespace {

int failures = 0;

void require(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
    } else {
        std::cout << "[FAIL] " << name << "\n";
        ++failures;
    }
}

bool near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

Bar bar(double o, double h, double l, double c) {
    Bar b;
    b.open = o;
    b.high = h;
    b.low = l;
    b.close = c;
    return b;
}

void test_bullish_engulf_2() {
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 102.0, 89.5, 101.0)
    };

    const auto e = detect_at(bars, 1);
    require(e.has_value(), "ENGULF_2 bullish detected");
    if (!e) return;

    require(e->type == FormationType::ENGULF_2, "ENGULF_2 bullish type");
    require(e->direction == Direction::BULLISH, "ENGULF_2 bullish direction");
    require(near(e->zone_low, 89.0) && near(e->zone_high, 101.0),
            "ENGULF_2 zone is source full range");
    require(!e->has_first_bar_progress,
            "ENGULF_2 has no intermediate progress");
}

void test_bearish_engulf_2() {
    std::vector<Bar> bars = {
        bar(90.0, 101.0, 89.0, 100.0),
        bar(100.0, 100.5, 88.0, 89.0)
    };

    const auto e = detect_at(bars, 1);
    require(e.has_value(), "ENGULF_2 bearish detected");
    if (!e) return;

    require(e->type == FormationType::ENGULF_2, "ENGULF_2 bearish type");
    require(e->direction == Direction::BEARISH, "ENGULF_2 bearish direction");
}

void test_bullish_engulf_3_progress_90() {
    // B0: bearish body 100 -> 90.
    // B1 closes at 99: reclaimed 9 of 10 = 90%, but has not crossed 100.
    // B2 is small, but closes at 100.20 and completes the engulfing.
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 99.2, 89.5, 99.0),
        bar(99.0, 100.3, 98.8, 100.2)
    };

    const auto e = detect_at(bars, 2);
    require(e.has_value(), "ENGULF_3 bullish detected");
    if (!e) return;

    require(e->type == FormationType::ENGULF_3, "ENGULF_3 bullish type");
    require(e->direction == Direction::BULLISH, "ENGULF_3 bullish direction");
    require(e->has_intermediate && e->intermediate_index == 1,
            "ENGULF_3 intermediate bar recorded");
    require(e->has_first_bar_progress, "ENGULF_3 progress present");
    require(near(e->first_bar_progress_pct, 90.0),
            "ENGULF_3 bullish progress = 90%");
    require(near(e->zone_low, 89.0) && near(e->zone_high, 101.0),
            "ENGULF_3 zone remains source full range");
}

void test_bearish_engulf_3_progress_90() {
    // Mirror case: B0 bullish body 90 -> 100.
    // B1 closes at 91: reclaimed 9 of 10 = 90%.
    // B2 closes below 90 and completes the engulfing.
    std::vector<Bar> bars = {
        bar(90.0, 101.0, 89.0, 100.0),
        bar(100.0, 100.5, 90.8, 91.0),
        bar(91.0, 91.2, 89.5, 89.8)
    };

    const auto e = detect_at(bars, 2);
    require(e.has_value(), "ENGULF_3 bearish detected");
    if (!e) return;

    require(e->type == FormationType::ENGULF_3, "ENGULF_3 bearish type");
    require(e->direction == Direction::BEARISH, "ENGULF_3 bearish direction");
    require(near(e->first_bar_progress_pct, 90.0),
            "ENGULF_3 bearish progress = 90%");
}

void test_three_bar_does_not_exist_until_b2_closes() {
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 99.2, 89.5, 99.0),
        bar(99.0, 99.9, 98.8, 99.8)
    };

    require(!detect_at(bars, 1).has_value(),
            "ENGULF_3 is not known after incomplete B1");
    require(!detect_at(bars, 2).has_value(),
            "ENGULF_3 rejected if B2 still does not complete");
}

void test_first_bar_that_already_completes_is_two_bar_only() {
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 101.5, 89.5, 101.0),
        bar(101.0, 102.0, 100.5, 101.5)
    };

    const auto e1 = detect_at(bars, 1);
    require(e1.has_value() && e1->type == FormationType::ENGULF_2,
            "Completed B1 is classified as ENGULF_2");

    const auto e2 = detect_at(bars, 2);
    require(!e2.has_value(),
            "Completed B1 is not reclassified as ENGULF_3");
}

void test_progress_is_feature_not_filter() {
    // First bullish bar barely advances: 10%.
    // Event must still be accepted as ENGULF_3; 10% is data for later research.
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 91.2, 89.5, 91.0),
        bar(91.0, 100.4, 90.5, 100.2)
    };

    const auto e = detect_at(bars, 2);
    require(e.has_value() && e->type == FormationType::ENGULF_3,
            "Low-progress ENGULF_3 is not filtered out");
    if (e) {
        require(near(e->first_bar_progress_pct, 10.0),
                "Low progress retained as 10% feature");
    }
}

} // namespace

int main() {
    std::cout << "============================================\n";
    std::cout << "RZA CANONICAL BLOCK 01 - FORMATION DETECTOR\n";
    std::cout << "============================================\n";

    test_bullish_engulf_2();
    test_bearish_engulf_2();
    test_bullish_engulf_3_progress_90();
    test_bearish_engulf_3_progress_90();
    test_three_bar_does_not_exist_until_b2_closes();
    test_first_bar_that_already_completes_is_two_bar_only();
    test_progress_is_feature_not_filter();

    std::cout << "--------------------------------------------\n";
    if (failures == 0) {
        std::cout << "BLOCK01 PASS\n";
        return EXIT_SUCCESS;
    }

    std::cout << "BLOCK01 FAIL failures=" << failures << "\n";
    return EXIT_FAILURE;
}
