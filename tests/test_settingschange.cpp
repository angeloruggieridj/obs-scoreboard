// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "SettingsChange.hpp"
#include "locale_util.hpp"

using namespace sb;

namespace {
constexpr Micros sec(std::int64_t s) {
    return s * 1000000;
}
MatchSettings settingsFor(const char* id) {
    return MatchSettings::forSport(*findSport(id));
}
MatchEngine engineFor(const char* id) {
    return MatchEngine(settingsFor(id));
}
std::string field(const MatchEngine& e, FieldId id) {
    return e.fields()[static_cast<std::size_t>(id)];
}
} // namespace

TEST_CASE("identical settings change nothing") {
    const MatchSettings s = settingsFor("futsal");
    CHECK(classifySettingsChange(s, s, false) == SettingsChange::None);
    CHECK(classifySettingsChange(s, s, true) == SettingsChange::None);
}

TEST_CASE("texts, formats, labels, names and the stoppage mode are live changes") {
    const MatchSettings base = settingsFor("soccer");
    const auto liveChange = [&](auto edit) {
        MatchSettings s = base;
        edit(s);
        return classifySettingsChange(base, s, true);
    };
    CHECK(liveChange([](MatchSettings& s) { s.homeName = "Roseville"; }) == SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.awayName = "Stillwater"; }) == SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.twoDigitMinutes = false; }) == SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.tenthsInLastMinute = true; }) ==
          SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.stoppage = StoppageMode::RunPastDuration; }) ==
          SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.penaltyLabelFormat = "#{player}"; }) ==
          SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.strength.even = "even"; }) == SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.strength.uneven = "{home}:{away}"; }) ==
          SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) {
              s.playTimeFormat = TimeFormat::HoursMinutesSeconds;
          }) == SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.regulationLabels = {"1T", "2T"}; }) ==
          SettingsChange::Live);
    CHECK(liveChange([](MatchSettings& s) { s.overtimeLabels = {"1TS"}; }) == SettingsChange::Live);
}

TEST_CASE("sport structure changes replace the match, or ask once it has started") {
    const MatchSettings base = settingsFor("ice_hockey");
    const auto structure = [&](auto edit) {
        MatchSettings s = base;
        edit(s);
        return s;
    };
    const MatchSettings changes[] = {
        settingsFor("basketball"),
        structure([](MatchSettings& s) { s.sport.periods = 4; }),
        structure([](MatchSettings& s) { s.sport.periodDuration = minutes(12); }),
        structure([](MatchSettings& s) { s.sport.direction = Direction::Up; }),
        structure([](MatchSettings& s) { s.sport.overtimePeriods = 2; }),
        structure([](MatchSettings& s) { s.sport.overtimeDuration = minutes(3); }),
        structure([](MatchSettings& s) { s.sport.playersPerSide = 6; }),
        structure([](MatchSettings& s) { s.sport.minPlayers = 2; }),
        structure([](MatchSettings& s) { s.sport.scoreValues = {1, 2}; }),
        structure([](MatchSettings& s) { s.sport.penaltyOptions.pop_back(); }),
        structure([](MatchSettings& s) { s.sport.penaltyOptions[0].phases[0].duration += 10; }),
        structure([](MatchSettings& s) { s.sport.shots = false; }),
    };
    for (const MatchSettings& next : changes) {
        CHECK(classifySettingsChange(base, next, false) == SettingsChange::ReplaceNow);
        CHECK(classifySettingsChange(base, next, true) == SettingsChange::NeedsNewMatch);
    }
}

TEST_CASE("a structure change wins over a live change in the same edit") {
    const MatchSettings base = settingsFor("futsal");
    MatchSettings next = base;
    next.homeName = "Casa";
    next.sport.periods = 3;
    CHECK(classifySettingsChange(base, next, true) == SettingsChange::NeedsNewMatch);
    CHECK(classifySettingsChange(base, next, false) == SettingsChange::ReplaceNow);
}

TEST_CASE("a fresh match is not started, anything the user did makes it started") {
    CHECK_FALSE(matchStarted(engineFor("ice_hockey")));
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::SetTeamName{Team::Home, "Casa"}, 0);
        CHECK_FALSE(matchStarted(e)); // a name is not game state
    }
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::ClockStart{}, 0);
        CHECK(matchStarted(e));
    }
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::AddStat{Team::Away, Stat::Score, 1}, 0);
        CHECK(matchStarted(e));
    }
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::AddStat{Team::Home, Stat::Shots, 1}, 0);
        CHECK(matchStarted(e)); // not an event, still data
    }
    {
        MatchEngine e = engineFor("basketball");
        e.apply(cmd::AddStat{Team::Home, Stat::Fouls, 1}, 0);
        CHECK(matchStarted(e));
    }
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::ClockAdjust{-seconds(1)}, 0);
        CHECK(matchStarted(e));
    }
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}}}, 0);
        CHECK(matchStarted(e));
    }
    {
        MatchEngine e = engineFor("ice_hockey");
        e.apply(cmd::PeriodNext{}, 0);
        CHECK(matchStarted(e));
    }
}

TEST_CASE("withSettings keeps the match and applies the new texts") {
    MatchEngine e = engineFor("ice_hockey");
    e.apply(cmd::AddStat{Team::Home, Stat::Score, 1}, 0);
    e.apply(cmd::AddStat{Team::Home, Stat::Shots, 3}, 0);
    e.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}}}, 0);
    e.apply(cmd::ClockStart{}, 0);
    e.advance(sec(30));
    e.apply(cmd::ClockStop{}, sec(30));
    const std::string clockBefore = field(e, FieldId::Clock);
    REQUIRE(field(e, FieldId::HomePenalty1Time) == "1:30");

    MatchSettings next = e.settings();
    next.penaltyLabelFormat = "#{player} {time}";
    next.regulationLabels = {"1P", "2P", "3P"};
    next.twoDigitMinutes = false;
    const MatchEngine r = withSettings(e.snapshot(), next);

    CHECK(field(r, FieldId::HomeScore) == "1");
    CHECK(field(r, FieldId::HomeShots) == "3");
    CHECK(field(r, FieldId::HomePenalty1Label) == "#24 1:30");
    CHECK(field(r, FieldId::Period) == "1P");
    CHECK(field(r, FieldId::Clock) == "19:30");
    CHECK(clockBefore == "19:30");
    CHECK_FALSE(r.clock().running());
    CHECK(r.events().events().size() == e.events().events().size());
}

TEST_CASE("withSettings survives turning the run-past mode off beyond the duration") {
    MatchSettings s = settingsFor("soccer");
    s.stoppage = StoppageMode::RunPastDuration;
    MatchEngine e(s);
    e.apply(cmd::ClockStart{}, 0);
    e.advance(sec(46 * 60));
    e.apply(cmd::ClockStop{}, sec(46 * 60));
    MatchSettings next = e.settings();
    next.stoppage = StoppageMode::StopAtDuration;
    CHECK_NOTHROW(withSettings(e.snapshot(), next));
    const MatchEngine r = withSettings(e.snapshot(), next);
    CHECK_FALSE(r.clock().running());
    CHECK_FALSE(field(r, FieldId::Clock).empty());
}

TEST_CASE("settingsProblem names the first thing wrong, or nothing") {
    MatchSettings ok = settingsFor("futsal");
    CHECK(settingsProblem(ok).empty());
    for (const SportPreset& sport : builtInSports()) {
        INFO(sport.id);
        CHECK(settingsProblem(MatchSettings::forSport(sport)).empty());
    }

    MatchSettings s = ok;
    s.sport.scoreValues = {};
    CHECK(settingsProblem(s) == "scoreValues");
    s.sport.scoreValues = {0};
    CHECK(settingsProblem(s) == "scoreValues");
    s.sport.scoreValues = {100};
    CHECK(settingsProblem(s) == "scoreValues");
    s.sport.scoreValues = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(settingsProblem(s) == "scoreValues");

    s = ok;
    s.sport.direction = Direction::Down;
    s.sport.periodDuration = 0;
    CHECK(settingsProblem(s) == "needsDuration");
    s.sport.direction = Direction::Up; // a count-up clock may have no duration (generic sport)
    CHECK(settingsProblem(s).empty());

    s = settingsFor("ice_hockey");
    s.sport.minPlayers = s.sport.playersPerSide + 1;
    CHECK(settingsProblem(s) == "minPlayers");

    // The first problem wins.
    s.sport.scoreValues = {};
    CHECK(settingsProblem(s) == "scoreValues");
}

TEST_CASE("every settings problem is translated") {
    for (const char* file : {"en-US.ini", "it-IT.ini"})
        for (const char* key : {"scoreValues", "needsDuration", "minPlayers"})
            CHECK(localeHasKey(file, std::string("Settings.Problem.") + key));
}
