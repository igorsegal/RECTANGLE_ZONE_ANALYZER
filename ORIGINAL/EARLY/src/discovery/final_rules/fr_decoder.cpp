// ============================================================================
// fr_decoder.cpp — Реализация декодирования
// ============================================================================
// ИЗМЕНЕНО: добавлено заполнение числовых границ для строгой логики
// ============================================================================

#include "discovery/final_rules/fr_decoder.h"
#include "core/buckets.h"
#include "utils/math_utils.h"
#include <sstream>

namespace rza {

// ============================================================================
// Человекочитаемые строки для бакетов
// ============================================================================

std::string FRDecoder::age_range_to_string(int age_bucket) {
    double min_min, max_min;
    if (!get_age_bounds(age_bucket, min_min, max_min)) {
        return "UNKNOWN";
    }
    std::ostringstream oss;
    oss << "[" << min_min;
    if (max_min < 0.0) {
        oss << ", +inf) min";
    } else {
        oss << ", " << max_min << ") min";
    }
    return oss.str();
}

std::string FRDecoder::touch_range_to_string(int touch_bucket) {
    if (touch_bucket < 0) return "ALL";
    int min_val, max_val;
    if (!get_touch_bounds(touch_bucket, min_val, max_val)) {
        return "UNKNOWN";
    }
    std::ostringstream oss;
    oss << "[" << min_val;
    if (max_val < 0) {
        oss << ", +inf)";
    } else {
        oss << ", " << max_val << "]";
    }
    return oss.str();
}

std::string FRDecoder::depth_range_to_string(int depth_bucket) {
    if (depth_bucket < 0) return "ALL";
    double min_pct, max_pct;
    if (!get_depth_bounds(depth_bucket, min_pct, max_pct)) {
        return "UNKNOWN";
    }
    std::ostringstream oss;
    oss << "[" << min_pct << "%, " << max_pct << "%]";
    return oss.str();
}

std::string FRDecoder::approach_range_to_string(int approach_bucket) {
    if (approach_bucket < 0) return "ALL";
    double min_atr, max_atr;
    if (!get_approach_bounds(approach_bucket, min_atr, max_atr)) {
        return "UNKNOWN";
    }
    std::ostringstream oss;
    if (min_atr < 0.0) {
        oss << "FLAT_OR_AWAY";
    } else if (max_atr < 0.0) {
        oss << "[" << min_atr << ", +inf) ATR5";
    } else {
        oss << "[" << min_atr << ", " << max_atr << ") ATR5";
    }
    return oss.str();
}

std::string FRDecoder::height_range_to_string(int height_bucket) {
    if (height_bucket < 0) return "ALL";
    double min_atr, max_atr;
    if (!get_height_bounds(height_bucket, min_atr, max_atr)) {
        return "UNKNOWN";
    }
    std::ostringstream oss;
    if (max_atr < 0.0) {
        oss << "[" << min_atr << ", +inf) ATR";
    } else {
        oss << "[" << min_atr << ", " << max_atr << ") ATR";
    }
    return oss.str();
}

std::string FRDecoder::session_range_to_string(int session_bucket) {
    if (session_bucket < 0) return "ALL";
    return session_bucket_name(session_bucket);
}

// ============================================================================
// НОВОЕ: Заполнение числовых границ
// ============================================================================
void FRDecoder::fill_numeric_bounds(FinalRule& rule) {
    // Возраст
    get_age_bounds(rule.pattern_key.age_bucket,
                   rule.age_min_minutes, rule.age_max_minutes);

    // Номер касания
    if (rule.pattern_key.touch_bucket >= 0) {
        get_touch_bounds(rule.pattern_key.touch_bucket,
                         rule.touch_min, rule.touch_max);
    }

    // Глубина
    if (rule.pattern_key.depth_bucket >= 0) {
        get_depth_bounds(rule.pattern_key.depth_bucket,
                         rule.depth_min_pct, rule.depth_max_pct);
    }

    // Скорость подхода
    if (rule.pattern_key.approach_bucket >= 0) {
        get_approach_bounds(rule.pattern_key.approach_bucket,
                            rule.approach_min_atr, rule.approach_max_atr);
    }

    // Высота зоны
    if (rule.pattern_key.height_bucket >= 0) {
        get_height_bounds(rule.pattern_key.height_bucket,
                          rule.height_min_atr, rule.height_max_atr);
    }

    // Сессия (используем значения по умолчанию для серверного времени)
    if (rule.pattern_key.session_bucket >= 0) {
        get_session_bounds(rule.pattern_key.session_bucket,
                           7, 13, 21,  // default hours
                           rule.session_start_hour, rule.session_end_hour);
    }
}

// ============================================================================
// Направление сигнала
// ============================================================================
std::string FRDecoder::determine_direction(ZoneType zone) {
    return (zone == ZoneType::BULL) ? "BUY" : "SELL";
}

// ============================================================================
// Режим цели
// ============================================================================
std::string FRDecoder::determine_target_mode(PatternType type) {
    switch (type) {
        case PatternType::AGE_ONLY:
        case PatternType::AGE_TOUCH:
        case PatternType::AGE_DEPTH:
        case PatternType::AGE_TOUCH_DEPTH:
        case PatternType::AGE_APPROACH:
            return "PRIMARY_REACTION_DISTANCE";
        case PatternType::AGE_HEIGHT:
        case PatternType::AGE_SESSION:
            return "NEXT_OPPOSITE_ZONE";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// Полное описание
// ============================================================================
std::string FRDecoder::build_full_description(const FinalRule& rule) {
    std::ostringstream oss;
    oss << rule.zone_type_str << " zone, age " << rule.age_range_str;

    if (!rule.touch_range_str.empty() && rule.touch_range_str != "ALL") {
        oss << ", touch #" << rule.touch_range_str;
    }
    if (!rule.depth_range_str.empty() && rule.depth_range_str != "ALL") {
        oss << ", depth " << rule.depth_range_str;
    }
    if (!rule.approach_range_str.empty() && rule.approach_range_str != "ALL") {
        oss << ", approach " << rule.approach_range_str;
    }
    if (!rule.height_range_str.empty() && rule.height_range_str != "ALL") {
        oss << ", height " << rule.height_range_str;
    }
    if (!rule.session_range_str.empty() && rule.session_range_str != "ALL") {
        oss << ", session " << rule.session_range_str;
    }

    return oss.str();
}

// ============================================================================
// Декодирование одного паттерна
// ============================================================================
FinalRule FRDecoder::decode(const FinalRuleValidation& validation) {
    FinalRule rule;
    rule.pattern_key = validation.pattern_key;

    // Rule ID
    std::ostringstream id_oss;
    id_oss << "R_" << (int)validation.pattern_key.pattern_type
           << "_" << ((validation.pattern_key.zone_type == ZoneType::BULL) ? "B" : "S")
           << "_A" << validation.pattern_key.age_bucket;
    if (validation.pattern_key.touch_bucket >= 0)
        id_oss << "_T" << validation.pattern_key.touch_bucket;
    if (validation.pattern_key.depth_bucket >= 0)
        id_oss << "_D" << validation.pattern_key.depth_bucket;
    if (validation.pattern_key.approach_bucket >= 0)
        id_oss << "_AP" << validation.pattern_key.approach_bucket;
    if (validation.pattern_key.height_bucket >= 0)
        id_oss << "_H" << validation.pattern_key.height_bucket;
    if (validation.pattern_key.session_bucket >= 0)
        id_oss << "_S" << validation.pattern_key.session_bucket;
    rule.rule_id = id_oss.str();

    // Человекочитаемые поля
    rule.zone_type_str = (validation.pattern_key.zone_type == ZoneType::BULL) ? "BULL" : "BEAR";
    rule.age_range_str = age_range_to_string(validation.pattern_key.age_bucket);
    rule.touch_range_str = touch_range_to_string(validation.pattern_key.touch_bucket);
    rule.depth_range_str = depth_range_to_string(validation.pattern_key.depth_bucket);
    rule.approach_range_str = approach_range_to_string(validation.pattern_key.approach_bucket);
    rule.height_range_str = height_range_to_string(validation.pattern_key.height_bucket);
    rule.session_range_str = session_range_to_string(validation.pattern_key.session_bucket);

    // Сигнал
    rule.direction = determine_direction(validation.pattern_key.zone_type);
    rule.target_mode = determine_target_mode(validation.pattern_key.pattern_type);

    // Полное описание
    rule.full_description = build_full_description(rule);

    // Метрики
    rule.local_pct = validation.pooled_oos_local_pct;
    rule.struct_pct = validation.pooled_oos_struct_pct;
    rule.mfe_mae_ratio = validation.pooled_oos_mfe_mae_ratio;
    rule.oos_samples = validation.twf_summary.pooled_val_stats.samples;

    int decisive_local = validation.twf_summary.pooled_val_stats.decisive_local();
    int decisive_struct = validation.twf_summary.pooled_val_stats.decisive_struct();
    if (decisive_local > 0) {
        rule.local_wilson = wilson_lower_95(
            validation.twf_summary.pooled_val_stats.local_reversal, decisive_local);
    }
    if (decisive_struct > 0) {
        rule.struct_wilson = wilson_lower_95(
            validation.twf_summary.pooled_val_stats.struct_opposite, decisive_struct);
    }

    // Валидация
    rule.grade = validation.grade;
    rule.accepted = validation.fully_valid;

    // НОВОЕ: заполнение числовых границ
    fill_numeric_bounds(rule);

    return rule;
}

// ============================================================================
// Декодирование всех паттернов
// ============================================================================
std::vector<FinalRule> FRDecoder::decode_all(
    const std::vector<FinalRuleValidation>& validations,
    bool only_accepted)
{
    std::vector<FinalRule> result;
    result.reserve(validations.size());

    for (const auto& v : validations) {
        if (only_accepted && !v.fully_valid) continue;
        result.push_back(decode(v));
    }

    return result;
}

} // namespace rza