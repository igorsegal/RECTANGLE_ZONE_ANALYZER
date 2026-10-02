#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Stats {
    std::uint64_t rows = 0;
    std::uint64_t reaction = 0;
    std::uint64_t breakout = 0;
    std::uint64_t other = 0;

    std::uint64_t resolved() const {
        return reaction + breakout;
    }
};

std::vector<std::string> split_semicolon_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    bool quoted = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field.push_back(c);
            }
        } else {
            if (c == '"') {
                quoted = true;
            } else if (c == ';') {
                out.push_back(field);
                field.clear();
            } else {
                field.push_back(c);
            }
        }
    }

    out.push_back(field);
    return out;
}

double reaction_rate_pct(const Stats& s) {
    const auto n = s.resolved();
    if (n == 0) return 0.0;
    return 100.0 * static_cast<double>(s.reaction) / static_cast<double>(n);
}

std::pair<double,double> wilson95_pct(const Stats& s) {
    const double n = static_cast<double>(s.resolved());
    if (n <= 0.0) return {0.0, 0.0};

    const double p = static_cast<double>(s.reaction) / n;
    const double z = 1.959963984540054;
    const double z2 = z * z;

    const double denom = 1.0 + z2 / n;
    const double center = (p + z2 / (2.0 * n)) / denom;
    const double half =
        z * std::sqrt((p * (1.0 - p) / n) + (z2 / (4.0 * n * n))) / denom;

    return {
        100.0 * std::max(0.0, center - half),
        100.0 * std::min(1.0, center + half)
    };
}

void update(Stats& s, const std::string& outcome) {
    ++s.rows;
    if (outcome == "REACTION_FIRST") {
        ++s.reaction;
    } else if (outcome == "BREAKOUT_FIRST") {
        ++s.breakout;
    } else {
        ++s.other;
    }
}

void write_stats_line(
    std::ostream& os,
    const std::string& label,
    const Stats& s)
{
    const auto ci = wilson95_pct(s);

    os
        << label
        << " | rows=" << s.rows
        << " | resolved=" << s.resolved()
        << " | reaction=" << s.reaction
        << " | breakout=" << s.breakout
        << " | other=" << s.other;

    if (s.resolved() > 0) {
        os
            << std::fixed << std::setprecision(6)
            << " | reaction_pct=" << reaction_rate_pct(s)
            << " | CI95=[" << ci.first << ',' << ci.second << ']';
    } else {
        os << " | reaction_pct=NA | CI95=[NA,NA]";
    }

    os << '\n';
}

void write_group_csv(
    std::ofstream& out,
    const std::string& family,
    const std::string& key,
    const Stats& s)
{
    const auto ci = wilson95_pct(s);

    out
        << family << ';'
        << key << ';'
        << s.rows << ';'
        << s.resolved() << ';'
        << s.reaction << ';'
        << s.breakout << ';'
        << s.other << ';';

    if (s.resolved() > 0) {
        out
            << std::fixed << std::setprecision(9)
            << reaction_rate_pct(s) << ';'
            << ci.first << ';'
            << ci.second;
    } else {
        out << ";;";
    }

    out << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const fs::path input =
        argc >= 2
            ? fs::path(argv[1])
            : fs::path("../03out/03_REACTION_SCREEN.csv");

    const fs::path out_root =
        argc >= 3
            ? fs::path(argv[2])
            : fs::path("../04out");

    std::error_code ec;
    if (!fs::exists(input, ec) || !fs::is_regular_file(input, ec)) {
        std::cerr << "BLOCK04 FAIL - INPUT_NOT_FOUND: "
                  << input.string() << "\n";
        return 2;
    }

    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr << "BLOCK04 FAIL - CANNOT_CREATE_OUTDIR\n";
        return 3;
    }

    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::cerr << "BLOCK04 FAIL - CANNOT_OPEN_INPUT\n";
        return 4;
    }

    const fs::path summary_path = out_root / "04_SUMMARY.txt";
    const fs::path groups_path = out_root / "04_GROUP_STATS.csv";

    std::ofstream groups(groups_path, std::ios::binary);
    if (!groups) {
        std::cerr << "BLOCK04 FAIL - CANNOT_OPEN_GROUP_OUTPUT\n";
        return 5;
    }

    std::string header_line;
    if (!std::getline(in, header_line)) {
        std::cerr << "BLOCK04 FAIL - EMPTY_INPUT\n";
        return 6;
    }
    if (!header_line.empty() && header_line.back() == '\r') {
        header_line.pop_back();
    }

    const auto header = split_semicolon_csv(header_line);
    std::map<std::string, std::size_t> col;
    for (std::size_t i = 0; i < header.size(); ++i) {
        col[header[i]] = i;
    }

    const std::vector<std::string> required = {
        "Direction",
        "Timeframe",
        "ProgressBucket",
        "LifecycleResult"
    };

    for (const auto& name : required) {
        if (col.find(name) == col.end()) {
            std::cerr << "BLOCK04 FAIL - MISSING_COLUMN: "
                      << name << "\n";
            return 7;
        }
    }

    Stats overall;
    std::map<std::string, Stats> by_progress;
    std::map<std::string, Stats> by_direction;
    std::map<std::string, Stats> by_timeframe;
    std::map<std::string, Stats> by_progress_direction;
    std::map<std::string, Stats> by_progress_timeframe;

    std::uint64_t rows_read = 0;
    std::uint64_t malformed = 0;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        const auto f = split_semicolon_csv(line);
        if (f.size() != header.size()) {
            ++malformed;
            continue;
        }

        const std::string& direction = f[col["Direction"]];
        const std::string& timeframe = f[col["Timeframe"]];
        const std::string& progress = f[col["ProgressBucket"]];
        const std::string& outcome = f[col["LifecycleResult"]];

        ++rows_read;

        update(overall, outcome);
        update(by_direction[direction], outcome);
        update(by_timeframe[timeframe], outcome);

        // All confirmed engulf rectangles are one continuous research stream.
        // Progress is optional metadata and is grouped only when it is present.
        if (!progress.empty()) {
            update(by_progress[progress], outcome);
            update(by_progress_direction[progress + "|" + direction], outcome);
            update(by_progress_timeframe[progress + "|" + timeframe], outcome);
        }
    }

    groups
        << "GroupFamily;GroupKey;Rows;Resolved;Reaction;Breakout;Other;"
        << "ReactionPct;CI95LowPct;CI95HighPct\n";

    write_group_csv(groups, "OVERALL", "ALL", overall);

    for (const auto& kv : by_progress)
        write_group_csv(groups, "PROGRESS_AVAILABLE", kv.first, kv.second);
    for (const auto& kv : by_direction)
        write_group_csv(groups, "DIRECTION", kv.first, kv.second);
    for (const auto& kv : by_timeframe)
        write_group_csv(groups, "TIMEFRAME", kv.first, kv.second);
    for (const auto& kv : by_progress_direction)
        write_group_csv(groups, "PROGRESS_DIRECTION", kv.first, kv.second);
    for (const auto& kv : by_progress_timeframe)
        write_group_csv(groups, "PROGRESS_TIMEFRAME", kv.first, kv.second);

    groups.close();

    std::ofstream summary(summary_path, std::ios::binary);
    if (!summary) {
        std::cerr << "BLOCK04 FAIL - CANNOT_WRITE_SUMMARY\n";
        return 8;
    }

    summary << "RZA CANONICAL BLOCK 04 - DEVELOPMENT STATISTICS\n";
    summary << "SOURCE=03_REACTION_SCREEN.csv\n";
    summary << "NO_RAW_RESCAN=1\n";
    summary << "ROWS_READ=" << rows_read << "\n";
    summary << "MALFORMED_ROWS=" << malformed << "\n";
    summary << "STREAM=ALL_CONFIRMED_ENGULF_RECTANGLES\n";
    summary << "FORMATION_TYPE_USED_FOR_SPLIT=0\n";
    summary << "RATE_DENOMINATOR=REACTION_FIRST+BREAKOUT_FIRST\n";
    summary << "OTHER_LIFECYCLE_EXCLUDED_FROM_RATE=1\n";
    summary << "\n[OVERALL]\n";
    write_stats_line(summary, "ALL", overall);

    summary << "\n[PROGRESS_WHERE_AVAILABLE]\n";
    static const std::vector<std::string> progress_order = {
        "P00_25", "P25_50", "P50_75", "P75_90", "P90_100"
    };
    for (const auto& p : progress_order) {
        const auto it = by_progress.find(p);
        if (it != by_progress.end()) {
            write_stats_line(summary, p, it->second);
        }
    }

    summary << "\n[DIRECTION]\n";
    for (const auto& kv : by_direction)
        write_stats_line(summary, kv.first, kv.second);

    summary << "\n[TIMEFRAME]\n";
    for (const auto& kv : by_timeframe)
        write_stats_line(summary, kv.first, kv.second);

    summary << "\nCONTRACT:\n";
    summary << "- This block reads only the frozen Block03 output.\n";
    summary << "- No market BIN file is reopened.\n";
    summary << "- No threshold, filter, TP, SL or parameter is optimized.\n";
    summary << "- ENGULF_2 and ENGULF_3 are not separate research branches.\n";
    summary << "- All confirmed engulf rectangles form one continuous stream.\n";
    summary << "- ReactionPct is descriptive development evidence, not trading expectancy.\n";
    summary << "- 2024+ remains reserved for the final independent OOS check.\n";
    summary.close();

    std::cout << "============================================================\n";
    std::cout << "RZA CANONICAL BLOCK 04 - DEVELOPMENT STATISTICS\n";
    std::cout << "============================================================\n";
    std::cout << "ROWS_READ=" << rows_read << "\n";
    std::cout << "MALFORMED_ROWS=" << malformed << "\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "OVERALL_REACTION_PCT=" << reaction_rate_pct(overall) << "\n";

    std::cout << "SUMMARY=" << summary_path.string() << "\n";
    std::cout << "GROUPS=" << groups_path.string() << "\n";

    if (malformed != 0) {
        std::cout << "BLOCK04 FAIL - MALFORMED_ROWS="
                  << malformed << "\n";
        return 9;
    }

    if (overall.resolved() == 0) {
        std::cout << "BLOCK04 FAIL - ZERO_RESOLVED_TOUCHES\n";
        return 10;
    }

    std::cout << "BLOCK04 PASS\n";
    return 0;
}
