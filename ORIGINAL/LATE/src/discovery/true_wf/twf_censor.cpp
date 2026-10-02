// ============================================================================
// twf_censor.cpp — Реализация цензурации
// ============================================================================

#include "discovery/true_wf/twf_censor.h"

namespace rza {

bool TWFCensor::in_range(const TouchRecord& touch,
                          int64_t start_time, int64_t end_time) {
    return touch.touch_bar_time >= start_time &&
           touch.touch_bar_time < end_time;
}

std::vector<TouchRecord> TWFCensor::censor_opt(
    const std::vector<TouchRecord>& all_touches,
    const TWFWindow& window)
{
    std::vector<TouchRecord> result;
    result.reserve(all_touches.size() / 4);  // примерная оценка

    for (const auto& touch : all_touches) {
        if (in_range(touch, window.opt_start_time, window.opt_end_time)) {
            result.push_back(touch);
        }
    }

    return result;
}

std::vector<TouchRecord> TWFCensor::censor_val(
    const std::vector<TouchRecord>& all_touches,
    const TWFWindow& window)
{
    std::vector<TouchRecord> result;
    result.reserve(all_touches.size() / 4);

    for (const auto& touch : all_touches) {
        if (in_range(touch, window.val_start_time, window.val_end_time)) {
            result.push_back(touch);
        }
    }

    return result;
}

std::vector<TouchRecord> TWFCensor::censor_window(
    const std::vector<TouchRecord>& all_touches,
    const TWFWindow& window)
{
    std::vector<TouchRecord> result;
    result.reserve(all_touches.size() / 2);

    for (const auto& touch : all_touches) {
        if (in_range(touch, window.window_start_time, window.window_end_time)) {
            result.push_back(touch);
        }
    }

    return result;
}

} // namespace rza