// ============================================================================
// stat_aggregator.cpp — Реализация агрегации касаний
// ============================================================================

#include "discovery/statistics/stat_aggregator.h"
#include <cmath>

namespace rza {

PatternKey StatAggregator::make_age_only_key(ZoneType zone, int age_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_ONLY;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    return k;
}

PatternKey StatAggregator::make_age_touch_key(ZoneType zone, int age_bucket, int touch_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_TOUCH;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    k.touch_bucket = touch_bucket;
    return k;
}

PatternKey StatAggregator::make_age_depth_key(ZoneType zone, int age_bucket, int depth_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_DEPTH;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    k.depth_bucket = depth_bucket;
    return k;
}

PatternKey StatAggregator::make_age_touch_depth_key(ZoneType zone, int age_bucket,
                                                     int touch_bucket, int depth_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_TOUCH_DEPTH;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    k.touch_bucket = touch_bucket;
    k.depth_bucket = depth_bucket;
    return k;
}

PatternKey StatAggregator::make_age_approach_key(ZoneType zone, int age_bucket, int approach_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_APPROACH;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    k.approach_bucket = approach_bucket;
    return k;
}

PatternKey StatAggregator::make_age_height_key(ZoneType zone, int age_bucket, int height_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_HEIGHT;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    k.height_bucket = height_bucket;
    return k;
}

PatternKey StatAggregator::make_age_session_key(ZoneType zone, int age_bucket, int session_bucket) {
    PatternKey k;
    k.pattern_type = PatternType::AGE_SESSION;
    k.zone_type = zone;
    k.age_bucket = age_bucket;
    k.session_bucket = session_bucket;
    return k;
}

void StatAggregator::create_pattern_keys(const TouchRecord& touch,
                                          int asia_end_hour,
                                          int london_end_hour,
                                          int newyork_end_hour,
                                          std::vector<PatternKey>& keys) {
    keys.clear();
    keys.reserve(7);

    ZoneType zone = (touch.zone_type == "BULL") ? ZoneType::BULL : ZoneType::BEAR;

    // Вычисляем бакеты
    int age_bucket = get_age_bucket(touch.zone_age_calendar_minutes);
    int touch_bucket = get_touch_bucket(touch.touch_number);
    int depth_bucket = get_depth_bucket(touch.touch_depth_percent);
    int approach_bucket = get_approach_bucket(touch.approach_net_move_atr5);
    int height_bucket = get_height_bucket(touch.zone_height_atr);
    int session_bucket = get_session_bucket(touch.touch_bar_time,
                                            asia_end_hour, london_end_hour, newyork_end_hour);

    // Создаём 7 ключей паттернов
    keys.push_back(make_age_only_key(zone, age_bucket));
    keys.push_back(make_age_touch_key(zone, age_bucket, touch_bucket));
    keys.push_back(make_age_depth_key(zone, age_bucket, depth_bucket));
    keys.push_back(make_age_touch_depth_key(zone, age_bucket, touch_bucket, depth_bucket));
    keys.push_back(make_age_approach_key(zone, age_bucket, approach_bucket));
    keys.push_back(make_age_height_key(zone, age_bucket, height_bucket));
    keys.push_back(make_age_session_key(zone, age_bucket, session_bucket));
}

StatAccumulator StatAggregator::aggregate(const std::vector<TouchRecord>& touches,
                                           int asia_end_hour,
                                           int london_end_hour,
                                           int newyork_end_hour) {
    StatAccumulator acc;

    std::vector<PatternKey> keys;
    keys.reserve(7);

    for (const auto& touch : touches) {
        create_pattern_keys(touch, asia_end_hour, london_end_hour, newyork_end_hour, keys);

        for (const auto& key : keys) {
            acc.add_touch(key, touch.local_first_result, touch.structural_first_result,
                          touch.mfe_close_atr, touch.mae_close_atr);
        }
    }

    return acc;
}

} // namespace rza