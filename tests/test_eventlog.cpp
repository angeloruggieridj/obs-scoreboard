// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "EventLog.hpp"

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

TEST_CASE("chapters from the stream start, first line at 0:00, minimum 10 s apart") {
    const Micros stream = 5'000'000;
    const std::vector<MatchEvent> events = {
        at(EventType::PeriodStart, stream + 65'000'000, 0),
        at(EventType::Score, stream + 70'000'000, seconds(5)), // 5 s after the previous: skipped
        at(EventType::Score, stream + 3'725'000'000LL, minutes(50)),
    };
    CHECK(youtubeChapters(events, ChapterBase::StreamStart, stream, "Pre-game", describe) ==
          "0:00 Pre-game\n1:05 Start\n1:02:05 Goal");
}

TEST_CASE("chapters from play time; events before the stream clamp to zero and are skipped") {
    const std::vector<MatchEvent> events = {
        at(EventType::PeriodStart, 0, 0),
        at(EventType::Score, 0, minutes(12) + seconds(3)),
    };
    CHECK(youtubeChapters(events, ChapterBase::PlayTime, 0, "Kick-off", describe) ==
          "0:00 Kick-off\n12:03 Goal");
    CHECK(youtubeChapters({at(EventType::Score, 1, 0)}, ChapterBase::StreamStart, 10'000'000, "X",
                          describe) == "0:00 X");
}

TEST_CASE("the log keeps events in order and can be restored") {
    EventLog log;
    log.add(at(EventType::Score, 1, 1));
    log.add(at(EventType::MatchEnd, 2, 2));
    REQUIRE(log.events().size() == 2);
    CHECK(log.events()[1].type == EventType::MatchEnd);
    log.restoreFrom({at(EventType::PeriodEnd, 3, 3)});
    CHECK(log.events().size() == 1);
    log.clear();
    CHECK(log.events().empty());
}
