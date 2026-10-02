// ============================================================================
// stat_accumulator.cpp — Реализация накопителя статистики
// ============================================================================

#include "discovery/statistics/stat_accumulator.h"

namespace rza {

void StatAccumulator::add_touch(const PatternKey& key,
                                 const std::string& local_result,
                                 const std::string& struct_result,
                                 double mfe_atr, double mae_atr) {
    PatternStats& stats = stats_[key];
    stats.samples++;

    // Локальный результат
    if (local_result == "REVERSAL_FIRST") {
        stats.local_reversal++;
    } else if (local_result == "BREAKOUT_FIRST") {
        stats.local_breakout++;
    } else if (local_result == "TIMEOUT") {
        stats.local_timeout++;
    }

    // Структурный результат
    if (struct_result == "OPPOSITE_ZONE_FIRST") {
        stats.struct_opposite++;
    } else if (struct_result == "SAME_TYPE_ZONE_FIRST") {
        stats.struct_same_type++;
    } else if (struct_result == "TARGETS_BROKEN") {
        stats.struct_targets_broken++;
    } else if (struct_result == "TIMEOUT") {
        stats.struct_timeout++;
    }

    // MFE/MAE
    if (mfe_atr >= 0.0) {
        stats.sum_mfe += mfe_atr;
        stats.count_mfe++;
    }
    if (mae_atr >= 0.0) {
        stats.sum_mae += mae_atr;
        stats.count_mae++;
    }
}

const PatternStats* StatAccumulator::get_stats(const PatternKey& key) const {
    auto it = stats_.find(key);
    if (it == stats_.end()) return nullptr;
    return &it->second;
}

const std::unordered_map<PatternKey, PatternStats, PatternKeyHash>&
StatAccumulator::all_stats() const {
    return stats_;
}

void StatAccumulator::clear() {
    stats_.clear();
}

size_t StatAccumulator::pattern_count() const {
    return stats_.size();
}

int32_t StatAccumulator::total_samples() const {
    int32_t total = 0;
    for (const auto& pair : stats_) {
        total += pair.second.samples;
    }
    return total;
}

} // namespace rza