#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../01/formation_detector.h"

namespace rza::canonical {

struct XfbarData {
    bool success = false;
    std::string error;

    std::string file_path;
    std::string symbol;
    int32_t period_seconds = 0;
    int32_t digits = 0;
    double point = 0.0;
    int64_t declared_bar_count = 0;
    int64_t first_time = 0;
    int64_t last_time = 0;

    std::vector<Bar> bars;
};

XfbarData read_xfbar(const std::string& file_path);
std::string timeframe_name(int32_t period_seconds);

} // namespace rza::canonical
