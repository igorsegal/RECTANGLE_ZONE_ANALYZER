#pragma once
// ============================================================================
// params.h — Параметры системы
// ============================================================================

#include <string>
#include <vector>

namespace rza {

// ============================================================================
// 1. ПАРАМЕТРЫ ПОИСКА ЗОН (Блок 01)
// ============================================================================

struct ZoneSearchParams {
    double min_gap_atr = 0.30;
    double min_gap_points = 20.0;
    double breakout_atr = 0.10;
    double breakout_points = 10.0;
    int    atr_period = 14;
    int    history_bars_to_scan = 0;
};

// ============================================================================
// 2. ПАРАМЕТРЫ КАСАНИЙ (Блок 02)
// ============================================================================

struct TouchSearchParams {
    int    atr_period = 14;
    double reaction_points = 0.0;
    double reaction_atr = 1.0;
    double reaction_zone_height = 1.0;
    double local_breakout_points = 10.0;
    double local_breakout_atr = 0.10;
    int    max_local_bars = 30;
    int    max_structural_bars = 300;
    int    mfe_mae_obs_bars = 30;
};

// ============================================================================
// 3. ПАРАМЕТРЫ СТАТИСТИКИ (Блок 03)
// ============================================================================

struct StatisticsParams {
    int    min_reliable_samples = 30;
    int    asia_end_hour = 7;
    int    london_end_hour = 13;
    int    newyork_end_hour = 21;
};

// ============================================================================
// 4. ПАРАМЕТРЫ ОТБОРА (Блок 04)
// ============================================================================

struct RankingParams {
    int    min_samples = 100;
    int    strong_samples = 300;
    int    min_local_decisive = 80;
    int    min_structural_decisive = 80;
    double min_local_wilson = 25.0;
    double min_structural_wilson = 50.0;
    double min_improvement_pp = 2.0;
    double min_mfe_mae_ratio = 0.0;
    double weight_local_wilson = 0.35;
    double weight_structural_wilson = 0.40;
    double weight_mfe_mae = 0.10;
    double weight_improvement = 0.15;
};

// ============================================================================
// 5. ПАРАМЕТРЫ WALK-FORWARD (Блоки 05-06)
// ============================================================================

struct WalkForwardParams {
    int    number_of_segments = 6;
    int    training_segments = 4;
    int    min_train_samples = 100;
    int    strong_train_samples = 300;
    int    min_train_local_decisive = 80;
    int    min_train_structural_decisive = 80;
    double min_train_local_wilson = 25.0;
    double min_train_structural_wilson = 50.0;
    double min_train_improvement_pp = 2.0;
    int    min_oos_samples = 20;
    int    min_oos_local_decisive = 15;
    int    min_oos_structural_decisive = 15;
    double min_oos_improvement_pp = 0.0;
};

// ============================================================================
// 6. ПАРАМЕТРЫ ФИНАЛЬНЫХ ПРАВИЛ (Блок 07)
// ============================================================================

struct FinalRulesParams {
    std::string required_oos_grade = "CONFIRMED_TWO_FOLDS";
    int    required_passed_folds = 2;
    double required_pass_share_pct = 100.0;
    int    min_rule_oos_samples = 50;
    int    strong_rule_samples = 300;
    int    min_local_decisive = 50;
    double min_local_improvement_pp = 0.0;
    double min_avg_local_improvement_pp = 0.0;
    double min_local_wilson_lower95 = 0.0;
    double min_local_mfe_mae_ratio = 0.0;
    int    min_structural_decisive = 50;
    double min_structural_improvement_pp = 0.0;
    double min_avg_structural_improvement_pp = 0.0;
    double min_structural_wilson_lower95 = 0.0;
};

// ============================================================================
// 7. ENUM ДЛЯ СТРОГОГО ИСПОЛНЕНИЯ
// ============================================================================

enum class ReuseMode : int {
    ONCE = 0,
    LIMITED = 1,
    UNLIMITED = 2
};

enum class ProfitManagement : int {
    STATIC = 0,
    BREAKEVEN = 1,
    TRAILING = 2
};

enum class BreakevenTrigger : int {
    ATR_0_6 = 0,
    HALF_TP = 1
};

// ============================================================================
// 8. ПАРАМЕТРЫ БЭКТЕСТА
// ============================================================================

struct BacktestParams {
    double deposit = 100000.0;
    double risk_percent = 1.0;
    int    max_concurrent_positions = 1;
    double commission_per_lot = 4.0;
    double min_lot = 0.01;
    double lot_step = 0.01;
    double max_lot = 1.0;
    double slippage_points = 0.0;

    // Параметры входа (для обратной совместимости)
    double sl_atr_multiplier = 1.5;
    double tp_atr_multiplier = 3.0;
    int    min_bars_between_entries = 5;
    int    max_bars_per_trade = 30;
    bool   use_only_confirmed_rules = true;

    // Новые параметры строгого исполнения
    ReuseMode        reuse_mode = ReuseMode::ONCE;
    int              reuse_limit = 1;
    ProfitManagement profit_management = ProfitManagement::STATIC;
    BreakevenTrigger breakeven_trigger = BreakevenTrigger::ATR_0_6;
    double           breakeven_atr_threshold = 0.6;
};

// ============================================================================
// 9. ПАРАМЕТРЫ ОПТИМИЗАЦИИ
// ============================================================================

struct OptimizationParams {
    std::vector<double> min_gap_atr_values        = {0.25, 0.35, 0.45};
    std::vector<double> breakout_atr_values       = {0.08, 0.12, 0.16};
    std::vector<double> reaction_atr_values       = {0.8, 1.2, 1.6};
    std::vector<double> local_breakout_atr_values = {0.08, 0.12};
    std::vector<int>    max_local_bars_values     = {25, 35};
    std::vector<int>    max_structural_bars_values= {200, 300};
    std::vector<double> risk_percent_values       = {1.0, 2.0};
    std::vector<int>    max_concurrent_pos_values = {1, 3};

    std::vector<int>    opt_weeks_values          = {2, 3, 4};
    std::vector<int>    oos_weeks_values          = {1};
    std::vector<int>    step_weeks_values         = {1};

    std::vector<int>    reuse_mode_values         = {0, 1, 2};
    std::vector<int>    reuse_limit_values        = {1, 2, 3};
    std::vector<int>    profit_mgmt_values        = {0, 1, 2};
    std::vector<int>    breakeven_trigger_values  = {0, 1};

    int    top_discovery_count = 10;
    double min_profit_factor = 2.0;
};

// ============================================================================
// 10. ПАРАМЕТРЫ ВАЛИДАЦИИ
// ============================================================================

struct ValidationParams {
    int    opt_weeks = 12;
    int    oos_weeks = 4;
    int    step_weeks = 4;
    int    min_validation_windows = 1;
};

// ============================================================================
// 11. ОБЪЕДИНЁННЫЕ ПАРАМЕТРЫ СИСТЕМЫ
// ============================================================================

struct SystemParams {
    ZoneSearchParams    zone;
    TouchSearchParams   touch;
    StatisticsParams    stats;
    RankingParams       ranking;
    WalkForwardParams   walkforward;
    FinalRulesParams    final_rules;
    BacktestParams      backtest;
    OptimizationParams  optimization;
    ValidationParams    validation;

    std::string data_dir    = "D:/AHexaTrader/1DataFiles/raw";
    std::string results_dir = "D:/AHexaTrader/2026.08.18 RECTANGLE_ZONE_ANALYZER/results";

    int  num_threads = 2;
    int  progress_every_bars = 5000;
    bool verbose = false;
};

} // namespace rza