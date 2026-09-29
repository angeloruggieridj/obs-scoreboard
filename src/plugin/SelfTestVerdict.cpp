// SPDX-License-Identifier: GPL-2.0-or-later
#include "SelfTestVerdict.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace sbtest {

namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::string describeSequence(const std::vector<std::string>& expected,
                             const std::vector<std::string>& got) {
    const std::size_t n = std::min(expected.size(), got.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (expected[i] != got[i])
            return "text #" + std::to_string(i) + ": expected \"" + expected[i] + "\", got \"" +
                   got[i] + "\"";
    }
    return "expected " + std::to_string(expected.size()) + " texts, the source received " +
           std::to_string(got.size());
}

// First frame at or after `from` (and before `until`) whose hash differs from the last frame
// before `from`.
std::optional<std::uint64_t> firstChangedFrame(const std::vector<FrameSample>& frames,
                                               std::uint64_t from, std::uint64_t until) {
    std::optional<std::uint64_t> baseline;
    for (const FrameSample& f : frames) {
        if (f.ns < from) {
            baseline = f.hash;
            continue;
        }
        if (f.ns >= until) break;
        if (!baseline) return std::nullopt; // nothing to compare with
        if (f.hash != *baseline) return f.ns;
    }
    return std::nullopt;
}

} // namespace

Verdict evaluate(const VerdictInput& in) {
    Verdict v;
    const double intervalMs = 1000.0 / in.fps;
    const auto intervalNs = static_cast<std::uint64_t>(1e9 / in.fps);
    v.budgetMs = intervalMs + static_cast<double>(in.slackNs) / 1e6;

    std::vector<std::string> got;
    got.reserve(in.pushes.size());
    for (const TextPush& p : in.pushes) got.push_back(p.text);
    v.sequenceOk =
        !got.empty() && got == in.expectedTexts && in.expectedTexts.size() == in.expectedNs.size();
    if (!v.sequenceOk) v.problems.push_back("sequence: " + describeSequence(in.expectedTexts, got));

    v.drawnOk = true;
    v.latencyOk = true;
    for (std::size_t i = 1; i < in.expectedTexts.size() && i < in.expectedNs.size(); ++i) {
        ChangeResult c;
        c.text = in.expectedTexts[i];
        c.expectedNs = in.expectedNs[i];
        if (i >= in.pushes.size() || in.pushes[i].text != c.text) {
            v.drawnOk = false;
            v.latencyOk = false;
            v.problems.push_back("\"" + c.text + "\" never reached the source");
            v.changes.push_back(c);
            continue;
        }
        c.pushNs = in.pushes[i].ns;
        const std::uint64_t from = c.pushNs > intervalNs ? c.pushNs - intervalNs : 0;
        const std::uint64_t until = i + 1 < in.pushes.size() && in.pushes[i + 1].ns > intervalNs
                                        ? in.pushes[i + 1].ns - intervalNs
                                        : std::numeric_limits<std::uint64_t>::max();
        const auto shown = firstChangedFrame(in.frames, from, until);
        if (!shown) {
            v.drawnOk = false;
            v.latencyOk = false;
            v.problems.push_back("\"" + c.text + "\" was never drawn");
            v.changes.push_back(c);
            continue;
        }
        c.shownNs = *shown;
        c.latencyMs = (static_cast<double>(c.shownNs) - static_cast<double>(c.expectedNs)) / 1e6;
        c.ok = c.latencyMs >= -intervalMs && c.latencyMs <= v.budgetMs;
        if (!c.ok) {
            v.latencyOk = false;
            v.problems.push_back("\"" + c.text + "\" shown " + std::to_string(c.latencyMs) +
                                 " ms after it was due (budget " + std::to_string(v.budgetMs) +
                                 " ms)");
        }
        v.maxLatencyMs = std::max(v.maxLatencyMs, c.latencyMs);
        v.changes.push_back(c);
    }
    return v;
}

std::uint64_t regionHash(const std::uint8_t* rgba, std::uint32_t linesize, std::uint32_t frameWidth,
                         std::uint32_t frameHeight, int x, int y, int w, int h) {
    const long long x0 = std::max<long long>(0, x);
    const long long y0 = std::max<long long>(0, y);
    const long long x1 = std::min<long long>(frameWidth, static_cast<long long>(x) + w);
    const long long y1 = std::min<long long>(frameHeight, static_cast<long long>(y) + h);
    std::uint64_t hash = kFnvOffset;
    for (long long row = y0; row < y1; ++row) {
        const std::uint8_t* line = rgba + static_cast<std::size_t>(row) * linesize;
        for (long long byte = x0 * 4; byte < x1 * 4; ++byte) {
            hash ^= line[byte];
            hash *= kFnvPrime;
        }
    }
    return hash;
}

} // namespace sbtest
