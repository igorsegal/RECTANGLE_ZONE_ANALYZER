// ============================================================================
// pipe_worker.cpp — Реализация обработки задачи
// ============================================================================

#include "pipeline/pipe_worker.h"
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
#include "backtest/bt_engine.h"
#include "utils/timer.h"
#include <unordered_set>
#include <unordered_map>

namespace rza {

// ============================================================================
// Discovery: Zones + Touches + Statistics + Ranking + WF + TWF + Final Rules
// ============================================================================
bool PipelineWorker::run_discovery(
    const std::string& file_path,
    const SystemParams& params,
    TaskResult& result)
{
    Timer timer;

    // 1. Чтение файла
    auto data = BinReader::read(file_path);
    if (!data.success) {
        result.error_message = "Read failed: " + data.error_message;
        return false;
    }
    result.bars_count = static_cast<int32_t>(data.bar_count);
    result.symbol = data.symbol;
    result.timeframe = data.timeframe_str();
    result.file_path = file_path;

    // 2. ATR
    auto atr = ATRCalculator::calculate(data.bars, params.zone.atr_period);

    // 3. Zones
    auto zones_result = ZoneFinder::find_zones(
        data.bars, atr, params.zone,
        data.period_seconds, data.point, data.digits,
        data.symbol, data.timeframe_str()
    );
    result.zones_found = zones_result.accepted_zones;

    if (zones_result.accepted_zones == 0) {
        result.error_message = "No zones found";
        return false;
    }

    // 4. Touches
    auto touches_result = TouchFinder::find_touches(
        data.bars, atr, zones_result.zones, params.touch,
        data.period_seconds, data.point, data.digits,
        data.symbol, data.timeframe_str()
    );
    result.touches_found = touches_result.total_touches;

    if (touches_result.total_touches == 0) {
        result.error_message = "No touches found";
        return false;
    }

    // 5. Statistics
    auto acc = StatAggregator::aggregate(touches_result.touches,
                                          params.stats.asia_end_hour,
                                          params.stats.london_end_hour,
                                          params.stats.newyork_end_hour);

    // 6. True Walk-Forward
    // ИЗМЕНЕНО: передача параметров в неделях (opt_weeks, oos_weeks, step_weeks)
    auto windows = TWFWindowBuilder::build_windows(
        data.first_time, data.last_time,
        params.validation.opt_weeks,
        params.validation.oos_weeks,
        params.validation.step_weeks
    );

    if (!windows.success || windows.total_windows == 0) {
        result.error_message = "No TWF windows built";
        return false;
    }

    // Собираем уникальные ключи паттернов
    std::unordered_set<uint64_t> all_keys;
    std::unordered_map<uint64_t, PatternKey> key_map;

    for (const auto& pair : acc.all_stats()) {
        uint64_t k = pair.first.to_uint64();
        all_keys.insert(k);
        key_map[k] = pair.first;
    }

    // WalkForwardParams для TWF
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
    result.patterns_evaluated = twf_result.total_patterns_evaluated;

    // 7. Final Rules
    auto validations = FRValidator::validate_all(twf_result, params.final_rules);
    auto accepted_rules = FRDecoder::decode_all(validations, true);

    result.rules_accepted = static_cast<int32_t>(accepted_rules.size());
    result.accepted_rules = accepted_rules;

    if (accepted_rules.empty()) {
        result.error_message = "No rules accepted";
        return false;
    }

    return true;
}

// ============================================================================
// Backtest по принятым правилам
// ============================================================================
bool PipelineWorker::run_backtest(
    const TaskResult& discovery_result,
    const SystemParams& params,
    TaskResult& result)
{
    // Читаем файл заново (для простоты; в оптимизации можно кэшировать)
    auto data = BinReader::read(discovery_result.file_path);
    if (!data.success) {
        result.error_message = "Read failed for backtest: " + data.error_message;
        return false;
    }

    auto atr = ATRCalculator::calculate(data.bars, params.zone.atr_period);

    BacktestInput bt_input;
    bt_input.bars = data.bars;
    bt_input.atr = atr;
    bt_input.rules = discovery_result.accepted_rules;
    bt_input.symbol = data.symbol;
    bt_input.timeframe = data.timeframe_str();
    bt_input.period_seconds = data.period_seconds;
    bt_input.point = data.point;
    bt_input.digits = data.digits;
    bt_input.bt_params = params.backtest;
    bt_input.commission_params.commission_per_lot = params.backtest.commission_per_lot;
    bt_input.commission_params.point = data.point;

    auto bt_result = BacktestEngine::run(bt_input);
    if (!bt_result.success) {
        result.error_message = "Backtest failed: " + bt_result.error_message;
        return false;
    }

    // Заполняем результат
    const auto& combined = bt_result.combined_report;
    result.total_trades = combined.total_trades;
    result.winning_trades = combined.winning_trades;
    result.losing_trades = combined.losing_trades;
    result.net_profit = combined.net_profit;
    result.profit_factor = combined.profit_factor;
    result.winrate_pct = combined.winrate_pct;
    result.max_drawdown_pct = combined.max_drawdown_pct;
    result.quality_score = combined.quality_score;
    result.backtest_report = combined;

    return true;
}

// ============================================================================
// Главная функция обработки задачи
// ============================================================================
TaskResult PipelineWorker::process_task(
    const PipelineTask& task,
    const SystemParams& params)
{
    TaskResult result;
    result.symbol = task.symbol;
    result.timeframe = task.timeframe;
    result.file_path = task.file_path;

    Timer timer;

    // 1. Discovery
    if (!run_discovery(task.file_path, params, result)) {
        result.success = false;
        result.processing_time_ms = timer.elapsed_ms();
        return result;
    }

    // 2. Backtest
    if (!run_backtest(result, params, result)) {
        result.success = false;
        result.processing_time_ms = timer.elapsed_ms();
        return result;
    }

    result.success = true;
    result.processing_time_ms = timer.elapsed_ms();
    return result;
}

} // namespace rza