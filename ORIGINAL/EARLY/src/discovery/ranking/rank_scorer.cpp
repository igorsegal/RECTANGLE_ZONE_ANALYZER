// ============================================================================
// rank_scorer.cpp — Реализация скоринга и фильтрации
// ============================================================================

#include "discovery/ranking/rank_scorer.h"
#include "utils/math_utils.h"
#include <sstream>

namespace rza {

// ============================================================================
// Проверка фильтра
// ============================================================================
bool RankScorer::passes_filter(const RankedPattern& pattern,
                                const RankingParams& params,
                                std::string& reason)
{
    std::ostringstream oss;

    // 1. Минимальное количество samples
    if (pattern.stats.samples < params.min_samples) {
        oss << "samples=" << pattern.stats.samples
            << " < min=" << params.min_samples;
        reason = oss.str();
        return false;
    }

    // 2. Минимальное decisive local
    int decisive_local = pattern.stats.decisive_local();
    if (decisive_local < params.min_local_decisive) {
        oss << "decisive_local=" << decisive_local
            << " < min=" << params.min_local_decisive;
        reason = oss.str();
        return false;
    }

    // 3. Минимальное decisive structural
    int decisive_struct = pattern.stats.decisive_struct();
    if (decisive_struct < params.min_structural_decisive) {
        oss << "decisive_struct=" << decisive_struct
            << " < min=" << params.min_structural_decisive;
        reason = oss.str();
        return false;
    }

    // 4. Минимальный Wilson local
    if (pattern.local_wilson < params.min_local_wilson) {
        oss << "local_wilson=" << pattern.local_wilson
            << " < min=" << params.min_local_wilson;
        reason = oss.str();
        return false;
    }

    // 5. Минимальный Wilson structural
    if (pattern.struct_wilson < params.min_structural_wilson) {
        oss << "struct_wilson=" << pattern.struct_wilson
            << " < min=" << params.min_structural_wilson;
        reason = oss.str();
        return false;
    }

    // 6. Минимальный improvement
    double max_improvement = std::max(pattern.local_improvement_pp,
                                      pattern.struct_improvement_pp);
    if (max_improvement < params.min_improvement_pp) {
        oss << "max_improvement=" << max_improvement
            << " < min=" << params.min_improvement_pp;
        reason = oss.str();
        return false;
    }

    // 7. MFE/MAE ratio
    if (pattern.mfe_mae_ratio >= 0.0 &&
        pattern.mfe_mae_ratio < params.min_mfe_mae_ratio) {
        oss << "mfe_mae_ratio=" << pattern.mfe_mae_ratio
            << " < min=" << params.min_mfe_mae_ratio;
        reason = oss.str();
        return false;
    }

    reason = "OK";
    return true;
}

// ============================================================================
// Расчёт финального скоринга
// ============================================================================
void RankScorer::score(RankedPattern& pattern, const RankingParams& params) {
    double score = 0.0;

    // Wilson local (вес 0.35)
    if (pattern.local_wilson >= 0.0) {
        score += params.weight_local_wilson * pattern.local_wilson;
    }

    // Wilson structural (вес 0.40)
    if (pattern.struct_wilson >= 0.0) {
        score += params.weight_structural_wilson * pattern.struct_wilson;
    }

    // MFE/MAE ratio (вес 0.10)
    if (pattern.mfe_mae_ratio >= 0.0) {
        // Нормализуем: ratio > 2 считается отличным
        double normalized = std::min(pattern.mfe_mae_ratio / 2.0, 1.0) * 100.0;
        score += params.weight_mfe_mae * normalized;
    }

    // Improvement (вес 0.15)
    double max_improvement = std::max(pattern.local_improvement_pp,
                                      pattern.struct_improvement_pp);
    if (max_improvement > 0.0) {
        // Нормализуем: improvement > 10 pp считается отличным
        double normalized = std::min(max_improvement / 10.0, 1.0) * 100.0;
        score += params.weight_improvement * normalized;
    }

    pattern.final_score = score;
}

// ============================================================================
// Применение ко всем паттернам
// ============================================================================
void RankScorer::score_and_filter(
    std::vector<RankedPattern>& patterns,
    const RankingParams& params)
{
    for (auto& pattern : patterns) {
        score(pattern, params);
        pattern.passes_filter = passes_filter(pattern, params, pattern.filter_reason);
    }
}

} // namespace rza