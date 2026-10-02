// ============================================================================
// pipe_progress.cpp — Реализация отображения прогресса
// ============================================================================

#include "pipeline/pipe_progress.h"
#include <iostream>
#include <iomanip>

namespace rza {

void PipelineProgress::print_progress(
    const std::vector<PipelineTask>& tasks,
    int completed,
    int failed,
    int total)
{
    int percent = (total > 0) ? (completed * 100 / total) : 0;

    std::cout << "\r  Progress: [";
    const int bar_width = 40;
    int pos = (bar_width * completed) / (total > 0 ? total : 1);
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::setw(3) << percent << "%  "
              << "(" << completed << "/" << total
              << " done, " << failed << " failed)";
    std::cout.flush();
}

void PipelineProgress::print_summary(
    const std::vector<PipelineTask>& tasks,
    double total_time_ms)
{
    int completed = 0, failed = 0;
    int total_bars = 0, total_zones = 0, total_touches = 0;
    int total_rules = 0, total_trades = 0;
    double total_profit = 0.0;
    int profitable = 0;

    for (const auto& task : tasks) {
        if (task.status == TaskStatus::COMPLETED) {
            completed++;
            const auto& r = task.result;
            total_bars += r.bars_count;
            total_zones += r.zones_found;
            total_touches += r.touches_found;
            total_rules += r.rules_accepted;
            total_trades += r.total_trades;
            total_profit += r.net_profit;
            if (r.net_profit > 0.0) profitable++;
        } else {
            failed++;
        }
    }

    std::cout << "\n\n  === Pipeline Summary ===\n";
    std::cout << "  Total tasks:         " << tasks.size() << "\n";
    std::cout << "  Completed:           " << completed << "\n";
    std::cout << "  Failed:              " << failed << "\n";
    std::cout << "  Total time:          " << std::fixed << std::setprecision(1)
              << total_time_ms << " ms\n";
    std::cout << "\n  Discovery stats:\n";
    std::cout << "    Total bars:        " << total_bars << "\n";
    std::cout << "    Total zones:       " << total_zones << "\n";
    std::cout << "    Total touches:     " << total_touches << "\n";
    std::cout << "    Total rules:       " << total_rules << "\n";
    std::cout << "\n  Backtest stats:\n";
    std::cout << "    Total trades:      " << total_trades << "\n";
    std::cout << "    Total profit:      $" << std::fixed << std::setprecision(2)
              << total_profit << "\n";
    std::cout << "    Profitable:        " << profitable << " / " << completed << "\n";
}

} // namespace rza