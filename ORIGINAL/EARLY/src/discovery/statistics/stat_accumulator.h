#pragma once
// ============================================================================
// stat_accumulator.h — Накопитель статистики по паттернам (Блок 03)
// ============================================================================
// Одна ответственность: хранение и обновление статистики для каждого
// паттерна (комбинации бакетов).
// ============================================================================

#include <cstdint>
#include <unordered_map>
#include <string>
#include "core/types.h"

namespace rza {

// ============================================================================
// Ключ паттерна (компактный, для хэш-таблицы)
// ============================================================================
struct PatternKey {
    PatternType  pattern_type = PatternType::AGE_ONLY;
    ZoneType     zone_type = ZoneType::BULL;
    int          age_bucket = 0;
    int          touch_bucket = -1;
    int          depth_bucket = -1;
    int          approach_bucket = -1;
    int          height_bucket = -1;
    int          session_bucket = -1;

    bool operator==(const PatternKey& o) const {
        return pattern_type == o.pattern_type &&
               zone_type == o.zone_type &&
               age_bucket == o.age_bucket &&
               touch_bucket == o.touch_bucket &&
               depth_bucket == o.depth_bucket &&
               approach_bucket == o.approach_bucket &&
               height_bucket == o.height_bucket &&
               session_bucket == o.session_bucket;
    }

    uint64_t to_uint64() const {
        uint64_t k = 0;
        k |= (uint64_t)(int)pattern_type;
        k |= (uint64_t)(int)zone_type    << 4;
        k |= (uint64_t)(age_bucket & 0xF) << 8;
        k |= (uint64_t)((touch_bucket + 1) & 0xF) << 12;
        k |= (uint64_t)((depth_bucket + 1) & 0xF) << 16;
        k |= (uint64_t)((approach_bucket + 1) & 0xF) << 20;
        k |= (uint64_t)((height_bucket + 1) & 0xF) << 24;
        k |= (uint64_t)((session_bucket + 1) & 0xF) << 28;
        return k;
    }
};

struct PatternKeyHash {
    size_t operator()(const PatternKey& k) const {
        return std::hash<uint64_t>{}(k.to_uint64());
    }
};

// ============================================================================
// Статистика паттерна (накопитель)
// ============================================================================
struct PatternStats {
    int32_t  samples = 0;
    int32_t  local_reversal = 0;
    int32_t  local_breakout = 0;
    int32_t  struct_opposite = 0;
    int32_t  struct_same_type = 0;
    int32_t  struct_targets_broken = 0;
    int32_t  struct_timeout = 0;
    int32_t  local_timeout = 0;

    double   sum_mfe = 0.0;
    int32_t  count_mfe = 0;
    double   sum_mae = 0.0;
    int32_t  count_mae = 0;

    int decisive_local() const { return local_reversal + local_breakout; }
    int decisive_struct() const { return struct_opposite + struct_same_type; }
};

// ============================================================================
// Накопитель статистики
// ============================================================================
class StatAccumulator {
public:
    // Добавить касание в статистику паттерна
    void add_touch(const PatternKey& key, const std::string& local_result,
                   const std::string& struct_result,
                   double mfe_atr, double mae_atr);

    // Получить статистику по ключу
    const PatternStats* get_stats(const PatternKey& key) const;

    // Получить все накопленные паттерны
    const std::unordered_map<PatternKey, PatternStats, PatternKeyHash>& all_stats() const;

    // Очистить накопитель
    void clear();

    // Количество уникальных паттернов
    size_t pattern_count() const;

    // Общее количество касаний
    int32_t total_samples() const;

private:
    std::unordered_map<PatternKey, PatternStats, PatternKeyHash> stats_;
};

} // namespace rza