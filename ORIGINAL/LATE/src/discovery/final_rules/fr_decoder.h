#pragma once
// ============================================================================
// fr_decoder.h — Декодирование бакетов в человекочитаемые правила
// ============================================================================
// ИЗМЕНЕНО: добавлены числовые границы для строгой логики исполнения
// ============================================================================

#include <string>
#include <vector>
#include "core/types.h"
#include "discovery/statistics/stat_accumulator.h"
#include "discovery/final_rules/fr_validator.h"

namespace rza {

// ============================================================================
// Человекочитаемое финальное правило
// ============================================================================
struct FinalRule {
    std::string  rule_id;
    PatternKey   pattern_key;

    // Человекочитаемое описание
    std::string  zone_type_str;
    std::string  age_range_str;
    std::string  touch_range_str;
    std::string  depth_range_str;
    std::string  approach_range_str;
    std::string  height_range_str;
    std::string  session_range_str;

    // Полное описание
    std::string  full_description;

    // Сигнал
    std::string  direction;
    std::string  target_mode;

    // Метрики
    double       local_wilson = -1.0;
    double       struct_wilson = -1.0;
    double       local_pct = -1.0;
    double       struct_pct = -1.0;
    double       mfe_mae_ratio = -1.0;
    int          oos_samples = 0;

    // Валидация
    std::string  grade;
    bool         accepted = false;

    // НОВОЕ: числовые границы для строгой логики исполнения
    // Возраст зоны (минуты)
    double       age_min_minutes = 0.0;
    double       age_max_minutes = -1.0;  // -1 = без ограничения

    // Номер касания
    int          touch_min = -1;          // -1 = не задан (любой)
    int          touch_max = -1;          // -1 = без ограничения

    // Глубина входа (%)
    double       depth_min_pct = -1.0;    // -1 = не задан
    double       depth_max_pct = -1.0;    // -1 = без ограничения

    // Скорость подхода (ATR5)
    double       approach_min_atr = -1.0; // -1 = не задан
    double       approach_max_atr = -1.0; // -1 = без ограничения

    // Высота зоны (ATR)
    double       height_min_atr = -1.0;   // -1 = не задан
    double       height_max_atr = -1.0;   // -1 = без ограничения

    // Сессия (часы серверного времени)
    int          session_start_hour = -1; // -1 = не задана
    int          session_end_hour = -1;
};

// ============================================================================
// Класс для декодирования
// ============================================================================
class FRDecoder {
public:
    // Декодировать один паттерн в правило
    static FinalRule decode(
        const FinalRuleValidation& validation);

    // Декодировать все валидные паттерны
    static std::vector<FinalRule> decode_all(
        const std::vector<FinalRuleValidation>& validations,
        bool only_accepted = true);

private:
    // Построить человекочитаемую строку для бакета
    static std::string age_range_to_string(int age_bucket);
    static std::string touch_range_to_string(int touch_bucket);
    static std::string depth_range_to_string(int depth_bucket);
    static std::string approach_range_to_string(int approach_bucket);
    static std::string height_range_to_string(int height_bucket);
    static std::string session_range_to_string(int session_bucket);

    // НОВОЕ: заполнить числовые границы
    static void fill_numeric_bounds(FinalRule& rule);

    // Построить полное описание
    static std::string build_full_description(const FinalRule& rule);

    // Определить направление сигнала
    static std::string determine_direction(ZoneType zone);

    // Определить режим цели
    static std::string determine_target_mode(PatternType type);
};

} // namespace rza