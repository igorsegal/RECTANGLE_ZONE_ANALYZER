#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <chrono>
#include <locale>

#include "pipeline/pipe_orchestrator.h"
#include "core/params.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    try {
        std::locale::global(std::locale(""));
        std::cout.imbue(std::locale());
    } catch (...) {}

    std::cout << "====================================================" << std::endl;
    std::cout << "   RectangleZoneAnalyzer - FAST PROGRESS ENGINE" << std::endl;
    std::cout << "====================================================" << std::endl;

    std::vector<std::string> data_files;
    std::string data_dir = "D:\\AHexaTrader\\1DataFiles\\raw";

    std::cout << "[0/3] Scanning global data repository..." << std::endl;
    if (fs::exists(data_dir) && fs::is_directory(data_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(data_dir)) {
            if (fs::is_regular_file(entry) && entry.path().extension() == ".bin") {
                data_files.push_back(entry.path().string());
            }
        }
    }

    // === ФИЛЬТРАЦИЯ: оставляем каждый 7-й файл (анализируем каждую 7-ю пару) ===
    size_t total_found = data_files.size();
    if (total_found > 0) {
        std::vector<std::string> filtered_files;
        const size_t step = 7;  // шаг выборки изменён с 10 на 7
        for (size_t i = 0; i < total_found; i += step) {
            filtered_files.push_back(data_files[i]);
        }
        data_files = filtered_files;
        std::cout << "[FILTER] Pairs reduced by factor 7: "
                  << total_found << " -> " << data_files.size() << " files." << std::endl;
    }
    // ===========================================================================

    if (data_files.empty()) {
        std::cerr << "[ERROR] Historical binary files (.bin) not found in: " << data_dir << std::endl;
        return 1;
    }

    std::cout << "[OK] Total files queued for fast processing: " << data_files.size() << std::endl;

    try {
        rza::SystemParams params;
        
        std::cout << "[1/3] Generating global tasks for portfolio..." << std::endl;
        std::vector<rza::PipelineTask> tasks = rza::PipelineOrchestrator::create_tasks(data_files);

        std::cout << "[2/3] Executing Walk-Forward Simulation (2 Cores, Fast Native Mode)..." << std::endl;
        std::cout << "      Engine is running at maximum speed. Native progress logs enabled." << std::endl;
        std::cout << "----------------------------------------------------" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();

        // Запуск ОДИН раз на ВСЕ файлы в 2 потока. Никаких циклов и накладных расходов!
        // Встроенный модуль pipe_progress.cpp сам будет аккуратно выводить прогресс в консоль
        rza::PipelineResult result = rza::PipelineOrchestrator::run_all(tasks, params, 2);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        std::cout << "----------------------------------------------------" << std::endl;
        std::cout << "\n[3/3] PORTFOLIO SIMULATION SUMMARY:" << std::endl;
        std::cout << "   --------------------------------------------" << std::endl;
        std::cout << "   Total bars processed  : " << result.total_bars_processed << std::endl;
        std::cout << "   Total zones discovered: " << result.total_zones_found << std::endl;
        std::cout << "   Net portfolio profit  : $" << std::fixed << std::setprecision(2) << result.total_net_profit << std::endl;
        std::cout << "   Successfully optimized: " << result.completed_tasks << " / " << result.total_tasks << std::endl;
        std::cout << "   --------------------------------------------" << std::endl;
        std::cout << " Portfolio calculation finished in " << std::fixed << std::setprecision(2) << elapsed.count() << " sec." << std::endl;
        std::cout << "====================================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "!!! CRITICAL PIPELINE EXCEPTION: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}