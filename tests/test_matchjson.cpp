// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdint>
#include <limits>

#include "doctest/doctest.h"
#include "MatchJson.hpp"

#include <nlohmann/json.hpp>

using namespace sb;
using nlohmann::json;

namespace {
constexpr Micros sec(std::int64_t s) {
    return s * 1000000;
}
std::string field(const MatchEngine& e, FieldId id) {
    return e.fields()[static_cast<std::size_t>(id)];
}

// Wraps snapshotFromJson in CHECK_NOTHROW: every malformed-input case below must come back as
// nullopt, never as an escaping exception.
std::optional<MatchSnapshot> parseNoThrow(const std::string& text, std::string* error) {
    std::optional<MatchSnapshot> result;
    CHECK_NOTHROW(result = snapshotFromJson(text, error));
    return result;
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

// A round-tripped snapshot as a mutable nlohmann tree, for tests that need to inject a specific
// (wrong) JSON value rather than search-and-replace pretty-printed text.
json goodJson() {
    return json::parse(toJson(busyHockeyMatch().snapshot()));
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

TEST_CASE("basketball fouls of an earlier quarter come back after a save in a later one") {
    MatchEngine e(MatchSettings::forSport(*findSport("basketball")));
    e.apply(cmd::AddStat{Team::Home, Stat::Fouls, 3}, 0);
    e.apply(cmd::PeriodNext{}, 0);
    e.apply(cmd::AddStat{Team::Home, Stat::Fouls, 1}, 0);
    const auto back = snapshotFromJson(toJson(e.snapshot()), nullptr);
    REQUIRE(back.has_value());
    MatchEngine restored = MatchEngine::fromSnapshot(*back);
    CHECK(field(restored, FieldId::HomeFouls) == "1");
    restored.apply(cmd::PeriodPrev{}, 0);
    CHECK(field(restored, FieldId::HomeFouls) == "3");
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

TEST_CASE("duplicate penalty ids across the two teams' boxes are rejected") {
    // validate()'s id set used to be declared per box: the same id reused in both teams' boxes
    // passed, and PenaltyEdit/PenaltyCancel (which search Home first) would then hit the wrong
    // penalty.
    MatchEngine e(MatchSettings::forSport(*findSport("ice_hockey")));
    e.apply(cmd::PenaltyAdd{Team::Home, "5", {{minutes(2), true}}}, 0);
    e.apply(cmd::PenaltyAdd{Team::Away, "9", {{minutes(2), true}}}, 0);
    json j = json::parse(toJson(e.snapshot()));
    REQUIRE(j["penalties"][0][0]["id"] == 1);
    REQUIRE(j["penalties"][1][0]["id"] == 2);
    j["penalties"][1][0]["id"] = 1; // away's penalty now collides with home's

    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("sport period/overtime durations and the clock/period-end values are bounded to 24h") {
    // Values near INT64_MAX overflow in MatchEngine::playTime()/Periods::displayOffset() and
    // Clock::computeAt(): every stored time field must be a plausible tenths-of-a-second value,
    // never an attacker- or corruption-supplied extreme.
    const std::int64_t huge = std::numeric_limits<std::int64_t>::max() / 2;
    {
        json j = goodJson();
        j["settings"]["sport"]["periodDuration"] = huge;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
    {
        json j = goodJson();
        j["settings"]["sport"]["overtimeDuration"] = huge;
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        json j = goodJson();
        j["clockValue"] = huge;
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        // futsal has a periodEndValues entry once a period has been completed.
        MatchEngine e(MatchSettings::forSport(*findSport("futsal")));
        e.apply(cmd::PeriodNext{}, 0);
        json j = json::parse(toJson(e.snapshot()));
        REQUIRE_FALSE(j["periodEndValues"].empty());
        j["periodEndValues"][0][1] = huge;
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
}

TEST_CASE("penalty phase duration and remaining time are bounded to 24h") {
    const std::int64_t huge = std::numeric_limits<std::int64_t>::max() / 2;
    {
        json j = goodJson();
        // The away team's first phase duration (2 minutes = 1200 tenths).
        j["penalties"][1][0]["phases"][0]["duration"] = huge;
        j["penalties"][1][0]["remaining"] = huge;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
    {
        json j = goodJson();
        j["penalties"][1][0]["remaining"] = huge; // duration untouched: remaining > duration anyway
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
}

TEST_CASE("event playTime is bounded to 24h and event.at must not be negative") {
    const std::int64_t huge = std::numeric_limits<std::int64_t>::max() / 2;
    {
        json j = goodJson();
        j["events"][0]["playTime"] = huge;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
    {
        json j = goodJson();
        j["events"][0]["at"] = -1;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("more than 8 penalties in one box is rejected") {
    json j = goodJson();
    const json templatePenalty = j["penalties"][1][0]; // the away team's one penalty
    json box = json::array();
    for (int i = 1; i <= 9; ++i) {
        json p = templatePenalty;
        p["id"] = i;
        box.push_back(p);
    }
    j["penalties"][1] = box;
    j["nextPenaltyId"] = 10;

    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("an unknown enum string is rejected, never mapped to the first entry") {
    // NLOHMANN_JSON_SERIALIZE_ENUM would silently turn each of these into the first mapped
    // enumerator (StopAtDuration / Home / Up); a stable roster of values makes that dangerous.
    {
        json j = goodJson();
        j["settings"]["stoppage"] = "bogus";
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        json j = goodJson();
        j["events"][1]["team"] = "referee"; // a string, but not "home"/"away"
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        json j = goodJson();
        j["settings"]["sport"]["direction"] = "sideways";
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("a wrong-typed enum value is rejected") {
    {
        json j = goodJson();
        j["settings"]["stoppage"] = 1; // a number, not a string
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        json j = goodJson();
        j["events"][1]["team"] = 42; // the away penalty's event: team must be "home"/"away"/null
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        json j = goodJson();
        j["settings"]["sport"]["direction"] = true;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("a floating point value in an integer field is rejected, never truncated") {
    // get_arithmetic_value's static_cast<T>(double) is undefined behavior once the value is out
    // of T's range, and silently drops the fraction even when it isn't (e.g. 1.5 -> 1).
    {
        json j = goodJson();
        j["clockValue"] = 1e300;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
    {
        json j = goodJson();
        j["clockValue"] = 1.5;
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
}

TEST_CASE("a negative value into the (unsigned) penalty phase index is rejected") {
    json j = goodJson();
    j["penalties"][1][0]["phase"] = -1;
    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("a negative value into an unsigned field is rejected even with no bounds check to catch"
          " the wraparound downstream") {
    // Unlike the penalty phase index above (also caught by validate()'s own
    // "phase >= phases.size()" once a negative wraps to a huge std::size_t), nextPenaltyId has no
    // such second line of defense: nothing else in validate() bounds it from below, so a negative
    // that silently wrapped to a huge PenaltyId would sail through unless strictInt rejects the
    // negative value itself.
    json j = goodJson();
    j["nextPenaltyId"] = -1;
    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("a value beyond INT_MAX into an int field is rejected") {
    json j = goodJson();
    j["stoppageAnnounced"] = static_cast<std::int64_t>(std::numeric_limits<int>::max()) + 1;
    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("an unlimited-overtime sport still bounds the period, at regulation + 99") {
    // basketball has overtimePeriods == kUnlimited: periods() and displayOffset() loop up to the
    // period index, so a corrupted huge value must be rejected, not merely accepted forever.
    const MatchEngine basketball(MatchSettings::forSport(*findSport("basketball"))); // 4 periods
    const json base = json::parse(toJson(basketball.snapshot()));

    {
        json j = base;
        j["period"] = 103; // 4 regulation + 99 (the unlimited-overtime cap)
        CHECK(parseNoThrow(j.dump(), nullptr).has_value());
    }
    {
        json j = base;
        j["period"] = 104;
        std::string error;
        CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
        CHECK_FALSE(error.empty());
    }
    {
        json j = base;
        j["period"] = 1000000;
        CHECK_FALSE(parseNoThrow(j.dump(), nullptr).has_value());
    }
}

TEST_CASE("a huge sport.periods is rejected instead of looping playTime()/displayOffset()") {
    // Before this became a bounds check, only kUnlimited overtime was capped: a finite
    // sport.periods (or a finite overtimePeriods) of ~2e9 sailed through and made
    // MatchEngine::playTime()/Periods::displayOffset() loop that many times per fields() call.
    json j = goodJson();
    j["settings"]["sport"]["periods"] = 2000000000;
    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("a huge finite overtimePeriods is rejected") {
    json j = goodJson();
    j["settings"]["sport"]["overtimePeriods"] = 2000000000;
    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("a negative overtimePeriods other than kUnlimited is rejected") {
    json j = goodJson();
    j["settings"]["sport"]["overtimePeriods"] =
        -2; // kUnlimited is -1, everything else must be >= 0
    std::string error;
    CHECK_FALSE(parseNoThrow(j.dump(), &error).has_value());
    CHECK_FALSE(error.empty());
}
