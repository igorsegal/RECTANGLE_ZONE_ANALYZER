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

void test_bullish_two_bar() {
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 102.0, 89.5, 101.0)
    };

    const auto e = detect_at(bars, 1);
    require(e.has_value(), "ABS_TRACK bullish two-bar detected");
    if (!e) return;

    require(e->type == FormationType::ENGULF_2, "two-bar technical origin");
    require(e->direction == Direction::BULLISH, "two-bar bullish direction");
}

void test_bearish_two_bar() {
    std::vector<Bar> bars = {
        bar(90.0, 101.0, 89.0, 100.0),
        bar(100.0, 100.5, 88.0, 89.0)
    };

    const auto e = detect_at(bars, 1);
    require(e.has_value(), "ABS_TRACK bearish two-bar detected");
    if (!e) return;

    require(e->direction == Direction::BEARISH, "two-bar bearish direction");
}

void test_bullish_three_bar() {
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 99.2, 89.5, 99.0),
        bar(99.0, 100.3, 98.8, 100.2)
    };

    const auto e = detect_at(bars, 2);
    require(e.has_value(), "ABS_TRACK bullish three-bar detected");
    if (!e) return;

    require(e->type == FormationType::ENGULF_3, "three-bar technical origin");
    require(e->direction == Direction::BULLISH, "three-bar bullish direction");
    require(e->source_index == 0 && e->confirmation_index == 2,
            "three-bar source and confirmation indices");
}

void test_bearish_three_bar() {
    std::vector<Bar> bars = {
        bar(90.0, 101.0, 89.0, 100.0),
        bar(100.0, 100.5, 90.8, 91.0),
        bar(91.0, 91.2, 89.5, 89.8)
    };

    const auto e = detect_at(bars, 2);
    require(e.has_value(), "ABS_TRACK bearish three-bar detected");
    if (!e) return;

    require(e->direction == Direction::BEARISH, "three-bar bearish direction");
}

void test_three_bar_requires_final_cross() {
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 99.2, 89.5, 99.0),
        bar(99.0, 99.9, 98.8, 99.8)
    };

    require(!detect_at(bars, 2).has_value(),
            "three-bar rejected until final close crosses source open");
}

void test_abs_track_allows_three_bar_candidate_after_prior_cross() {
    // B1 already completed a two-bar engulf of B0 on the previous decision.
    // ABS_TRACK can still recognize B0+B1+B2 as a three-bar candidate later.
    // The active-zone distance rule, not the detector, suppresses the duplicate.
    std::vector<Bar> bars = {
        bar(100.0, 101.0, 89.0, 90.0),
        bar(90.0, 101.5, 89.5, 101.0),
        bar(101.0, 102.0, 100.5, 101.5)
    };

    const auto first = detect_at(bars, 1);
    require(first.has_value() && first->type == FormationType::ENGULF_2,
            "prior decision detects two-bar zone");

    const auto later = detect_at(bars, 2);
    require(later.has_value() && later->type == FormationType::ENGULF_3,
            "later ABS_TRACK three-bar candidate is not artificially blocked");
    if (later) {
        require(later->source_index == 0,
                "later candidate keeps original source bar");
    }
}

void test_two_bar_priority_on_current_decision() {
    // Latest two bars form a bearish engulf. Even if older bars exist,
    // ABS_TRACK stops at the two-bar match and does not inspect three-bar logic.
    std::vector<Bar> bars = {
        bar(80.0, 82.0, 79.0, 81.0),
        bar(90.0, 101.0, 89.0, 100.0),
        bar(100.0, 100.5, 88.0, 89.0)
    };

    const auto e = detect_at(bars, 2);
    require(e.has_value() && e->type == FormationType::ENGULF_2,
            "two-bar has priority on current decision");
    if (e) {
        require(e->source_index == 1,
                "two-bar priority uses latest source bar");
    }
}

} // namespace

int main() {
    std::cout << "============================================\n";
    std::cout << "RZA CANONICAL BLOCK 01 - ABS_TRACK DETECTOR\n";
    std::cout << "============================================\n";

    test_bullish_two_bar();
    test_bearish_two_bar();
    test_bullish_three_bar();
    test_bearish_three_bar();
    test_three_bar_requires_final_cross();
    test_abs_track_allows_three_bar_candidate_after_prior_cross();
    test_two_bar_priority_on_current_decision();

    std::cout << "--------------------------------------------\n";
    if (failures == 0) {
        std::cout << "BLOCK01 PASS\n";
        return EXIT_SUCCESS;
    }

    std::cout << "BLOCK01 FAIL failures=" << failures << "\n";
    return EXIT_FAILURE;
}
