#pragma once
// ============================================================================
// stat_aggregator.h — Агрегация касаний в паттерны (Блок 03)
// ============================================================================
// Одна ответственность: для каждого касания вычислить ключи всех
// 7 типов паттернов и добавить в накопитель.
// ============================================================================

#include <vector>
#include "core/types.h"
#include "core/buckets.h"
#include "discovery/touches/touch_types.h"
#include "discovery/statistics/stat_accumulator.h"

namespace rza {

class StatAggregator {
public:
    // Агрегировать все касания в накопитель статистики
    static StatAccumulator aggregate(const std::vector<TouchRecord>& touches,
                                     int asia_end_hour = 7,
                                     int london_end_hour = 13,
                                     int newyork_end_hour = 21);

private:
    // Создать ключи всех 7 типов паттернов для одного касания
    static void create_pattern_keys(const TouchRecord& touch,
                                    int asia_end_hour,
                                    int london_end_hour,
                                    int newyork_end_hour,
                                    std::vector<PatternKey>& keys);

    // Вспомогательные функции для создания ключей
    static PatternKey make_age_only_key(ZoneType zone, int age_bucket);
    static PatternKey make_age_touch_key(ZoneType zone, int age_bucket, int touch_bucket);
    static PatternKey make_age_depth_key(ZoneType zone, int age_bucket, int depth_bucket);
    static PatternKey make_age_touch_depth_key(ZoneType zone, int age_bucket,
                                               int touch_bucket, int depth_bucket);
    static PatternKey make_age_approach_key(ZoneType zone, int age_bucket, int approach_bucket);
    static PatternKey make_age_height_key(ZoneType zone, int age_bucket, int height_bucket);
    static PatternKey make_age_session_key(ZoneType zone, int age_bucket, int session_bucket);
};

} // namespace rza