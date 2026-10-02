// ============================================================================
// rank_writer.cpp — Реализация записи рейтинга в CSV
// ============================================================================

#include "discovery/ranking/rank_writer.h"
#include "io/csv_writer.h"
#include "core/buckets.h"
#include <algorithm>

namespace rza {

bool RankWriter::write(const std::string& file_path,
                        const std::vector<RankedPattern>& patterns)
{
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    // Заголовок
    std::vector<std::string> columns = {
        "PatternType", "ZoneType", "AgeBucket",
        "Samples", "LocalReversal", "LocalBreakout", "DecisiveLocal",
        "StructOpposite", "StructSameType", "DecisiveStruct",
        "LocalWilson", "StructWilson",
        "ParentSamples", "ParentLocalWilson", "ParentStructWilson",
        "LocalImprovementPP", "StructImprovementPP",
        "AvgMfe", "AvgMae", "MfeMaeRatio",
        "FinalScore", "PassesFilter", "FilterReason"
    };
    writer.write_header(columns);

    // Сортировка по финальному скору (убывание)
    std::vector<RankedPattern> sorted = patterns;
    std::sort(sorted.begin(), sorted.end(),
              [](const RankedPattern& a, const RankedPattern& b) {
                  return a.final_score > b.final_score;
              });

    for (const auto& rp : sorted) {
        writer.begin_row();

        // Тип паттерна
        std::string type_str;
        switch (rp.key.pattern_type) {
            case PatternType::AGE_ONLY:        type_str = "AGE_ONLY"; break;
            case PatternType::AGE_TOUCH:       type_str = "AGE_TOUCH"; break;
            case PatternType::AGE_DEPTH:       type_str = "AGE_DEPTH"; break;
            case PatternType::AGE_TOUCH_DEPTH: type_str = "AGE_TOUCH_DEPTH"; break;
            case PatternType::AGE_APPROACH:    type_str = "AGE_APPROACH"; break;
            case PatternType::AGE_HEIGHT:      type_str = "AGE_HEIGHT"; break;
            case PatternType::AGE_SESSION:     type_str = "AGE_SESSION"; break;
            default:                           type_str = "UNKNOWN"; break;
        }
        writer.add_field(type_str);
        writer.add_field(rp.key.zone_type == ZoneType::BULL ? "BULL" : "BEAR");
        writer.add_field(age_bucket_name(rp.key.age_bucket));

        // Статистика
        writer.add_field(rp.stats.samples);
        writer.add_field(rp.stats.local_reversal);
        writer.add_field(rp.stats.local_breakout);
        writer.add_field(rp.stats.decisive_local());
        writer.add_field(rp.stats.struct_opposite);
        writer.add_field(rp.stats.struct_same_type);
        writer.add_field(rp.stats.decisive_struct());

        // Wilson
        writer.add_field_or_blank(rp.local_wilson, 2);
        writer.add_field_or_blank(rp.struct_wilson, 2);

        // Родитель
        writer.add_field(rp.has_parent ? rp.parent_stats.samples : 0);
        writer.add_field_or_blank(rp.parent_local_wilson, 2);
        writer.add_field_or_blank(rp.parent_struct_wilson, 2);

        // Improvement
        writer.add_field_or_blank(rp.local_improvement_pp, 2);
        writer.add_field_or_blank(rp.struct_improvement_pp, 2);

        // MFE/MAE
        writer.add_field_or_blank(rp.avg_mfe, 4);
        writer.add_field_or_blank(rp.avg_mae, 4);
        writer.add_field_or_blank(rp.mfe_mae_ratio, 4);

        // Финальный скор и фильтр
        writer.add_field(rp.final_score, 2);
        writer.add_field_bool(rp.passes_filter);
        writer.add_field(rp.filter_reason);

        writer.end_row();
    }

    writer.close();
    return true;
}

} // namespace rza