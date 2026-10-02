// ============================================================================
// opt_writer.cpp — Реализация записи результатов оптимизации
// ============================================================================

#include "optimizer/opt_writer.h"
#include "io/csv_writer.h"
#include <vector>

namespace rza {

bool OptimizationWriter::write_all_results(
    const std::string& file_path,
    const OptimizationResult& result)
{
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    std::vector<std::string> columns = {
        "ComboID", "MinGapATR", "BreakoutATR", "ReactionATR",
        "LocalBreakoutATR", "MaxLocalBars", "MaxStructuralBars",
        "RiskPercent", "MaxConcurrentPos",
        "QualityScore", "ProfitFactor", "WinratePct",
        "MaxDrawdownPct", "TotalNetProfit", "TotalTrades",
        "ProfitableSymbols", "TotalSymbols", "StabilityScore",
        "ProcessingTimeMs"
    };
    writer.write_header(columns);

    for (const auto& run : result.all_runs) {
        writer.begin_row();
        writer.add_field(run.combo_id);
        writer.add_field(run.params.min_gap_atr, 2);
        writer.add_field(run.params.breakout_atr, 2);
        writer.add_field(run.params.reaction_atr, 2);
        writer.add_field(run.params.local_breakout_atr, 2);
        writer.add_field(run.params.max_local_bars);
        writer.add_field(run.params.max_structural_bars);
        writer.add_field(run.params.risk_percent, 1);
        writer.add_field(run.params.max_concurrent_positions);

        writer.add_field(run.quality.quality_score, 2);
        writer.add_field(run.quality.profit_factor, 2);
        writer.add_field(run.quality.winrate_pct, 1);
        writer.add_field(run.quality.max_drawdown_pct, 2);
        writer.add_field(run.quality.total_net_profit, 2);
        writer.add_field(run.quality.total_trades);
        writer.add_field(run.quality.profitable_symbols);
        writer.add_field(run.quality.total_symbols);
        writer.add_field(run.quality.stability_score, 1);
        writer.add_field(run.processing_time_ms, 1);

        writer.end_row();
    }

    writer.close();
    return true;
}

bool OptimizationWriter::write_top_results(
    const std::string& file_path,
    const OptimizationResult& result)
{
    CsvWriter writer;
    if (!writer.open(file_path)) {
        return false;
    }

    std::vector<std::string> columns = {
        "Rank", "ComboID", "MinGapATR", "BreakoutATR", "ReactionATR",
        "QualityScore", "ProfitFactor", "WinratePct",
        "MaxDrawdownPct", "TotalNetProfit", "TotalTrades",
        "ProfitableSymbols", "StabilityScore"
    };
    writer.write_header(columns);

    int rank = 1;
    for (const auto& run : result.top_results) {
        writer.begin_row();
        writer.add_field(rank++);
        writer.add_field(run.combo_id);
        writer.add_field(run.params.min_gap_atr, 2);
        writer.add_field(run.params.breakout_atr, 2);
        writer.add_field(run.params.reaction_atr, 2);

        writer.add_field(run.quality.quality_score, 2);
        writer.add_field(run.quality.profit_factor, 2);
        writer.add_field(run.quality.winrate_pct, 1);
        writer.add_field(run.quality.max_drawdown_pct, 2);
        writer.add_field(run.quality.total_net_profit, 2);
        writer.add_field(run.quality.total_trades);
        writer.add_field(run.quality.profitable_symbols);
        writer.add_field(run.quality.stability_score, 1);

        writer.end_row();
    }

    writer.close();
    return true;
}

} // namespace rza