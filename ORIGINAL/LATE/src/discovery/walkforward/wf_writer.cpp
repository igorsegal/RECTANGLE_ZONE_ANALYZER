// ============================================================================
// wf_writer.cpp — Реализация записи Walk-Forward в CSV
// ============================================================================

#include "discovery/walkforward/wf_writer.h"
#include "io/csv_writer.h"
#include "core/buckets.h"
#include <vector>
#include <algorithm>

namespace rza {

static std::string pattern_type_str(PatternType type) {
    switch (type) {
        case PatternType::AGE_ONLY:        return "AGE_ONLY";
        case PatternType::AGE_TOUCH:       return "AGE_TOUCH";
        case PatternType::AGE_DEPTH:       return "AGE_DEPTH";
        case PatternType::AGE_TOUCH_DEPTH: return "AGE_TOUCH_DEPTH";
        case PatternType::AGE_APPROACH:    return "AGE_APPROACH";
        case PatternType::AGE_HEIGHT:      return "AGE_HEIGHT";
        case PatternType::AGE_SESSION:     return "AGE_SESSION";
        default:                           return "UNKNOWN";
    }
}

bool WFWriter::write_summary(const std::string& file_path,
                              const WFResult& result)
{
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    std::vector<std::string> columns = {
        "PatternType", "ZoneType", "AgeBucket",
        "TotalFolds", "TrainEligibleFolds", "OOSPassedFolds",
        "AvgTrainLocalWilson", "AvgTrainStructWilson",
        "AvgOOSLocalPct", "AvgOOSStructPct",
        "PooledOOS_Samples", "PooledOOS_LocalReversal", "PooledOOS_LocalBreakout",
        "PooledOOS_StructOpposite", "PooledOOS_StructSameType",
        "Grade", "Accepted"
    };
    writer.write_header(columns);

    // Сортировка по количеству oos_passed_folds (убывание)
    std::vector<WFPatternSummary> sorted = result.pattern_summaries;
    std::sort(sorted.begin(), sorted.end(),
              [](const WFPatternSummary& a, const WFPatternSummary& b) {
                  if (a.oos_passed_folds != b.oos_passed_folds)
                      return a.oos_passed_folds > b.oos_passed_folds;
                  return a.train_eligible_folds > b.train_eligible_folds;
              });

    for (const auto& s : sorted) {
        writer.begin_row();
        writer.add_field(pattern_type_str(s.pattern_key.pattern_type));
        writer.add_field(s.pattern_key.zone_type == ZoneType::BULL ? "BULL" : "BEAR");
        writer.add_field(age_bucket_name(s.pattern_key.age_bucket));

        writer.add_field(s.total_folds);
        writer.add_field(s.train_eligible_folds);
        writer.add_field(s.oos_passed_folds);

        writer.add_field_or_blank(s.avg_train_local_wilson, 2);
        writer.add_field_or_blank(s.avg_train_struct_wilson, 2);
        writer.add_field_or_blank(s.avg_oos_local_pct, 2);
        writer.add_field_or_blank(s.avg_oos_struct_pct, 2);

        writer.add_field(s.pooled_oos_stats.samples);
        writer.add_field(s.pooled_oos_stats.local_reversal);
        writer.add_field(s.pooled_oos_stats.local_breakout);
        writer.add_field(s.pooled_oos_stats.struct_opposite);
        writer.add_field(s.pooled_oos_stats.struct_same_type);

        writer.add_field(s.grade);
        writer.add_field_bool(s.accepted);

        writer.end_row();
    }

    writer.close();
    return true;
}

bool WFWriter::write_folds(const std::string& file_path,
                            const WFResult& result)
{
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    std::vector<std::string> columns = {
        "FoldIndex", "PatternType", "ZoneType", "AgeBucket",
        "TrainSamples", "TrainLocalWilson", "TrainStructWilson",
        "TrainEligible", "TrainFailReason",
        "OOSSamples", "OOSLocalPct", "OSStructPct",
        "OOSPassed", "OOSFailReason",
        "FoldPass"
    };
    writer.write_header(columns);

    for (const auto& fr : result.fold_results) {
        writer.begin_row();
        writer.add_field(fr.fold_index);
        writer.add_field(pattern_type_str(fr.pattern_key.pattern_type));
        writer.add_field(fr.pattern_key.zone_type == ZoneType::BULL ? "BULL" : "BEAR");
        writer.add_field(age_bucket_name(fr.pattern_key.age_bucket));

        writer.add_field(fr.train_stats.samples);
        writer.add_field_or_blank(fr.train_local_wilson, 2);
        writer.add_field_or_blank(fr.train_struct_wilson, 2);
        writer.add_field_bool(fr.train_eligible);
        writer.add_field(fr.train_fail_reason);

        writer.add_field(fr.oos_stats.samples);
        writer.add_field_or_blank(fr.oos_local_pct, 2);
        writer.add_field_or_blank(fr.oos_struct_pct, 2);
        writer.add_field_bool(fr.oos_passed);
        writer.add_field(fr.oos_fail_reason);

        writer.add_field_bool(fr.fold_pass);

        writer.end_row();
    }

    writer.close();
    return true;
}

} // namespace rza