// ============================================================================
// stat_writer.cpp — Реализация записи статистики в CSV
// ============================================================================

#include "discovery/statistics/stat_writer.h"
#include "core/buckets.h"
#include <vector>
#include <algorithm>

namespace rza {

std::string StatWriter::pattern_type_str(PatternType type) {
    switch (type) {
        case PatternType::AGE_ONLY:          return "AGE_ONLY";
        case PatternType::AGE_TOUCH:         return "AGE_TOUCH";
        case PatternType::AGE_DEPTH:         return "AGE_DEPTH";
        case PatternType::AGE_TOUCH_DEPTH:   return "AGE_TOUCH_DEPTH";
        case PatternType::AGE_APPROACH:      return "AGE_APPROACH";
        case PatternType::AGE_HEIGHT:        return "AGE_HEIGHT";
        case PatternType::AGE_SESSION:       return "AGE_SESSION";
        default:                             return "UNKNOWN";
    }
}

std::string StatWriter::zone_type_str(ZoneType type) {
    return (type == ZoneType::BULL) ? "BULL" : "BEAR";
}

std::string StatWriter::bucket_name(PatternType type, int bucket) {
    if (bucket < 0) return "ALL";
    switch (type) {
        case PatternType::AGE_ONLY:          return age_bucket_name(bucket);
        case PatternType::AGE_TOUCH:         return age_bucket_name(bucket);
        case PatternType::AGE_DEPTH:         return age_bucket_name(bucket);
        case PatternType::AGE_TOUCH_DEPTH:   return age_bucket_name(bucket);
        case PatternType::AGE_APPROACH:      return age_bucket_name(bucket);
        case PatternType::AGE_HEIGHT:        return age_bucket_name(bucket);
        case PatternType::AGE_SESSION:       return age_bucket_name(bucket);
        default:                             return "UNKNOWN";
    }
}

void StatWriter::write_header(CsvWriter& writer) {
    std::vector<std::string> columns = {
        "PatternType", "ZoneType", "AgeBucket",
        "TouchBucket", "DepthBucket", "ApproachBucket",
        "HeightBucket", "SessionBucket",
        "Samples", "LocalReversal", "LocalBreakout", "LocalTimeout",
        "StructOpposite", "StructSameType", "StructTargetsBroken", "StructTimeout",
        "DecisiveLocal", "DecisiveStruct",
        "LocalReversalPct", "LocalBreakoutPct",
        "StructOppositePct", "StructSameTypePct",
        "AvgMfeAtr", "AvgMaeAtr", "MfeMaeRatio"
    };
    writer.write_header(columns);
}

void StatWriter::write_pattern(CsvWriter& writer,
                                const PatternKey& key,
                                const PatternStats& stats) {
    writer.begin_row();

    writer.add_field(pattern_type_str(key.pattern_type));
    writer.add_field(zone_type_str(key.zone_type));
    writer.add_field(age_bucket_name(key.age_bucket));

    // Дополнительные бакеты (только для соответствующих типов паттернов)
    if (key.pattern_type == PatternType::AGE_TOUCH ||
        key.pattern_type == PatternType::AGE_TOUCH_DEPTH) {
        writer.add_field(touch_bucket_name(key.touch_bucket));
    } else {
        writer.add_field("");
    }

    if (key.pattern_type == PatternType::AGE_DEPTH ||
        key.pattern_type == PatternType::AGE_TOUCH_DEPTH) {
        writer.add_field(depth_bucket_name(key.depth_bucket));
    } else {
        writer.add_field("");
    }

    if (key.pattern_type == PatternType::AGE_APPROACH) {
        writer.add_field(approach_bucket_name(key.approach_bucket));
    } else {
        writer.add_field("");
    }

    if (key.pattern_type == PatternType::AGE_HEIGHT) {
        writer.add_field(height_bucket_name(key.height_bucket));
    } else {
        writer.add_field("");
    }

    if (key.pattern_type == PatternType::AGE_SESSION) {
        writer.add_field(session_bucket_name(key.session_bucket));
    } else {
        writer.add_field("");
    }

    // Статистика
    writer.add_field(stats.samples);
    writer.add_field(stats.local_reversal);
    writer.add_field(stats.local_breakout);
    writer.add_field(stats.local_timeout);
    writer.add_field(stats.struct_opposite);
    writer.add_field(stats.struct_same_type);
    writer.add_field(stats.struct_targets_broken);
    writer.add_field(stats.struct_timeout);

    writer.add_field(stats.decisive_local());
    writer.add_field(stats.decisive_struct());

    // Проценты
    int decisive_local = stats.decisive_local();
    int decisive_struct = stats.decisive_struct();

    if (decisive_local > 0) {
        writer.add_field(100.0 * stats.local_reversal / decisive_local, 2);
        writer.add_field(100.0 * stats.local_breakout / decisive_local, 2);
    } else {
        writer.add_field("");
        writer.add_field("");
    }

    if (decisive_struct > 0) {
        writer.add_field(100.0 * stats.struct_opposite / decisive_struct, 2);
        writer.add_field(100.0 * stats.struct_same_type / decisive_struct, 2);
    } else {
        writer.add_field("");
        writer.add_field("");
    }

    // MFE/MAE
    if (stats.count_mfe > 0) {
        writer.add_field(stats.sum_mfe / stats.count_mfe, 4);
    } else {
        writer.add_field("");
    }

    if (stats.count_mae > 0) {
        writer.add_field(stats.sum_mae / stats.count_mae, 4);
    } else {
        writer.add_field("");
    }

    // MFE/MAE ratio
    if (stats.count_mae > 0 && stats.count_mfe > 0) {
        double avg_mfe = stats.sum_mfe / stats.count_mfe;
        double avg_mae = stats.sum_mae / stats.count_mae;
        if (avg_mae > 0.0) {
            writer.add_field(avg_mfe / avg_mae, 4);
        } else {
            writer.add_field("");
        }
    } else {
        writer.add_field("");
    }

    writer.end_row();
}

bool StatWriter::write(const std::string& file_path,
                        const StatAccumulator& acc) {
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    write_header(writer);

    // Сортируем паттерны для детерминированного вывода
    std::vector<std::pair<PatternKey, PatternStats>> sorted_stats(
        acc.all_stats().begin(), acc.all_stats().end());

    std::sort(sorted_stats.begin(), sorted_stats.end(),
              [](const auto& a, const auto& b) {
                  if (a.first.pattern_type != b.first.pattern_type)
                      return (int)a.first.pattern_type < (int)b.first.pattern_type;
                  if (a.first.zone_type != b.first.zone_type)
                      return (int)a.first.zone_type < (int)b.first.zone_type;
                  return a.first.age_bucket < b.first.age_bucket;
              });

    for (const auto& pair : sorted_stats) {
        write_pattern(writer, pair.first, pair.second);
    }

    writer.close();
    return true;
}

} // namespace rza