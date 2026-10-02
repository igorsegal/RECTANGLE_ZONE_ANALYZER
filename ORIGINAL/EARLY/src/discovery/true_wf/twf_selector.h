#pragma once
// ============================================================================
// twf_selector.h — Отбор паттернов в True Walk-Forward (Блок 06)
// ============================================================================
// Одна ответственность: для каждого окна отобрать паттерны, прошедшие
// opt-условия, и проверить их на val-окне.
// ============================================================================

#include <vector>
#include <string>
#include "core/params.h"
#include "discovery/statistics/stat_accumulator.h"
#include "discovery/touches/touch_types.h"
#include "discovery/true_wf/twf_window.h"

namespace rza {

// ============================================================================
// Результат для одного паттерна в одном окне
// ============================================================================
struct TWFWindowPatternResult {
    int          window_index = -1;
    PatternKey   pattern_key;

    // OPT статистика
    PatternStats opt_stats;
    double       opt_local_wilson = -1.0;
    double       opt_struct_wilson = -1.0;
    double       opt_local_pct = -1.0;
    double       opt_struct_pct = -1.0;
    bool         opt_eligible = false;
    std::string  opt_fail_reason;

    // VAL статистика
    PatternStats val_stats;
    double       val_local_wilson = -1.0;
    double       val_struct_wilson = -1.0;
    double       val_local_pct = -1.0;
    double       val_struct_pct = -1.0;
    bool         val_passed = false;
    std::string  val_fail_reason;

    // Итог
    bool         window_pass = false;
};

// ============================================================================
// Класс для отбора паттернов
// ============================================================================
class TWFSelector {
public:
    // Оценить один паттерн в одном окне
    static TWFWindowPatternResult evaluate_pattern_in_window(
        const PatternKey& pattern_key,
        const std::vector<TouchRecord>& opt_touches,
        const std::vector<TouchRecord>& val_touches,
        int window_index,
        const WalkForwardParams& params);

    // Проверить opt-условия
    static bool passes_opt(const PatternStats& stats,
                            const WalkForwardParams& params,
                            std::string& reason);

    // Проверить val-условия
    static bool passes_val(const PatternStats& opt_stats,
                            const PatternStats& val_stats,
                            const WalkForwardParams& params,
                            std::string& reason);
};

} // namespace rza