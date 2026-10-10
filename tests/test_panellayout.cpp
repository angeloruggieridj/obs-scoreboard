// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "PanelLayout.hpp"
#include "locale_util.hpp"

#include <set>
#include <string>

using namespace sb;

namespace {
PanelLayout layoutOf(const char* id) {
    return panelLayout(MatchSettings::forSport(*findSport(id)));
}
} // namespace

TEST_CASE("the layout mirrors every preset") {
    CHECK(builtInSports().size() == 14);
    for (const SportPreset& sport : builtInSports()) {
        INFO(sport.id);
        const PanelLayout l = panelLayout(MatchSettings::forSport(sport));
        CHECK(l.shots == sport.shots);
        CHECK(l.fouls == sport.fouls);
        CHECK(l.fouls2 == sport.fouls2);
        CHECK(l.penalties == sport.hasPenalties());
        CHECK(l.strength == (sport.strengthSource != StrengthSource::None));
        CHECK(l.stoppage == (sport.direction == Direction::Up && sport.periodDuration > 0));
        CHECK(l.regulationPeriods == sport.periods);
        CHECK(l.scoreValues == sport.scoreValues);
        CHECK(l.segmentLabel == sport.segment);
        CHECK(l.scoreLabel == sport.scoreLabel);
        CHECK(l.foulsLabel == sport.foulsLabel);
        CHECK(l.fouls2Label == sport.fouls2Label);
    }
}

TEST_CASE("known sports show the rows the spec lists") {
    const PanelLayout hockey = layoutOf("ice_hockey");
    CHECK(hockey.shots);
    CHECK(hockey.penalties);
    CHECK(hockey.strength);
    CHECK_FALSE(hockey.stoppage);

    const PanelLayout basket = layoutOf("basketball");
    CHECK(basket.fouls);
    CHECK_FALSE(basket.shots);
    CHECK_FALSE(basket.penalties);
    CHECK(basket.scoreValues == std::vector<int>{1, 2, 3});

    const PanelLayout soccer = layoutOf("soccer");
    CHECK(soccer.fouls);
    CHECK(soccer.fouls2);
    CHECK(soccer.strength);
    CHECK(soccer.stoppage);
    CHECK(soccer.foulsLabel == "Yellow");
    CHECK(soccer.fouls2Label == "Red");

    const PanelLayout football = layoutOf("american_football");
    CHECK(football.foulsLabel == "Flags");
    CHECK(football.scoreValues == std::vector<int>{6, 1, 2, 3});

    const PanelLayout rugby = layoutOf("rugby_union");
    CHECK(rugby.stoppage);
    CHECK(rugby.penalties);

    const PanelLayout futsal = layoutOf("futsal");
    CHECK(futsal.fouls);
    CHECK_FALSE(futsal.stoppage); // counts down

    const PanelLayout generic = layoutOf("generic");
    CHECK_FALSE(generic.shots);
    CHECK_FALSE(generic.fouls);
    CHECK_FALSE(generic.fouls2);
    CHECK_FALSE(generic.penalties);
    CHECK_FALSE(generic.strength);
    CHECK_FALSE(generic.stoppage); // count-up but no duration
    CHECK(generic.regulationPeriods == 1);
}

TEST_CASE("layouts compare by value") {
    CHECK(layoutOf("soccer") == layoutOf("soccer"));
    CHECK(layoutOf("soccer") != layoutOf("futsal"));
    PanelLayout a = layoutOf("basketball");
    PanelLayout b = a;
    b.scoreValues = {1, 2};
    CHECK(a != b);
}

TEST_CASE("the run-past toggle maps to the sport's default stoppage mode") {
    const SportPreset& soccer = *findSport("soccer"); // default SecondaryCounter
    CHECK(stoppageForToggle(soccer, true) == StoppageMode::RunPastDuration);
    CHECK(stoppageForToggle(soccer, false) == StoppageMode::SecondaryCounter);
    const SportPreset& rugby = *findSport("rugby_union"); // default RunPastDuration
    CHECK(stoppageForToggle(rugby, true) == StoppageMode::RunPastDuration);
    CHECK(stoppageForToggle(rugby, false) == StoppageMode::StopAtDuration);
    const SportPreset& futsal = *findSport("futsal"); // default StopAtDuration
    CHECK(stoppageForToggle(futsal, false) == StoppageMode::StopAtDuration);
}

TEST_CASE("stoppage steps add to the announced minutes and never go below zero") {
    MatchEngine e(MatchSettings::forSport(*findSport("soccer")));
    CHECK(std::get<cmd::StoppageSet>(stoppageStep(e, 1)).minutes == 1);
    CHECK(std::get<cmd::StoppageSet>(stoppageStep(e, -1)).minutes == 0);
    e.apply(cmd::StoppageSet{2}, 0);
    CHECK(std::get<cmd::StoppageSet>(stoppageStep(e, 1)).minutes == 3);
    CHECK(std::get<cmd::StoppageSet>(stoppageStep(e, -1)).minutes == 1);
    CHECK(std::get<cmd::StoppageSet>(stoppageStep(e, -5)).minutes == 0);
}

TEST_CASE("there are 13 distinct reason keys") {
    const auto& keys = commandReasonKeys();
    CHECK(keys.size() == 13);
    std::set<std::string_view> unique(keys.begin(), keys.end());
    CHECK(unique.size() == keys.size());
    CHECK(unique.count("match.ended") == 1);
    CHECK(unique.count("plugin.shuttingDown") == 1);
}

TEST_CASE("every label, sport and reason the dock builds at run time is translated") {
    for (const char* file : {"en-US.ini", "it-IT.ini"}) {
        for (const SportPreset& sport : builtInSports()) {
            INFO(file << ": " << sport.id);
            CHECK(localeHasKey(file, "Sport." + sport.id));
            CHECK(localeHasKey(file, "Panel.Segment." + sport.segment));
            CHECK(localeHasKey(file, "Panel.Score." + sport.scoreLabel));
            CHECK(localeHasKey(file, "Panel.Fouls." + sport.foulsLabel));
            CHECK(localeHasKey(file, "Panel.Fouls2." + sport.fouls2Label));
        }
        for (std::string_view key : commandReasonKeys()) {
            INFO(file << ": " << key);
            CHECK(localeHasKey(file, "Reason." + std::string(key)));
        }
    }
}
