// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"

#include "Fields.hpp"
#include "MatchEngine.hpp"

#include <string>

using namespace sb;

namespace {
MatchEngine engineFor(const char* id) {
    return MatchEngine(MatchSettings::forSport(*findSport(id)));
}
std::string field(const MatchEngine& e, FieldId id) {
    return e.fields()[static_cast<std::size_t>(id)];
}
} // namespace

TEST_CASE("a sport with timed penalties fills the two on-air penalty slots and the strength text") {
    MatchEngine e = engineFor("ice_hockey");
    CHECK(field(e, FieldId::HomePenalty1Player) == "");
    CHECK(field(e, FieldId::HomePenalty1Time) == "");
    CHECK(field(e, FieldId::HomePenalty1Label) == "");
    CHECK(field(e, FieldId::Strength) == ""); // {home}-{away} only when uneven

    CHECK(e.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}}}, 0).isOk());
    CHECK(field(e, FieldId::HomePenalty1Player) == "24");
    CHECK(field(e, FieldId::HomePenalty1Time) == "2:00");
    CHECK(field(e, FieldId::HomePenalty1Label) == "24 2:00"); // default "{player} {time}"
    CHECK(field(e, FieldId::Strength) == "4-5");              // home reduced to 4, away 5

    CHECK(e.apply(cmd::PenaltyAdd{Team::Home, "11", {{minutes(2), true}, {minutes(2), true}}}, 0)
              .isOk());
    CHECK(field(e, FieldId::HomePenalty2Player) == "11");
    CHECK(field(e, FieldId::HomePenalty2Time) == "2:00");
    CHECK(field(e, FieldId::HomePenalty2Label) == "11 2:00");
    CHECK(field(e, FieldId::Strength) == "3-5");

    // A third reducer is queued: no on-air slot, the strength does not change.
    CHECK(e.apply(cmd::PenaltyAdd{Team::Home, "7", {{minutes(2), true}}}, 0).isOk());
    CHECK(field(e, FieldId::HomePenalty1Player) == "24");
    CHECK(field(e, FieldId::HomePenalty2Player) == "11");
    CHECK(field(e, FieldId::Strength) == "3-5");
}

TEST_CASE("the combined label uses the configured placeholders and shows phase 2") {
    MatchSettings s = MatchSettings::forSport(*findSport("ice_hockey"));
    s.penaltyLabelFormat = "#{player} {time} {phase2}";
    MatchEngine e(s);
    CHECK(e.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}, {minutes(2), true}}}, 0)
              .isOk());
    CHECK(field(e, FieldId::HomePenalty1Label) == "#24 2:00 +2:00");

    MatchEngine single = engineFor("ice_hockey");
    single.apply(cmd::PenaltyAdd{Team::Home, "7", {{minutes(5), true}}}, 0);
    CHECK(field(single, FieldId::HomePenalty1Label) == "7 5:00");
}

TEST_CASE("a sport without timed penalties or a strength source leaves those fields empty") {
    MatchEngine e = engineFor("basketball");
    CHECK(field(e, FieldId::HomePenalty1Player) == "");
    CHECK(field(e, FieldId::Strength) == "");
    CHECK(e.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}}}, 0).reason ==
          "penalty.disabled");
}

TEST_CASE("strength text follows the format and the second-fouls source") {
    MatchSettings s = MatchSettings::forSport(*findSport("soccer"));
    s.strength.even = "even";
    s.strength.uneven = "{home}v{away}";
    MatchEngine e(s);
    CHECK(field(e, FieldId::Strength) == "even");
    e.apply(cmd::AddStat{Team::Home, Stat::Fouls2, 1}, 0); // a red card: home drops to 10
    CHECK(field(e, FieldId::Strength) == "10v11");
}

TEST_CASE("the field set names every penalty and strength value with a stable key") {
    for (FieldId id :
         {FieldId::HomePenalty1Player, FieldId::HomePenalty1Time, FieldId::HomePenalty1Label,
          FieldId::HomePenalty2Player, FieldId::HomePenalty2Time, FieldId::HomePenalty2Label,
          FieldId::AwayPenalty1Player, FieldId::AwayPenalty1Time, FieldId::AwayPenalty1Label,
          FieldId::AwayPenalty2Player, FieldId::AwayPenalty2Time, FieldId::AwayPenalty2Label,
          FieldId::Strength}) {
        CHECK_FALSE(std::string(fieldKey(id)).empty());
    }
}
