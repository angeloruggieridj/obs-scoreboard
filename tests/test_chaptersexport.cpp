// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"

#include "ChaptersExport.hpp"

#include <string>
#include <vector>

using namespace sb;

namespace {
MatchEvent at(EventType type, Micros micros, Tenths playTime) {
    MatchEvent e;
    e.type = type;
    e.at = micros;
    e.playTime = playTime;
    return e;
}
std::string describe(const MatchEvent& e) {
    switch (e.type) {
    case EventType::PeriodStart:
        return "Start";
    case EventType::Score:
        return "Goal";
    default:
        return "Other";
    }
}
} // namespace

TEST_CASE("the YouTube chapter times use M:SS and H:MM:SS") {
    CHECK(chapterTimeString(0) == "0:00");
    CHECK(chapterTimeString(5) == "0:05");
    CHECK(chapterTimeString(65) == "1:05");
    CHECK(chapterTimeString(3599) == "59:59");
    CHECK(chapterTimeString(3600) == "1:00:00");
    CHECK(chapterTimeString(3725) == "1:02:05");
}

TEST_CASE("chapters from the stream start are ascending and the first is at 0:00") {
    const Micros stream = 5'000'000;
    const std::vector<MatchEvent> events = {
        at(EventType::PeriodStart, stream + 65'000'000, 0),
        at(EventType::Score, stream + 70'000'000, seconds(5)), // 5 s after: skipped
        at(EventType::Score, stream + 3'725'000'000LL, minutes(50)),
    };
    const ChaptersResult r =
        buildChapters(events, ChapterBase::StreamStart, stream, "Pre-game", describe);
    CHECK(r.text == "0:00 Pre-game\n1:05 Start\n1:02:05 Goal");
    REQUIRE(r.chapters.size() == 3);
    CHECK(r.chapters[0].seconds == 0);
    CHECK(r.chapters[0].title == "Pre-game");
    CHECK(r.chapters[1].seconds == 65);
    CHECK(r.chapters[2].seconds == 3725);
    CHECK(r.youtubeReady); // exactly three chapters
}

TEST_CASE("chapters from the game time ignore the stream clock") {
    const std::vector<MatchEvent> events = {
        at(EventType::PeriodStart, 0, 0),
        at(EventType::Score, 0, minutes(12) + seconds(3)),
    };
    const ChaptersResult r = buildChapters(events, ChapterBase::PlayTime, 0, "Kick-off", describe);
    CHECK(r.text == "0:00 Kick-off\n12:03 Goal");
    REQUIRE(r.chapters.size() == 2);
    CHECK_FALSE(r.youtubeReady); // YouTube needs at least three
}

TEST_CASE("events before the stream start clamp to zero and are skipped") {
    const ChaptersResult r = buildChapters({at(EventType::Score, 1, 0)}, ChapterBase::StreamStart,
                                           10'000'000, "X", describe);
    CHECK(r.text == "0:00 X");
    REQUIRE(r.chapters.size() == 1);
    CHECK_FALSE(r.youtubeReady);
}

TEST_CASE("the ten second gap is inclusive: nine seconds is skipped, ten is kept") {
    const std::vector<MatchEvent> events = {
        at(EventType::Score, 0, seconds(9)),               // 9 s after 0: skipped
        at(EventType::Score, 0, minutes(1)),               // 1:00: kept
        at(EventType::Score, 0, minutes(1) + seconds(9)),  // 9 s after 1:00: skipped
        at(EventType::Score, 0, minutes(1) + seconds(10)), // exactly 10 s after: kept
    };
    const ChaptersResult r = buildChapters(events, ChapterBase::PlayTime, 0, "Start", describe);
    CHECK(r.text == "0:00 Start\n1:00 Goal\n1:10 Goal");
    REQUIRE(r.chapters.size() == 3);
}

TEST_CASE("a resumed match's events from a previous session are ignored for the stream base") {
    const std::vector<MatchEvent> events = {
        at(EventType::PeriodStart, 100, 0),            // before the stream start
        at(EventType::Score, 100'000'000, seconds(5)), // after now: a previous session
    };
    const ChaptersResult r =
        buildChapters(events, ChapterBase::StreamStart, 1'000'000, "Pre-game", describe, 5'000'000);
    CHECK(r.text == "0:00 Pre-game");
    REQUIRE(r.chapters.size() == 1);
}
