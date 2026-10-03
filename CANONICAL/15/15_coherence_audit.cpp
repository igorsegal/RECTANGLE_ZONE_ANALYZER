#include "../02/xfbar_reader.h"
#include "../abs_track_policy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rza::canonical;
namespace ap = rza::canonical::abs_track;

namespace {

constexpr std::int32_t H1_SECONDS = 3600;
constexpr std::int32_t M5_SECONDS = 300;

std::size_t lower_bound_time(const std::vector<Bar>& bars, std::int64_t t) {
    const auto it = std::lower_bound(
        bars.begin(), bars.end(), t,
        [](const Bar& b, std::int64_t v) { return b.time < v; });
    return static_cast<std::size_t>(std::distance(bars.begin(), it));
}

bool quote_close(double a, double b, double point) {
    return std::isfinite(a) && std::isfinite(b) &&
           std::isfinite(point) && point > 0.0 &&
           std::abs(a - b) <= point * (1.0 + 1e-9);
}

struct PairAudit {
    std::uint64_t h1_bars = 0;
    std::uint64_t no_m5_inside = 0;
    std::uint64_t exact_anchor = 0;
    std::uint64_t partial_anchor = 0;
    std::uint64_t ohlc_match = 0;
    std::uint64_t ohlc_mismatch = 0;
    std::uint64_t partial_ohlc_match = 0;
    std::uint64_t partial_ohlc_mismatch = 0;
    std::uint64_t exact_ohlc_match = 0;
    std::uint64_t exact_ohlc_mismatch = 0;
    std::int64_t first_problem_h1 = 0;
    std::int64_t first_problem_m5 = 0;
    std::string first_problem_reason;
    double first_problem_diff_points = 0.0;
};

void set_first_problem(
    PairAudit& a,
    std::int64_t h1_time,
    std::int64_t m5_time,
    const std::string& reason,
    double diff_points = 0.0)
{
    if (!a.first_problem_reason.empty()) return;
    a.first_problem_h1 = h1_time;
    a.first_problem_m5 = m5_time;
    a.first_problem_reason = reason;
    a.first_problem_diff_points = diff_points;
}

PairAudit audit_pair(const XfbarData& h1, const XfbarData& m5) {
    PairAudit a;
    const double point = h1.point;

    for (const auto& hb : h1.bars) {
        ++a.h1_bars;

        const std::size_t begin = lower_bound_time(m5.bars, hb.time);
        const std::size_t end =
            lower_bound_time(m5.bars, hb.time + H1_SECONDS);

        if (begin >= m5.bars.size() || end <= begin ||
            m5.bars[begin].time >= hb.time + H1_SECONDS)
        {
            ++a.no_m5_inside;
            const std::int64_t mt =
                begin < m5.bars.size() ? m5.bars[begin].time : 0;
            set_first_problem(
                a, hb.time, mt, "NO_M5_INSIDE_H1_INTERVAL");
            continue;
        }

        const bool exact = m5.bars[begin].time == hb.time;
        if (exact) ++a.exact_anchor;
        else ++a.partial_anchor;

        double hi = -std::numeric_limits<double>::infinity();
        double lo =  std::numeric_limits<double>::infinity();

        for (std::size_t i = begin; i < end; ++i) {
            hi = std::max(hi, m5.bars[i].high);
            lo = std::min(lo, m5.bars[i].low);
        }

        const double mo = m5.bars[begin].open;
        const double mc = m5.bars[end - 1].close;

        const bool open_ok = quote_close(hb.open, mo, point);
        const bool high_ok = quote_close(hb.high, hi, point);
        const bool low_ok = quote_close(hb.low, lo, point);
        const bool close_ok = quote_close(hb.close, mc, point);
        const bool match = open_ok && high_ok && low_ok && close_ok;

        if (match) {
            ++a.ohlc_match;
            if (exact) ++a.exact_ohlc_match;
            else ++a.partial_ohlc_match;
        } else {
            ++a.ohlc_mismatch;
            if (exact) ++a.exact_ohlc_mismatch;
            else ++a.partial_ohlc_mismatch;

            double max_diff = 0.0;
            if (point > 0.0) {
                max_diff = std::max({
                    std::abs(hb.open - mo) / point,
                    std::abs(hb.high - hi) / point,
                    std::abs(hb.low - lo) / point,
                    std::abs(hb.close - mc) / point
                });
            }
            set_first_problem(
                a,
                hb.time,
                m5.bars[begin].time,
                exact ? "EXACT_ANCHOR_OHLC_MISMATCH"
                      : "PARTIAL_ANCHOR_OHLC_MISMATCH",
                max_diff);
        }
    }

    return a;
}

std::vector<fs::path> find_h1_files(const fs::path& root) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(
             root,
             fs::directory_options::skip_permission_denied,
             ec), end;
         it != end; it.increment(ec))
    {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) { ec.clear(); continue; }
        if (it->path().extension() != ".bin") continue;

        std::string symbol;
        std::int32_t tf = 0;
        if (!ap::inspect_xfbar(it->path(), symbol, tf)) continue;
        if (tf == H1_SECONDS) out.push_back(it->path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

double pct(std::uint64_t n, std::uint64_t d) {
    return d == 0 ? std::numeric_limits<double>::quiet_NaN()
                  : 100.0 * static_cast<double>(n) /
                    static_cast<double>(d);
}

int selftest() {
    int tests = 0;
    int failed = 0;
    auto check = [&](bool ok, const char* name) {
        ++tests;
        if (ok) std::cout << "[OK]   " << name << "\n";
        else { ++failed; std::cout << "[FAIL] " << name << "\n"; }
    };

    XfbarData m5;
    m5.success = true;
    m5.symbol = "TEST";
    m5.period_seconds = M5_SECONDS;
    m5.digits = 2;
    m5.point = 0.01;

    for (int i = 0; i < 12; ++i) {
        Bar b;
        b.time = static_cast<std::int64_t>(i) * M5_SECONDS;
        b.open = 100.0;
        b.high = 101.0;
        b.low = 99.0;
        b.close = 100.5;
        m5.bars.push_back(b);
    }

    XfbarData h1;
    h1.success = true;
    h1.symbol = "TEST";
    h1.period_seconds = H1_SECONDS;
    h1.digits = 2;
    h1.point = 0.01;
    h1.bars.push_back(Bar{0,100.0,101.0,99.0,100.5,0});

    auto a1 = audit_pair(h1, m5);
    check(a1.exact_anchor == 1 && a1.ohlc_match == 1,
          "exact anchor + matching aggregate");

    XfbarData partial = m5;
    partial.bars.erase(partial.bars.begin(), partial.bars.begin() + 6);
    auto a2 = audit_pair(h1, partial);
    check(a2.partial_anchor == 1 && a2.partial_ohlc_match == 1,
          "partial-session hour can still reproduce H1 OHLC");

    XfbarData late = m5;
    for (auto& b : late.bars) b.time += H1_SECONDS;
    auto a3 = audit_pair(h1, late);
    check(a3.no_m5_inside == 1,
          "late M5 history is not mapped into an earlier H1 hour");

    XfbarData bad = m5;
    bad.bars[3].high = 105.0;
    auto a4 = audit_pair(h1, bad);
    check(a4.ohlc_mismatch == 1,
          "OHLC mismatch is detected");

    std::cout << "SELFTESTS=" << tests
              << " FAILED=" << failed << "\n";
    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--selftest") {
        return selftest();
    }

    const fs::path data_root =
        argc >= 2 ? fs::path(argv[1])
                  : fs::path("D:/AHexaTrader/1DataFiles/raw");
    const fs::path out_root =
        argc >= 3 ? fs::path(argv[2])
                  : fs::path("../15out");

    std::error_code ec;
    fs::create_directories(out_root, ec);
    if (ec) {
        std::cerr << "COHERENCE AUDIT FAIL - CANNOT_CREATE_OUTDIR\n";
        return 2;
    }

    std::ofstream csv(out_root / "15_COHERENCE_AUDIT.csv",
                      std::ios::binary);
    std::ofstream summary(out_root / "15_COHERENCE_AUDIT_SUMMARY.txt",
                          std::ios::binary);
    if (!csv || !summary) {
        std::cerr << "COHERENCE AUDIT FAIL - CANNOT_OPEN_OUTPUTS\n";
        return 3;
    }

    csv << "Symbol;H1File;M5File;H1Bars;NoM5Inside;"
        << "ExactAnchor;PartialAnchor;OHLCMatch;OHLCMismatch;"
        << "ExactOHLCMatch;ExactOHLCMismatch;"
        << "PartialOHLCMatch;PartialOHLCMismatch;"
        << "CoveragePct;OHLCMatchPctOfCovered;"
        << "FirstProblemH1Time;FirstProblemM5Time;"
        << "FirstProblemReason;FirstProblemMaxDiffPoints\n";

    std::uint64_t files = 0;
    std::uint64_t audited = 0;
    std::uint64_t missing_m5 = 0;
    std::uint64_t invalid_h1 = 0;
    std::uint64_t invalid_m5 = 0;

    std::uint64_t total_h1 = 0;
    std::uint64_t total_no_m5 = 0;
    std::uint64_t total_exact = 0;
    std::uint64_t total_partial = 0;
    std::uint64_t total_match = 0;
    std::uint64_t total_mismatch = 0;
    std::uint64_t total_partial_match = 0;

    const auto h1_files = find_h1_files(data_root);

    for (const auto& h1_path : h1_files) {
        ++files;
        const XfbarData h1 = read_xfbar(h1_path.string());
        if (!h1.success) { ++invalid_h1; continue; }

        const fs::path m5_path =
            ap::find_m5_sibling(h1_path, h1.symbol);
        if (m5_path.empty()) { ++missing_m5; continue; }

        const XfbarData m5 = read_xfbar(m5_path.string());
        if (!m5.success) { ++invalid_m5; continue; }

        if (h1.symbol != m5.symbol ||
            h1.digits != m5.digits ||
            !std::isfinite(h1.point) ||
            !std::isfinite(m5.point) ||
            !(h1.point > 0.0) ||
            !(m5.point > 0.0))
        {
            ++invalid_m5;
            continue;
        }

        const double scale =
            std::max(std::abs(h1.point), std::abs(m5.point));
        if (std::abs(h1.point - m5.point) > scale * 1e-12) {
            ++invalid_m5;
            continue;
        }

        const PairAudit a = audit_pair(h1, m5);
        ++audited;

        total_h1 += a.h1_bars;
        total_no_m5 += a.no_m5_inside;
        total_exact += a.exact_anchor;
        total_partial += a.partial_anchor;
        total_match += a.ohlc_match;
        total_mismatch += a.ohlc_mismatch;
        total_partial_match += a.partial_ohlc_match;

        const std::uint64_t covered =
            a.exact_anchor + a.partial_anchor;

        csv << h1.symbol << ';'
            << h1_path.string() << ';'
            << m5_path.string() << ';'
            << a.h1_bars << ';'
            << a.no_m5_inside << ';'
            << a.exact_anchor << ';'
            << a.partial_anchor << ';'
            << a.ohlc_match << ';'
            << a.ohlc_mismatch << ';'
            << a.exact_ohlc_match << ';'
            << a.exact_ohlc_mismatch << ';'
            << a.partial_ohlc_match << ';'
            << a.partial_ohlc_mismatch << ';'
            << std::fixed << std::setprecision(9)
            << pct(covered, a.h1_bars) << ';'
            << pct(a.ohlc_match, covered) << ';'
            << a.first_problem_h1 << ';'
            << a.first_problem_m5 << ';'
            << a.first_problem_reason << ';'
            << a.first_problem_diff_points << '\n';
    }

    const std::uint64_t total_covered =
        total_exact + total_partial;

    summary
        << "RZA BLOCK15 CROSS-TF COHERENCE AUDIT\n"
        << "PURPOSE=DIAGNOSE_STRICT_ANCHOR_FAILURES_WITHOUT_CHANGING_RESEARCH_POLICY\n"
        << "H1_FILES_FOUND=" << files << "\n"
        << "PAIRS_AUDITED=" << audited << "\n"
        << "MISSING_M5=" << missing_m5 << "\n"
        << "INVALID_H1=" << invalid_h1 << "\n"
        << "INVALID_M5_OR_METADATA=" << invalid_m5 << "\n"
        << "H1_BARS_TOTAL=" << total_h1 << "\n"
        << "H1_NO_M5_INSIDE=" << total_no_m5 << "\n"
        << "H1_EXACT_ANCHOR=" << total_exact << "\n"
        << "H1_PARTIAL_ANCHOR=" << total_partial << "\n"
        << "H1_OHLC_MATCH=" << total_match << "\n"
        << "H1_OHLC_MISMATCH=" << total_mismatch << "\n"
        << "H1_PARTIAL_ANCHOR_OHLC_MATCH=" << total_partial_match << "\n"
        << std::fixed << std::setprecision(9)
        << "COVERAGE_PCT=" << pct(total_covered, total_h1) << "\n"
        << "OHLC_MATCH_PCT_OF_COVERED="
        << pct(total_match, total_covered) << "\n"
        << "PARTIAL_ANCHOR_MATCH_PCT_OF_PARTIAL="
        << pct(total_partial_match, total_partial) << "\n\n"
        << "INTERPRETATION:\n"
        << "- Exact H1 timestamp == first M5 timestamp is NOT assumed to be required here.\n"
        << "- A partial-anchor hour is diagnostic only: first available M5 lies later inside the H1 hour.\n"
        << "- OHLC_MATCH means available M5 bars inside that H1 interval reproduce H1 OHLC within one quote point.\n"
        << "- This tool does not change Block15 acceptance or produce trading statistics.\n";

    std::cout
        << "COHERENCE_AUDIT_PASS pairs=" << audited
        << " h1_bars=" << total_h1
        << " no_m5=" << total_no_m5
        << " partial=" << total_partial
        << " match=" << total_match
        << " mismatch=" << total_mismatch
        << "\n";

    return 0;
}
