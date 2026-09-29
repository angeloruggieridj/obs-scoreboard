// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Judgement of one selftest run (spec §10.2). Pure C++: no OBS, no Qt, unit-tested in tests/.

#include <cstdint>
#include <string>
#include <vector>

namespace sbtest {

struct TextPush {
    std::uint64_t ns = 0; // os_gettime_ns() when the Text source received it
    std::string text;
};

struct FrameSample {
    std::uint64_t ns = 0;   // the frame's own timestamp (os_gettime_ns base)
    std::uint64_t hash = 0; // regionHash of the Text source's box in that frame
};

struct VerdictInput {
    std::vector<std::string> expectedTexts; // in order; [0] is on screen when the clock starts
    std::vector<std::uint64_t> expectedNs;  // when each expected text becomes due
    std::vector<TextPush> pushes;           // what the source received, in order
    std::vector<FrameSample> frames;        // in timestamp order
    double fps = 30.0;
    std::uint64_t slackNs = 20'000'000ULL; // the clock driver's tick
};

struct ChangeResult {
    std::string text;
    std::uint64_t expectedNs = 0;
    std::uint64_t pushNs = 0;  // 0: never reached the source
    std::uint64_t shownNs = 0; // 0: no frame showed it
    double latencyMs = 0.0;
    bool ok = false;
};

struct Verdict {
    bool sequenceOk = false;
    bool drawnOk = false;
    bool latencyOk = false;
    double budgetMs = 0.0;
    double maxLatencyMs = 0.0;
    std::vector<ChangeResult> changes;
    std::vector<std::string> problems;
    bool pass() const { return sequenceOk && drawnOk && latencyOk; }
};

Verdict evaluate(const VerdictInput& input);

// FNV-1a 64 over the RGBA bytes of the rectangle (x, y, w, h), clipped to the frame.
std::uint64_t regionHash(const std::uint8_t* rgba, std::uint32_t linesize, std::uint32_t frameWidth,
                         std::uint32_t frameHeight, int x, int y, int w, int h);

} // namespace sbtest
