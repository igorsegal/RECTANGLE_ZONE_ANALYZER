#pragma once
// ============================================================================
// fr_validator.h — Валидация OOS для финальных правил (Блок 07)
// ============================================================================
// Одна ответственность: проверка, что паттерн соответствует требованиям
// FinalRulesParams и может быть включён в финальный отчёт.
// ============================================================================

#include <vector>
#include <string>
#include "core/params.h"
#include "discovery/true_wf/twf_summary.h"

namespace rza {

// ============================================================================
// Результат валидации одного паттерна
// ============================================================================
struct FinalRuleValidation {
    PatternKey   pattern_key;
    TWFPatternSummary twf_summary;

    bool         passes_grade = false;
    std::string  grade;

    bool         passes_oos_samples = false;
    bool         passes_local_decisive = false;
    bool         passes_local_improvement = false;
    bool         passes_local_wilson = false;
    bool         passes_local_mfe_mae = false;
    bool         passes_structural_decisive = false;
    bool         passes_structural_improvement = false;
    bool         passes_structural_wilson = false;

    bool         fully_valid = false;
    std::string  fail_reason;

    // Метрики
    double       pooled_oos_local_pct = -1.0;
    double       pooled_oos_struct_pct = -1.0;
    double       pooled_oos_mfe_mae_ratio = -1.0;
};

// ============================================================================
// Класс для валидации
// ============================================================================
class FRValidator {
public:
    // Валидировать один паттерн
    static FinalRuleValidation validate(
        const TWFPatternSummary& summary,
        const FinalRulesParams& params);

    // Валидировать все паттерны
    static std::vector<FinalRuleValidation> validate_all(
        const TWFResult& twf_result,
        const FinalRulesParams& params);

private:
    // Определить grade
    static std::string determine_grade(
        const TWFPatternSummary& summary,
        const FinalRulesParams& params);

    // Проверка отдельных условий
    static bool check_oos_samples(const TWFPatternSummary& summary,
                                   const FinalRulesParams& params);
    static bool check_local_decisive(const TWFPatternSummary& summary,
                                      const FinalRulesParams& params);
    static bool check_local_improvement(const TWFPatternSummary& summary,
                                         const FinalRulesParams& params);
    static bool check_local_wilson(const TWFPatternSummary& summary,
                                    const FinalRulesParams& params);
    static bool check_local_mfe_mae(const TWFPatternSummary& summary,
                                     const FinalRulesParams& params);
    static bool check_structural_decisive(const TWFPatternSummary& summary,
                                           const FinalRulesParams& params);
    static bool check_structural_improvement(const TWFPatternSummary& summary,
                                              const FinalRulesParams& params);
    static bool check_structural_wilson(const TWFPatternSummary& summary,
                                         const FinalRulesParams& params);
};

} // namespace rza