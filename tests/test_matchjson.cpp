// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "MatchJson.hpp"

using namespace sb;

namespace {
constexpr Micros sec(std::int64_t s) {
    return s * 1000000;
}
std::string field(const MatchEngine& e, FieldId id) {
    return e.fields()[static_cast<std::size_t>(id)];
}

MatchEngine busyHockeyMatch() {
    MatchSettings s = MatchSettings::forSport(*findSport("ice_hockey"));
    s.homeName = "Città";
    s.regulationLabels = {"1°", "2°", "3°"};
    s.tenthsInLastMinute = true;
    MatchEngine e(s);
    e.apply(cmd::AddStat{Team::Home, Stat::Score, 3}, 0);
    e.apply(cmd::AddStat{Team::Away, Stat::Shots, 11}, 0);
    e.apply(cmd::PenaltyAdd{Team::Away, "8", {{minutes(2), true}, {minutes(10), false}}}, 0);
    e.apply(cmd::ClockStart{}, 0);
    e.advance(sec(75));
    e.apply(cmd::ClockStop{}, sec(75));
    e.apply(cmd::StoppageSet{1}, sec(75));
    return e;
}

MatchEngine twoPenaltiesOnOneTeam() {
    MatchEngine e(MatchSettings::forSport(*findSport("ice_hockey")));
    e.apply(cmd::PenaltyAdd{Team::Away, "8", {{minutes(2), true}}}, 0);
    e.apply(cmd::PenaltyAdd{Team::Away, "9", {{minutes(2), true}}}, 0);
    return e;
}
} // namespace

TEST_CASE("a match survives a JSON round trip") {
    const MatchEngine e = busyHockeyMatch();
    const std::string json = toJson(e.snapshot());
    std::string error;
    const auto back = snapshotFromJson(json, &error);
    REQUIRE_MESSAGE(back.has_value(), error);
    const MatchEngine restored = MatchEngine::fromSnapshot(*back);
    CHECK(restored.fields() == e.fields());
    CHECK(restored.settings().homeName == "Città");
    CHECK(restored.settings().sport.penaltyOptions.size() ==
          e.settings().sport.penaltyOptions.size());
    CHECK(restored.events().events().size() == e.events().events().size());
    REQUIRE(restored.events().events().size() == 3); // score, penalty, period start
    CHECK(restored.events().events()[1].team == Team::Away);
    CHECK_FALSE(restored.events().events()[2].team.has_value());
}

TEST_CASE("futsal fouls of the 1st half come back after a save in the 2nd") {
    MatchEngine e(MatchSettings::forSport(*findSport("futsal")));
    e.apply(cmd::AddStat{Team::Home, Stat::Fouls, 4}, 0);
    e.apply(cmd::PeriodNext{}, 0);
    e.apply(cmd::AddStat{Team::Home, Stat::Fouls, 1}, 0);
    const auto back = snapshotFromJson(toJson(e.snapshot()), nullptr);
    REQUIRE(back.has_value());
    MatchEngine restored = MatchEngine::fromSnapshot(*back);
    CHECK(field(restored, FieldId::HomeFouls) == "1");
    restored.apply(cmd::PeriodPrev{}, 0);
    CHECK(field(restored, FieldId::HomeFouls) == "4");
}

TEST_CASE("broken input is an error, never an exception") {
    const std::string good = toJson(busyHockeyMatch().snapshot());
    std::string error;

    CHECK_FALSE(snapshotFromJson(good.substr(0, good.size() / 2), &error).has_value());
    CHECK_FALSE(error.empty());

    CHECK_FALSE(snapshotFromJson("[1,2,3]", &error).has_value());
    CHECK_FALSE(snapshotFromJson("", &error).has_value());

    std::string future = good;
    future.replace(future.find("\"version\": 1"), 12, "\"version\": 99");
    CHECK_FALSE(snapshotFromJson(future, &error).has_value());
    CHECK(error.find("version") != std::string::npos);

    // rfind: keys are sorted, so the top-level "period" comes after the events that also carry one.
    std::string wrongType = good;
    wrongType.replace(wrongType.rfind("\"period\": 1"), 11, "\"period\": \"one\"");
    CHECK_FALSE(snapshotFromJson(wrongType, &error).has_value());

    std::string badPeriod = good;
    badPeriod.replace(badPeriod.rfind("\"period\": 1"), 11, "\"period\": 0");
    CHECK_FALSE(snapshotFromJson(badPeriod, &error).has_value());

    std::string badPenalty = good;
    badPenalty.replace(badPenalty.find("\"phase\": 0"), 10, "\"phase\": 7");
    CHECK_FALSE(snapshotFromJson(badPenalty, &error).has_value());

    CHECK_FALSE(snapshotFromJson("{}", nullptr).has_value()); // no error sink: still no crash
}

TEST_CASE("a non-positive penalty phase duration is rejected even on a future phase") {
    const std::string good = toJson(busyHockeyMatch().snapshot());
    std::string error;

    // The away team's penalty is a 2+10: phase 0 (current, duration 1200) is the one "remaining"
    // is checked against; phase 1 (not yet reached, duration 6000 = minutes(10)) is only covered
    // by validating every phase, not just the current one. Corrupting phase 1 alone must still be
    // rejected, and must not be caught merely as "remaining > current phase duration".
    const std::string needle = "\"duration\": 6000";
    REQUIRE_MESSAGE(good.find(needle) != std::string::npos, good);

    std::string zeroDuration = good;
    zeroDuration.replace(zeroDuration.find(needle), needle.size(), "\"duration\": 0");
    CHECK_FALSE(snapshotFromJson(zeroDuration, &error).has_value());
    CHECK_FALSE(error.empty());

    std::string negativeDuration = good;
    negativeDuration.replace(negativeDuration.find(needle), needle.size(), "\"duration\": -5");
    CHECK_FALSE(snapshotFromJson(negativeDuration, nullptr).has_value());
}

TEST_CASE("duplicate penalty ids within the same team's box are rejected") {
    const std::string good = toJson(twoPenaltiesOnOneTeam().snapshot());
    std::string error;

    // Second penalty in the away team's box: "id": 2. Make it collide with the first one's id.
    const std::string needle = "\"id\": 2";
    REQUIRE_MESSAGE(good.find(needle) != std::string::npos, good);
    std::string duplicated = good;
    duplicated.replace(duplicated.find(needle), needle.size(), "\"id\": 1");
    CHECK_FALSE(snapshotFromJson(duplicated, &error).has_value());
    CHECK_FALSE(error.empty());
}
