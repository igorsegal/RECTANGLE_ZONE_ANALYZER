// ============================================================================
// fr_writer.cpp — Реализация записи финальных правил в CSV
// ============================================================================

#include "discovery/final_rules/fr_writer.h"
#include "io/csv_writer.h"
#include <algorithm>

namespace rza {

bool FRWriter::write(const std::string& file_path,
                      const std::vector<FinalRule>& rules)
{
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    // Заголовок
    std::vector<std::string> columns = {
        "RuleID", "PatternType", "ZoneType",
        "AgeRange", "TouchRange", "DepthRange",
        "ApproachRange", "HeightRange", "SessionRange",
        "Direction", "TargetMode", "FullDescription",
        "LocalWilson", "StructWilson",
        "LocalPct", "StructPct", "MfeMaeRatio", "OOSSamples",
        "Grade", "Accepted"
    };
    writer.write_header(columns);

    // Сортировка по rule_id
    std::vector<FinalRule> sorted = rules;
    std::sort(sorted.begin(), sorted.end(),
              [](const FinalRule& a, const FinalRule& b) {
                  return a.rule_id < b.rule_id;
              });

    for (const auto& rule : sorted) {
        writer.begin_row();

        writer.add_field(rule.rule_id);

        std::string type_str;
        switch (rule.pattern_key.pattern_type) {
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
        writer.add_field(rule.zone_type_str);

        writer.add_field(rule.age_range_str);
        writer.add_field(rule.touch_range_str);
        writer.add_field(rule.depth_range_str);
        writer.add_field(rule.approach_range_str);
        writer.add_field(rule.height_range_str);
        writer.add_field(rule.session_range_str);

        writer.add_field(rule.direction);
        writer.add_field(rule.target_mode);
        writer.add_field(rule.full_description);

        writer.add_field_or_blank(rule.local_wilson, 2);
        writer.add_field_or_blank(rule.struct_wilson, 2);
        writer.add_field_or_blank(rule.local_pct, 2);
        writer.add_field_or_blank(rule.struct_pct, 2);
        writer.add_field_or_blank(rule.mfe_mae_ratio, 4);
        writer.add_field(rule.oos_samples);

        writer.add_field(rule.grade);
        writer.add_field_bool(rule.accepted);

        writer.end_row();
    }

    writer.close();
    return true;
}

} // namespace rza