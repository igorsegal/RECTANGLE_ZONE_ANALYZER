#include "../01/formation_detector.h"
#include "xfbar_reader.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;

namespace {

struct Counts {
    std::uint64_t files_found = 0;
    std::uint64_t files_passed = 0;
    std::uint64_t files_failed = 0;
    std::uint64_t bars = 0;

    std::uint64_t e2_bull = 0;
    std::uint64_t e2_bear = 0;
    std::uint64_t e3_bull = 0;
    std::uint64_t e3_bear = 0;

    std::array<std::uint64_t, 5> progress_bins{};
    double progress_sum = 0.0;
    std::uint64_t progress_count = 0;
};

std::string csv_field(const std::string& s) {
    if (s.find_first_of(";\"\r\n") == std::string::npos) {
        return s;
    }

    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

const char* formation_name(FormationType t) {
    return t == FormationType::ENGULF_2 ? "ENGULF_2" : "ENGULF_3";
}

const char* direction_name(Direction d) {
    return d == Direction::BULLISH ? "BULLISH" : "BEARISH";
}

int progress_bin(double p) {
    if (p < 25.0) return 0;
    if (p < 50.0) return 1;
    if (p < 75.0) return 2;
    if (p < 90.0) return 3;
    return 4;
}

const char* progress_bin_name(int b) {
    static const char* names[5] = {
        "P00_25",
        "P25_50",
        "P50_75",
        "P75_90",
        "P90_100"
    };
    return names[b];
}

std::vector<std::string> find_bin_files(const fs::path& root) {
    std::vector<std::string> files;
    std::error_code ec;

    for (fs::recursive_directory_iterator it(
             root,
             fs::directory_options::skip_permission_denied,
             ec),
         end;
         it != end;
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        if (it->path().extension() == ".bin") {
            files.push_back(it->path().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

void write_event(
    std::ofstream& atlas,
    const XfbarData& data,
    const FormationEvent& e)
{
    const Bar& source = data.bars[e.source_index];
    const Bar& confirm = data.bars[e.confirmation_index];

    atlas
        << csv_field(data.file_path) << ';'
        << csv_field(data.symbol) << ';'
        << timeframe_name(data.period_seconds) << ';'
        << data.period_seconds << ';'
        << formation_name(e.type) << ';'
        << direction_name(e.direction) << ';'
        << e.source_index << ';';

    if (e.has_intermediate) atlas << e.intermediate_index;
    atlas << ';';

    atlas
        << e.confirmation_index << ';'
        << source.time << ';';

    if (e.has_intermediate) {
        atlas << data.bars[e.intermediate_index].time;
    }
    atlas << ';';

    atlas
        << confirm.time << ';'
        << (confirm.time + data.period_seconds) << ';'
        << std::setprecision(17)
        << e.zone_low << ';'
        << e.zone_high << ';'
        << source.open << ';'
        << source.close << ';';

    if (e.has_intermediate) {
        const Bar& mid = data.bars[e.intermediate_index];
        atlas << mid.open << ';' << mid.close;
    }
    atlas << ';';

    atlas
        << confirm.open << ';'
        << confirm.close << ';';

    if (e.has_first_bar_progress) {
        atlas
            << e.first_bar_progress_raw_pct << ';'
            << e.first_bar_progress_pct << ';'
            << progress_bin_name(progress_bin(e.first_bar_progress_pct));
    } else {
        atlas << ";;";
    }

    atlas << '\n';
}

void update_counts(Counts& total, const FormationEvent& e) {
    if (e.type == FormationType::ENGULF_2) {
        if (e.direction == Direction::BULLISH) ++total.e2_bull;
        else ++total.e2_bear;
    } else {
        if (e.direction == Direction::BULLISH) ++total.e3_bull;
        else ++total.e3_bear;

        const int b = progress_bin(e.first_bar_progress_pct);
        ++total.progress_bins[static_cast<std::size_t>(b)];
        total.progress_sum += e.first_bar_progress_pct;
        ++total.progress_count;
    }
}

std::uint64_t total_events(const Counts& c) {
    return c.e2_bull + c.e2_bear + c.e3_bull + c.e3_bear;
}

} // namespace

int main(int argc, char** argv) {
    const fs::path data_root =
        argc >= 2
            ? fs::path(argv[1])
            : fs::path("D:/AHexaTrader/1DataFiles/raw");

    const fs::path out_root =
        argc >= 3
            ? fs::path(argv[2])
            : fs::path("../02out");

    std::error_code ec;

    if (!fs::exists(data_root, ec) || !fs::is_directory(data_root, ec)) {
        std::cerr << "BLOCK02 FAIL - DATA_ROOT_NOT_FOUND: "
                  << data_root.string() << "\n";
        return 2;
    }

    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr << "BLOCK02 FAIL - CANNOT_CREATE_OUTDIR: "
                  << out_root.string() << "\n";
        return 3;
    }

    const fs::path atlas_path = out_root / "02_FORMATION_ATLAS.csv";
    const fs::path file_summary_path = out_root / "02_FILE_SUMMARY.csv";
    const fs::path failures_path = out_root / "02_FAILURES.csv";
    const fs::path summary_path = out_root / "02_SUMMARY.txt";

    std::ofstream atlas(atlas_path, std::ios::binary);
    std::ofstream file_summary(file_summary_path, std::ios::binary);
    std::ofstream failures(failures_path, std::ios::binary);

    if (!atlas || !file_summary || !failures) {
        std::cerr << "BLOCK02 FAIL - CANNOT_OPEN_OUTPUT_FILES\n";
        return 4;
    }

    atlas
        << "File;Symbol;Timeframe;PeriodSeconds;FormationType;Direction;"
        << "SourceIndex;IntermediateIndex;ConfirmationIndex;"
        << "SourceTime;IntermediateTime;ConfirmBarTime;AvailableAt;"
        << "ZoneLow;ZoneHigh;SourceOpen;SourceClose;"
        << "IntermediateOpen;IntermediateClose;ConfirmOpen;ConfirmClose;"
        << "FirstBarProgressRawPct;FirstBarProgressPct;ProgressBucket\n";

    file_summary
        << "File;Symbol;Timeframe;Bars;"
        << "Engulf2Bull;Engulf2Bear;Engulf3Bull;Engulf3Bear;TotalEvents\n";

    failures << "File;Reason\n";

    std::cout << "============================================================\n";
    std::cout << "RZA CANONICAL BLOCK 02 - RECTANGLE FORMATION ATLAS\n";
    std::cout << "DATA=" << data_root.string() << "\n";
    std::cout << "OUT =" << out_root.string() << "\n";
    std::cout << "============================================================\n";

    const auto files = find_bin_files(data_root);

    Counts total;
    total.files_found = files.size();

    if (files.empty()) {
        std::cerr << "BLOCK02 FAIL - NO_BIN_FILES\n";
        return 5;
    }

    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto data = read_xfbar(files[i]);

        if (!data.success) {
            ++total.files_failed;
            failures
                << csv_field(files[i]) << ';'
                << csv_field(data.error) << '\n';

            if ((i + 1) % 25 == 0 || i + 1 == files.size()) {
                std::cout
                    << "[" << (i + 1) << "/" << files.size() << "] "
                    << "events=" << total_events(total)
                    << " failed=" << total.files_failed
                    << "\n";
            }
            continue;
        }

        ++total.files_passed;
        total.bars += data.bars.size();

        Counts one_file;
        const auto events = detect_all(data.bars);

        for (const auto& e : events) {
            write_event(atlas, data, e);
            update_counts(total, e);
            update_counts(one_file, e);
        }

        file_summary
            << csv_field(files[i]) << ';'
            << csv_field(data.symbol) << ';'
            << timeframe_name(data.period_seconds) << ';'
            << data.bars.size() << ';'
            << one_file.e2_bull << ';'
            << one_file.e2_bear << ';'
            << one_file.e3_bull << ';'
            << one_file.e3_bear << ';'
            << total_events(one_file) << '\n';

        if ((i + 1) % 25 == 0 || i + 1 == files.size()) {
            std::cout
                << "[" << (i + 1) << "/" << files.size() << "] "
                << data.symbol << "_" << timeframe_name(data.period_seconds)
                << " events=" << total_events(total)
                << " failed=" << total.files_failed
                << "\n";
        }
    }

    atlas.close();
    file_summary.close();
    failures.close();

    std::ofstream summary(summary_path, std::ios::binary);
    if (!summary) {
        std::cerr << "BLOCK02 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 6;
    }

    const auto events = total_events(total);
    const auto e2 = total.e2_bull + total.e2_bear;
    const auto e3 = total.e3_bull + total.e3_bear;

    summary << "RZA CANONICAL BLOCK 02 - FORMATION ATLAS SUMMARY\n";
    summary << "FILES_FOUND=" << total.files_found << "\n";
    summary << "FILES_PASSED=" << total.files_passed << "\n";
    summary << "FILES_FAILED=" << total.files_failed << "\n";
    summary << "BARS=" << total.bars << "\n";
    summary << "EVENTS_TOTAL=" << events << "\n";
    summary << "ENGULF_2_TOTAL=" << e2 << "\n";
    summary << "ENGULF_2_BULL=" << total.e2_bull << "\n";
    summary << "ENGULF_2_BEAR=" << total.e2_bear << "\n";
    summary << "ENGULF_3_TOTAL=" << e3 << "\n";
    summary << "ENGULF_3_BULL=" << total.e3_bull << "\n";
    summary << "ENGULF_3_BEAR=" << total.e3_bear << "\n";

    if (total.progress_count > 0) {
        summary
            << std::fixed << std::setprecision(6)
            << "ENGULF_3_PROGRESS_MEAN_PCT="
            << (total.progress_sum / total.progress_count)
            << "\n";
    } else {
        summary << "ENGULF_3_PROGRESS_MEAN_PCT=NA\n";
    }

    for (int b = 0; b < 5; ++b) {
        summary
            << "ENGULF_3_" << progress_bin_name(b)
            << "=" << total.progress_bins[static_cast<std::size_t>(b)]
            << "\n";
    }

    summary.close();

    std::cout << "------------------------------------------------------------\n";
    std::cout << "FILES_FOUND=" << total.files_found << "\n";
    std::cout << "FILES_PASSED=" << total.files_passed << "\n";
    std::cout << "FILES_FAILED=" << total.files_failed << "\n";
    std::cout << "BARS=" << total.bars << "\n";
    std::cout << "EVENTS_TOTAL=" << events << "\n";
    std::cout << "ENGULF_2_TOTAL=" << e2 << "\n";
    std::cout << "ENGULF_3_TOTAL=" << e3 << "\n";
    std::cout << "ATLAS=" << atlas_path.string() << "\n";
    std::cout << "SUMMARY=" << summary_path.string() << "\n";

    // Нулевой tolerance: каноническая база должна читаться полностью.
    if (total.files_failed != 0) {
        std::cout << "BLOCK02 FAIL - INVALID_XFBAR_FILES="
                  << total.files_failed << "\n";
        return 7;
    }

    std::cout << "BLOCK02 PASS\n";
    return 0;
}
