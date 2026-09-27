// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "SportCatalog.hpp"

#include <algorithm>
#include <set>

using namespace sb;

namespace {
struct Row {
    const char* id;
    int periods;
    Tenths duration;
    Direction dir;
    int overtime;
    Tenths overtimeDuration;
};
// The spec's table (section 4), one row per sport.
const Row kTable[] = {
    {"ice_hockey", 3, minutes(20), Direction::Down, 1, minutes(5)},
    {"basketball", 4, minutes(10), Direction::Down, kUnlimited, minutes(5)},
    {"soccer", 2, minutes(45), Direction::Up, 2, minutes(15)},
    {"futsal", 2, minutes(20), Direction::Down, 2, minutes(5)},
    {"american_football", 4, minutes(15), Direction::Down, 1, minutes(10)},
    {"lacrosse", 4, minutes(15), Direction::Down, kUnlimited, minutes(4)},
    {"rugby_union", 2, minutes(40), Direction::Up, 2, minutes(10)},
    {"rugby_sevens", 2, minutes(7), Direction::Up, kUnlimited, minutes(5)},
    {"field_hockey", 4, minutes(15), Direction::Down, 0, 0},
    {"water_polo", 4, minutes(8), Direction::Down, 0, 0},
    {"handball", 2, minutes(30), Direction::Up, 2, minutes(5)},
    {"rink_hockey", 2, minutes(25), Direction::Down, 2, minutes(5)},
    {"floorball", 3, minutes(20), Direction::Down, 1, minutes(10)},
    {"generic", 1, 0, Direction::Up, 0, 0},
};
} // namespace

TEST_CASE("the catalog has the 14 sports of the spec with their timing") {
    CHECK(builtInSports().size() == 14);
    for (const Row& row : kTable) {
        CAPTURE(row.id);
        const SportPreset* s = findSport(row.id);
        REQUIRE(s != nullptr);
        CHECK(s->periods == row.periods);
        CHECK(s->periodDuration == row.duration);
        CHECK(s->direction == row.dir);
        CHECK(s->overtimePeriods == row.overtime);
        CHECK(s->overtimeDuration == row.overtimeDuration);
    }
    CHECK(findSport("curling") == nullptr);
}

TEST_CASE("every preset is internally consistent") {
    std::set<std::string> ids;
    for (const SportPreset& s : builtInSports()) {
        CAPTURE(s.id);
        CHECK(ids.insert(s.id).second);
        CHECK_FALSE(s.segment.empty());
        CHECK_FALSE(s.scoreValues.empty());
        CHECK(s.minPlayers <= s.playersPerSide);
        if (s.strengthSource == StrengthSource::Penalties) {
            CHECK(s.hasPenalties());
            CHECK(s.playersPerSide > 0);
        }
        if (s.strengthSource == StrengthSource::SecondFouls) CHECK(s.fouls2);
        if (s.foulReset == FoulReset::EachRegulationPeriod) CHECK(s.fouls);
        if (s.continuousDisplay) CHECK(s.direction == Direction::Up);
        if (s.stoppage != StoppageMode::StopAtDuration) CHECK(s.direction == Direction::Up);
        for (const PenaltyOption& o : s.penaltyOptions) {
            CHECK_FALSE(o.label.empty());
            CHECK_FALSE(o.phases.empty());
            for (const PenaltyPhase& p : o.phases) CHECK(p.duration > 0);
        }
    }
}

TEST_CASE("sport-specific rules from the spec") {
    CHECK(findSport("futsal")->foulReset == FoulReset::EachRegulationPeriod);
    CHECK(findSport("basketball")->foulReset == FoulReset::EachRegulationPeriod);
    CHECK(findSport("basketball")->scoreValues == std::vector<int>{1, 2, 3});
    CHECK(findSport("rugby_union")->scoreValues == std::vector<int>{5, 2, 3});
    CHECK(findSport("american_football")->scoreValues == std::vector<int>{6, 1, 2, 3});
    CHECK(findSport("soccer")->strengthSource == StrengthSource::SecondFouls);
    CHECK(findSport("soccer")->stoppage == StoppageMode::SecondaryCounter);
    CHECK(findSport("soccer")->continuousDisplay);
    CHECK(findSport("ice_hockey")->shots);
    CHECK(findSport("ice_hockey")->strengthSource == StrengthSource::Penalties);
    CHECK_FALSE(findSport("generic")->hasPenalties());

    const auto& hockey = findSport("ice_hockey")->penaltyOptions;
    const auto misconduct = std::find_if(hockey.begin(), hockey.end(),
                                         [](const PenaltyOption& o) { return o.label == "10'"; });
    REQUIRE(misconduct != hockey.end());
    CHECK_FALSE(misconduct->phases.front().reducesStrength);
    const auto compound = std::find_if(hockey.begin(), hockey.end(),
                                       [](const PenaltyOption& o) { return o.label == "2+2"; });
    REQUIRE(compound != hockey.end());
    CHECK(compound->phases.size() == 2);
}
