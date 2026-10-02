// ============================================================================
// main.cpp — Точка входа RectangleZoneAnalyzer
// ============================================================================
// Волна 13: Strict Execution Logic — демо-режим
// ИСПРАВЛЕНИЕ: перебор файлов для поиска подходящего
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_set>
#include <unordered_map>

#include "core/constants.h"
#include "core/types.h"
#include "core/params.h"
#include "core/buckets.h"
#include "utils/timer.h"
#include "utils/progress_bar.h"
#include "utils/logger.h"
#include "utils/math_utils.h"
#include "io/bin_reader.h"
#include "indicators/atr.h"
#include "discovery/zones/zone_finder.h"
#include "discovery/touches/touch_finder.h"
#include "discovery/statistics/stat_accumulator.h"
#include "discovery/statistics/stat_aggregator.h"
#include "discovery/true_wf/twf_window.h"
#include "discovery/true_wf/twf_censor.h"
#include "discovery/true_wf/twf_selector.h"
#include "discovery/true_wf/twf_summary.h"
#include "discovery/final_rules/fr_validator.h"
#include "discovery/final_rules/fr_decoder.h"
#include "backtest/strict/strict_engine.h"

using namespace rza;

namespace fs = std::filesystem;

void print_banner() {
    std::cout << "============================================================\n";
    std::cout << "  RectangleZoneAnalyzer C++\n";
    std::cout << "  Version: " << PROJECT_VERSION << "\n";
    std::cout << "  Wave 13: Strict Execution Logic (Demo)\n";
    std::cout << "============================================================\n";
}

bool ensure_results_dir(const std::string& results_dir) {
    try {
        if (!fs::exists(results_dir)) {
            fs::create_directories(results_dir);
            std::cout << "  [OK] Created results directory: " << results_dir << "\n";
        }
        return true;
    } catch (const std::exception& e) {
        std::cout << "  [ERROR] Failed to create results directory: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Структура для хранения промежуточных результатов Discovery
// ============================================================================
struct DiscoveryResult {
    bool success = false;
    std::string error_message;
    std::string file_path;
    
    BinFileData data;
    std::vector<double> atr;
    std::vector<ZoneRecord> zones;
    std::vector<FinalRule> accepted_rules;
    
    int zones_found = 0;
    int touches_found = 0;
    int twf_windows = 0;
    int rules_accepted = 0;
};

// ============================================================================
// Запуск Discovery на одном файле
// ============================================================================
DiscoveryResult run_discovery_on_file(
    const std::string& file_path,
    const SystemParams& params)
{
    DiscoveryResult result;
    result.file_path = file_path;
    result.success = false;

    // Чтение файла
    result.data = BinReader::read(file_path);
    if (!result.data.success) {
        result.error_message = "Read failed: " + result.data.error_message;
        return result;
    }

    // ATR
    result.atr = ATRCalculator::calculate(result.data.bars, params.zone.atr_period);

    // Zones
    auto zones_result = ZoneFinder::find_zones(
        result.data.bars, result.atr, params.zone,
        result.data.period_seconds, result.data.point, result.data.digits,
        result.data.symbol, result.data.timeframe_str()
    );
    result.zones = zones_result.zones;
    result.zones_found = zones_result.accepted_zones;

    if (zones_result.accepted_zones == 0) {
        result.error_message = "No zones found";
        return result;
    }

    // Touches
    auto touches_result = TouchFinder::find_touches(
        result.data.bars, result.atr, zones_result.zones, params.touch,
        result.data.period_seconds, result.data.point, result.data.digits,
        result.data.symbol, result.data.timeframe_str()
    );
    result.touches_found = touches_result.total_touches;

    if (touches_result.total_touches == 0) {
        result.error_message = "No touches found";
        return result;
    }

    // Statistics
    auto acc = StatAggregator::aggregate(touches_result.touches,
                                          params.stats.asia_end_hour,
                                          params.stats.london_end_hour,
                                          params.stats.newyork_end_hour);

    // TWF Windows
    auto windows = TWFWindowBuilder::build_windows(
        result.data.first_time, result.data.last_time,
        params.validation.opt_weeks,
        params.validation.oos_weeks,
        params.validation.step_weeks
    );

    if (!windows.success || windows.total_windows == 0) {
        result.error_message = "No TWF windows built";
        return result;
    }
    result.twf_windows = windows.total_windows;

    // Walk-Forward evaluation
    std::unordered_set<uint64_t> all_keys;
    std::unordered_map<uint64_t, PatternKey> key_map;

    for (const auto& pair : acc.all_stats()) {
        uint64_t k = pair.first.to_uint64();
        all_keys.insert(k);
        key_map[k] = pair.first;
    }

    WalkForwardParams wf_params;
    wf_params.min_train_samples = params.walkforward.min_train_samples;
    wf_params.min_train_local_decisive = params.walkforward.min_train_local_decisive;
    wf_params.min_train_structural_decisive = params.walkforward.min_train_structural_decisive;
    wf_params.min_train_local_wilson = params.walkforward.min_train_local_wilson;
    wf_params.min_train_structural_wilson = params.walkforward.min_train_structural_wilson;
    wf_params.min_oos_samples = params.walkforward.min_oos_samples;
    wf_params.min_oos_local_decisive = params.walkforward.min_oos_local_decisive;
    wf_params.min_oos_structural_decisive = params.walkforward.min_oos_structural_decisive;
    wf_params.min_oos_improvement_pp = params.walkforward.min_oos_improvement_pp;

    std::vector<TWFWindowPatternResult> all_window_results;

    for (const auto& window : windows.windows) {
        auto opt_touches = TWFCensor::censor_opt(touches_result.touches, window);
        auto val_touches = TWFCensor::censor_val(touches_result.touches, window);

        for (const auto& key : all_keys) {
            const PatternKey& pk = key_map[key];
            auto wr = TWFSelector::evaluate_pattern_in_window(
                pk, opt_touches, val_touches, window.index, wf_params);
            all_window_results.push_back(wr);
        }
    }

    auto twf_result = TWFSummary::build_summary(all_window_results, params.final_rules);

    // Final Rules
    auto validations = FRValidator::validate_all(twf_result, params.final_rules);
    result.accepted_rules = FRDecoder::decode_all(validations, true);
    result.rules_accepted = static_cast<int>(result.accepted_rules.size());

    if (result.accepted_rules.empty()) {
        result.error_message = "No rules accepted";
        return result;
    }

    result.success = true;
    return result;
}

// ============================================================================
// Демо-режим: строгая логика
// ============================================================================
void run_strict_demo(const std::string& data_dir, const std::string& results_dir) {
    std::cout << "\n[Strict Logic Demo]\n";
    std::cout << "  Data dir:    " << data_dir << "\n";
    std::cout << "  Results dir: " << results_dir << "\n";

    if (!ensure_results_dir(results_dir)) {
        return;
    }

    Timer timer;
    auto files = BinReader::find_bin_files(data_dir);
    std::cout << "  Found " << files.size() << " .bin files\n";

    if (files.empty()) {
        std::cout << "  [ERROR] No .bin files found.\n";
        return;
    }

    // Параметры системы (ослабленные для демо)
    SystemParams params;
    params.walkforward.min_train_samples = 1;
    params.walkforward.min_train_local_decisive = 1;
    params.walkforward.min_train_structural_decisive = 1;
    params.walkforward.min_train_local_wilson = 0.0;
    params.walkforward.min_train_structural_wilson = 0.0;
    params.walkforward.min_oos_samples = 1;
    params.walkforward.min_oos_local_decisive = 1;
    params.walkforward.min_oos_structural_decisive = 1;
    params.walkforward.min_oos_improvement_pp = -100.0;
    params.final_rules.required_oos_grade = "SINGLE_FOLD_PASS";
    params.final_rules.min_rule_oos_samples = 1;
    params.final_rules.min_local_decisive = 1;
    params.final_rules.min_structural_decisive = 1;
    params.final_rules.min_local_improvement_pp = -100.0;
    params.final_rules.min_structural_improvement_pp = -100.0;
    params.final_rules.min_local_wilson_lower95 = 0.0;
    params.final_rules.min_structural_wilson_lower95 = 0.0;
    params.final_rules.min_local_mfe_mae_ratio = 0.0;

    // ========================================================================
    // Шаг 1: Перебор файлов для поиска подходящего
    // ========================================================================
    std::cout << "\n  [Step 1] Searching for suitable file...\n";
    
    const int MAX_FILES_TO_TRY = 50;
    int files_checked = 0;
    DiscoveryResult discovery;

    for (size_t i = 0; i < std::min(files.size(), static_cast<size_t>(MAX_FILES_TO_TRY)); ++i) {
        discovery = run_discovery_on_file(files[i], params);
        files_checked++;

        std::cout << "\r    Checked " << files_checked << " files...";
        std::cout.flush();

        if (discovery.success) {
            std::cout << " [FOUND]\n";
            break;
        }
    }

    std::cout << "\n";

    if (!discovery.success) {
        std::cout << "  [ERROR] No suitable file found after checking " 
                  << files_checked << " files.\n";
        std::cout << "  Last error: " << discovery.error_message << "\n";
        return;
    }

    std::cout << "  Suitable file: " << discovery.file_path << "\n";
    std::cout << "    Bars:        " << discovery.data.bar_count << "\n";
    std::cout << "    Zones:       " << discovery.zones_found << "\n";
    std::cout << "    Touches:     " << discovery.touches_found << "\n";
    std::cout << "    TWF windows: " << discovery.twf_windows << "\n";
    std::cout << "    Rules:       " << discovery.rules_accepted << "\n";

    // ========================================================================
    // Шаг 2: StrictEngine (строгая логика)
    // ========================================================================
    std::cout << "\n  [Step 2] Running StrictEngine...\n";
    timer.reset();

    StrictBacktestInput strict_input;
    strict_input.bars = discovery.data.bars;
    strict_input.atr = discovery.atr;
    strict_input.zones = discovery.zones;
    strict_input.rules = discovery.accepted_rules;
    strict_input.symbol = discovery.data.symbol;
    strict_input.timeframe = discovery.data.timeframe_str();
    strict_input.period_seconds = discovery.data.period_seconds;
    strict_input.point = discovery.data.point;
    strict_input.digits = discovery.data.digits;
    strict_input.bt_params = params.backtest;

    // Настраиваем параметры строгой логики
    strict_input.bt_params.reuse_mode = ReuseMode::ONCE;
    strict_input.bt_params.profit_management = ProfitManagement::BREAKEVEN;
    strict_input.bt_params.breakeven_trigger = BreakevenTrigger::ATR_0_6;

    auto strict_result = StrictEngine::run(strict_input);

    double strict_time = timer.elapsed_ms();

    if (!strict_result.success) {
        std::cout << "  [ERROR] StrictEngine failed: " << strict_result.error_message << "\n";
        return;
    }

    // ========================================================================
    // Шаг 3: Вывод результатов
    // ========================================================================
    std::cout << "\n  === Strict Logic Results ===\n";
    std::cout << "  Symbol:              " << strict_input.symbol << "\n";
    std::cout << "  Timeframe:           " << strict_input.timeframe << "\n";
    std::cout << "  Bars:                " << discovery.data.bar_count << "\n";
    std::cout << "  Zones:               " << discovery.zones_found << "\n";
    std::cout << "  Rules:               " << discovery.rules_accepted << "\n";
    std::cout << "  Processing time:     " << std::fixed << std::setprecision(1) 
              << strict_time << " ms\n";

    std::cout << "\n  Strict Logic Statistics:\n";
    std::cout << "    Signals generated: " << strict_result.total_signals_generated << "\n";
    std::cout << "    Signals passed:    " << strict_result.signals_passed_filter << "\n";
    std::cout << "    Positions opened:  " << strict_result.positions_opened << "\n";
    std::cout << "    Rejected (limit):  " << strict_result.positions_rejected_by_limit << "\n";
    std::cout << "    Rejected (reuse):  " << strict_result.positions_rejected_by_reuse << "\n";

    const auto& report = strict_result.report;
    std::cout << "\n  Backtest Results:\n";
    std::cout << "    Total trades:      " << report.total_trades << "\n";
    std::cout << "    Winning:           " << report.winning_trades << "\n";
    std::cout << "    Losing:            " << report.losing_trades << "\n";
    std::cout << "    Winrate:           " << std::fixed << std::setprecision(1) 
              << report.winrate_pct << "%\n";
    std::cout << "    Profit factor:     " << std::setprecision(2) << report.profit_factor << "\n";
    std::cout << "    Net profit:        $" << std::setprecision(2) << report.net_profit << "\n";
    std::cout << "    Max drawdown:      " << std::setprecision(2) << report.max_drawdown_pct << "%\n";
    std::cout << "    Quality score:     " << std::setprecision(2) << report.quality_score << "\n";

    std::cout << "\n  Exit reasons:\n";
    std::cout << "    TP_REACTION:       " << report.exit_tp_reaction << "\n";
    std::cout << "    TP_STRUCTURAL:     " << report.exit_tp_structural << "\n";
    std::cout << "    SL_BREAKOUT:       " << report.exit_sl_breakout << "\n";
    std::cout << "    TIMEOUT:           " << report.exit_timeout << "\n";
    std::cout << "    END_OF_HISTORY:    " << report.exit_end_of_history << "\n";
}

int main() {
    print_banner();

    SystemParams p;
    std::cout << "\n[Configuration]\n";
    std::cout << "  Data dir:    " << p.data_dir << "\n";
    std::cout << "  Results dir: " << p.results_dir << "\n";

    run_strict_demo(p.data_dir, p.results_dir);

    std::cout << "\n============================================================\n";
    std::cout << "  Wave 13 complete. Strict Execution Logic ready.\n";
    std::cout << "  Next: Integrate StrictEngine into Optimizer\n";
    std::cout << "============================================================\n";
    return 0;
}