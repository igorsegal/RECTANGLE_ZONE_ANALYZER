#pragma once
// ============================================================================
// contracts.h — Журнал контрактов интерфейсов
// ============================================================================
// Одна ответственность: документация всех ключевых интерфейсов и структур,
// которые используются в разных модулях. Это помогает избежать
// рассинхронизации между .h и .cpp файлами.
//
// ВАЖНО: этот файл не содержит кода, только комментарии-контракты.
// Реальные структуры находятся в types.h.
// ============================================================================

namespace rza {

// ============================================================================
// КОНТРАКТ 1: BinFileData
// ============================================================================
// Источник: io/bin_reader.h
// Пользователи: main.cpp, pipeline/task.cpp
//
// Гарантии:
//   - success == true => все поля заполнены корректно
//   - bars.size() == bar_count
//   - bars упорядочены хронологически (от старых к новым)
//   - point > 0 (fallback применён автоматически)
//   - symbol не пустой
//
// Инварианты:
//   - period_seconds ∈ {60, 300, 900, 1800, 3600, 14400, 86400, 604800, 2592000}
//   - digits ∈ [0, 10]
// ============================================================================

// ============================================================================
// КОНТРАКТ 2: ZoneRecord
// ============================================================================
// Источник: discovery/zones/zone_types.h
// Пользователи: discovery/touches, discovery/statistics
//
// Гарантии:
//   - id >= 0
//   - zone_low <= zone_high
//   - confirmation_time > source_time
//   - Если final_status == "BROKEN", то broken_time > confirmation_time
//   - Если final_status == "ACTIVE", то broken_time == 0
//
// Инварианты:
//   - type ∈ {BULL, BEAR}
//   - final_status ∈ {"ACTIVE", "BROKEN", "END_OF_HISTORY", "REJECTED"}
// ============================================================================

// ============================================================================
// КОНТРАКТ 3: TouchRecord
// ============================================================================
// Источник: discovery/touches/touch_types.h
// Пользователи: discovery/statistics, discovery/ranking
//
// Гарантии:
//   - zone_id соответствует существующей зоне
//   - touch_time >= zone.confirmation_time
//   - depth_pct ∈ [0, 100]
//   - Если status == "TOUCH", то entry_direction == "EXPECTED_SIDE"
//   - Если status == "REENTRY", то entry_direction == "WRONG_SIDE"
//
// Инварианты:
//   - local_result ∈ {"REVERSAL_FIRST", "BREAKOUT_FIRST", "TIMEOUT", "END_OF_HISTORY"}
//   - structural_result ∈ {"OPPOSITE_ZONE_FIRST", "SAME_TYPE_ZONE_FIRST",
//                          "TARGETS_BROKEN", "TIMEOUT", "NO_TARGETS", "END_OF_HISTORY"}
// ============================================================================

// ============================================================================
// КОНТРАКТ 4: PatternStats
// ============================================================================
// Источник: discovery/statistics/stat_accumulator.h
// Пользователи: discovery/ranking, discovery/walkforward
//
// Гарантии:
//   - samples >= 0
//   - local_reversal + local_breakout <= samples
//   - struct_opposite + struct_same_type <= samples
//   - count_mfe <= samples
//   - count_mae <= samples
// ============================================================================

// ============================================================================
// КОНТРАКТ 5: BacktestReport
// ============================================================================
// Источник: backtest/bt_metrics.h
// Пользователи: optimizer, reports
//
// Гарантии:
//   - total_trades == winning_trades + losing_trades
//   - gross_profit >= 0
//   - gross_loss <= 0
//   - net_profit == gross_profit + gross_loss
//   - Если total_trades > 0, то winrate_pct ∈ [0, 100]
//   - max_drawdown_money >= 0
//   - profit_factor >= 0 (0 если нет прибыльных сделок)
// ============================================================================

// ============================================================================
// КОНТРАКТ 6: FinalRule
// ============================================================================
// Источник: discovery/final_rules/fr_decoder.h
// Пользователи: backtest/engine
//
// Гарантии:
//   - rule_id не пустой
//   - age_min_minutes >= 0
//   - age_max_minutes == -1 (без ограничения) ИЛИ age_max_minutes > age_min_minutes
//   - depth_min_pct ∈ [0, 100] ИЛИ == -1
//   - depth_max_pct ∈ [0, 100] ИЛИ == -1
//   - direction ∈ {BUY, SELL}
//   - target_mode ∈ {PRIMARY_REACTION_DISTANCE, NEXT_OPPOSITE_ZONE}
// ============================================================================

// ============================================================================
// КОНТРАКТ 7: OptimizationResult
// ============================================================================
// Источник: optimizer/opt_result_comparator.h
// Пользователи: pipeline/orchestrator, reports
//
// Гарантии:
//   - best_report.total_trades >= 0
//   - quality_score > -1e18 (если валидный)
//   - total_combinations_tested >= valid_combinations
//   - optimization_time_sec >= 0
// ============================================================================

} // namespace rza