// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "SelfTestVerdict.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace sbtest;

namespace {

constexpr std::uint64_t kSecond = 1'000'000'000ULL;
constexpr std::uint64_t kStart = 100 * kSecond;

std::string text(int secondsLeft) {
    return "00:" + std::string(secondsLeft < 10 ? "0" : "") + std::to_string(secondsLeft);
}

// A clean countdown from 25 s over `changes` seconds at `fps`: each text is pushed 5 ms after
// it is due and every frame from the push on shows it (hash = index of the text on screen).
VerdictInput cleanRun(double fps, int changes, std::uint64_t pushDelayNs = 5'000'000ULL) {
    VerdictInput in;
    in.fps = fps;
    in.slackNs = 20'000'000ULL;
    for (int k = 0; k <= changes; ++k) {
        in.expectedTexts.push_back(text(25 - k));
        in.expectedNs.push_back(kStart + static_cast<std::uint64_t>(k) * kSecond);
        in.pushes.push_back(
            {kStart + static_cast<std::uint64_t>(k) * kSecond + pushDelayNs, text(25 - k)});
    }
    const auto interval = static_cast<std::uint64_t>(1e9 / fps);
    const std::uint64_t end = kStart + static_cast<std::uint64_t>(changes + 1) * kSecond;
    for (std::uint64_t t = kStart - kSecond; t < end; t += interval) {
        std::uint64_t shown = 0;
        for (std::size_t i = 0; i < in.pushes.size(); ++i)
            if (in.pushes[i].ns <= t) shown = i;
        in.frames.push_back({t, 1000 + shown});
    }
    return in;
}

} // namespace

TEST_CASE("a clean countdown passes every criterion") {
    const Verdict v = evaluate(cleanRun(30.0, 10));
    CHECK(v.sequenceOk);
    CHECK(v.drawnOk);
    CHECK(v.latencyOk);
    CHECK(v.pass());
    CHECK(v.changes.size() == 10);
    CHECK(v.budgetMs == doctest::Approx(1000.0 / 30.0 + 20.0));
    CHECK(v.maxLatencyMs <= v.budgetMs);
    CHECK(v.problems.empty());
}

TEST_CASE("every framerate of the spec passes a clean run") {
    for (double fps : {25.0, 30.0, 50.0, 60.0}) {
        CAPTURE(fps);
        CHECK(evaluate(cleanRun(fps, 5)).pass());
    }
}

TEST_CASE("a skipped second fails the sequence") {
    VerdictInput in = cleanRun(30.0, 5);
    in.pushes.erase(in.pushes.begin() + 2); // "00:23" never reached the source
    const Verdict v = evaluate(in);
    CHECK_FALSE(v.sequenceOk);
    CHECK_FALSE(v.pass());
    CHECK_FALSE(v.problems.empty());
}

TEST_CASE("seconds out of order fail the sequence") {
    VerdictInput in = cleanRun(30.0, 5);
    std::swap(in.pushes[2].text, in.pushes[3].text);
    CHECK_FALSE(evaluate(in).sequenceOk);
}

TEST_CASE("a text that reached the source but was never drawn fails") {
    VerdictInput in = cleanRun(30.0, 5);
    // Frames keep showing text 2 while text 3 is on the source.
    for (FrameSample& f : in.frames)
        if (f.hash == 1003) f.hash = 1002;
    const Verdict v = evaluate(in);
    CHECK(v.sequenceOk);
    CHECK_FALSE(v.drawnOk);
    CHECK_FALSE(v.pass());
}

TEST_CASE("a change drawn two frames too late fails the latency") {
    VerdictInput in = cleanRun(30.0, 5);
    // Text 3 appears 90 ms after it was due: budget at 30 fps is 33.3 + 20 ms.
    const std::uint64_t late = in.expectedNs[3] + 90'000'000ULL;
    for (FrameSample& f : in.frames)
        if (f.hash == 1003 && f.ns < late) f.hash = 1002;
    const Verdict v = evaluate(in);
    CHECK(v.drawnOk);
    CHECK_FALSE(v.latencyOk);
    CHECK(v.maxLatencyMs > v.budgetMs);
}

TEST_CASE("latency exactly at the budget still passes") {
    VerdictInput in;
    in.fps = 50.0; // 20 ms frames, budget 40 ms
    in.slackNs = 20'000'000ULL;
    in.expectedTexts = {"00:25", "00:24"};
    in.expectedNs = {kStart, kStart + kSecond};
    in.pushes = {{kStart - 1'000'000ULL, "00:25"}, {kStart + kSecond + 1'000'000ULL, "00:24"}};
    for (std::uint64_t t = kStart - kSecond; t <= kStart + 2 * kSecond; t += 20'000'000ULL)
        in.frames.push_back({t, t >= kStart + kSecond + 40'000'000ULL ? 2u : 1u});
    const Verdict v = evaluate(in);
    CHECK(v.latencyOk);
    CHECK(v.maxLatencyMs == doctest::Approx(40.0));
}

TEST_CASE("a frame rendered just after its timestamp may already show the new text") {
    VerdictInput in = cleanRun(30.0, 3);
    // Move the last frame before the push of text 2 to 2 ms before that push, and let it
    // already show text 2 (OBS renders a frame slightly after its timestamp).
    const std::uint64_t push = in.pushes[2].ns;
    FrameSample* last = nullptr;
    for (FrameSample& f : in.frames)
        if (f.ns < push) last = &f;
    REQUIRE(last != nullptr);
    last->ns = push - 2'000'000ULL;
    last->hash = 1002;
    const Verdict v = evaluate(in);
    CHECK(v.pass());
    CHECK(v.changes[1].shownNs == push - 2'000'000ULL);
}

TEST_CASE("no frames at all fails without crashing") {
    VerdictInput in = cleanRun(30.0, 3);
    in.frames.clear();
    const Verdict v = evaluate(in);
    CHECK_FALSE(v.drawnOk);
    CHECK_FALSE(v.pass());
}

TEST_CASE("regionHash sees changes inside the region only") {
    const std::uint32_t w = 8, h = 6, stride = w * 4;
    std::vector<std::uint8_t> frame(stride * h, 0);
    const std::uint64_t base = regionHash(frame.data(), stride, w, h, 2, 1, 3, 2);

    frame[(1 * stride) + (2 * 4)] = 255; // inside (x=2, y=1)
    CHECK(regionHash(frame.data(), stride, w, h, 2, 1, 3, 2) != base);

    std::vector<std::uint8_t> other(stride * h, 0);
    other[(5 * stride) + (7 * 4)] = 255; // outside (x=7, y=5)
    CHECK(regionHash(other.data(), stride, w, h, 2, 1, 3, 2) == base);
}

TEST_CASE("regionHash clips a region that leaves the frame") {
    const std::uint32_t w = 4, h = 4, stride = w * 4;
    std::vector<std::uint8_t> frame(stride * h, 7);
    CHECK_NOTHROW(regionHash(frame.data(), stride, w, h, -10, -10, 100, 100));
    CHECK(regionHash(frame.data(), stride, w, h, -10, -10, 100, 100) ==
          regionHash(frame.data(), stride, w, h, 0, 0, 4, 4));
    CHECK(regionHash(frame.data(), stride, w, h, 10, 10, 5, 5) ==
          regionHash(frame.data(), stride, w, h, 0, 0, 0, 0));
}
