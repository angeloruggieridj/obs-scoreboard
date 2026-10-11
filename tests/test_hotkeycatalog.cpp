// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "HotkeyCatalog.hpp"
#include "locale_util.hpp"

#include <set>
#include <string>

using namespace sb;

namespace {
const HotkeySpec& find(const std::string& name) {
    for (const HotkeySpec& s : hotkeyCatalog())
        if (s.name == name) return s;
    FAIL("no hotkey named " << name);
    return hotkeyCatalog().front();
}
PanelLayout layoutOf(const char* id) {
    return panelLayout(MatchSettings::forSport(*findSport(id)));
}
bool applicable(const std::string& name, const char* sport) {
    return isHotkeyApplicable(find(name).command, layoutOf(sport));
}
} // namespace

TEST_CASE("the catalog has 40 unique, stable names") {
    const auto& catalog = hotkeyCatalog();
    CHECK(catalog.size() == 40);
    std::set<std::string> names;
    for (const HotkeySpec& s : catalog) {
        CHECK(s.name.rfind("obs_scoreboard.", 0) == 0);
        names.insert(s.name);
    }
    CHECK(names.size() == 40);
    CHECK(catalog.front().name == "obs_scoreboard.clock_toggle");
    CHECK(find("obs_scoreboard.home.score_plus1").team == Team::Home);
    CHECK(find("obs_scoreboard.away.fouls2_minus1").team == Team::Away);
    CHECK_FALSE(find("obs_scoreboard.stoppage_plus1").team.has_value());
}

TEST_CASE("the commands behind the names are the right ones") {
    CHECK(std::holds_alternative<cmd::ClockToggle>(find("obs_scoreboard.clock_toggle").command));
    CHECK(std::get<cmd::ClockAdjust>(find("obs_scoreboard.clock_min_up").command).delta ==
          minutes(1));
    CHECK(std::get<cmd::ClockAdjust>(find("obs_scoreboard.clock_sec_down").command).delta ==
          -seconds(1));
    CHECK(find("obs_scoreboard.clock_min_up").repeats);
    CHECK(find("obs_scoreboard.clock_sec_down").repeats);
    CHECK_FALSE(find("obs_scoreboard.clock_toggle").repeats);
    CHECK_FALSE(find("obs_scoreboard.home.score_plus1").repeats);

    const auto score5 = std::get<cmd::AddStat>(find("obs_scoreboard.away.score_plus5").command);
    CHECK(score5.team == Team::Away);
    CHECK(score5.stat == Stat::Score);
    CHECK(score5.delta == 5);
    const auto foul = std::get<cmd::AddStat>(find("obs_scoreboard.home.fouls_minus1").command);
    CHECK(foul.stat == Stat::Fouls);
    CHECK(foul.delta == -1);
    const auto cancel =
        std::get<cmd::PenaltyCancelActive>(find("obs_scoreboard.home.penalty_cancel2").command);
    CHECK(cancel.team == Team::Home);
    CHECK(cancel.slot == 1);
    CHECK(std::holds_alternative<cmd::PeriodPrev>(find("obs_scoreboard.period_prev").command));
}

TEST_CASE("score buttons the sport does not have are ignored") {
    CHECK(applicable("obs_scoreboard.home.score_plus1", "ice_hockey"));
    CHECK_FALSE(applicable("obs_scoreboard.home.score_plus2", "ice_hockey"));
    CHECK(applicable("obs_scoreboard.home.score_plus3", "basketball"));
    CHECK_FALSE(applicable("obs_scoreboard.home.score_plus5", "basketball"));
    CHECK(applicable("obs_scoreboard.away.score_plus6", "american_football"));
    CHECK_FALSE(applicable("obs_scoreboard.away.score_plus5", "american_football"));
    CHECK(applicable("obs_scoreboard.away.score_plus5", "rugby_union"));
    CHECK(applicable("obs_scoreboard.home.score_minus1", "ice_hockey")); // always
    CHECK(applicable("obs_scoreboard.home.score_minus1", "generic"));
}

TEST_CASE("counters, penalties and stoppage need the sport to use them") {
    CHECK(applicable("obs_scoreboard.home.shots_plus1", "ice_hockey"));
    CHECK_FALSE(applicable("obs_scoreboard.home.shots_plus1", "basketball"));
    CHECK(applicable("obs_scoreboard.home.fouls_plus1", "basketball"));
    CHECK_FALSE(applicable("obs_scoreboard.home.fouls_plus1", "ice_hockey"));
    CHECK(applicable("obs_scoreboard.home.fouls2_plus1", "soccer"));
    CHECK_FALSE(applicable("obs_scoreboard.home.fouls2_plus1", "futsal"));
    CHECK(applicable("obs_scoreboard.home.penalty_cancel1", "ice_hockey"));
    CHECK_FALSE(applicable("obs_scoreboard.home.penalty_cancel1", "basketball"));
    CHECK(applicable("obs_scoreboard.stoppage_plus1", "soccer"));
    CHECK_FALSE(applicable("obs_scoreboard.stoppage_plus1", "futsal"));
    CHECK(applicable("obs_scoreboard.clock_toggle", "generic"));
    CHECK(applicable("obs_scoreboard.period_next", "generic"));
}

TEST_CASE("stoppage hotkeys add to the announced minutes, never below zero") {
    MatchEngine e(MatchSettings::forSport(*findSport("soccer")));
    const HotkeySpec plus = find("obs_scoreboard.stoppage_plus1");
    const HotkeySpec minus = find("obs_scoreboard.stoppage_minus1");
    CHECK(std::get<cmd::StoppageSet>(resolveHotkey(plus, e)).minutes == 1);
    CHECK(std::get<cmd::StoppageSet>(resolveHotkey(minus, e)).minutes == 0);
    e.apply(cmd::StoppageSet{2}, 0);
    CHECK(std::get<cmd::StoppageSet>(resolveHotkey(plus, e)).minutes == 3);
    CHECK(std::get<cmd::StoppageSet>(resolveHotkey(minus, e)).minutes == 1);
    // A normal hotkey resolves to its own command.
    CHECK(std::holds_alternative<cmd::ClockStart>(
        resolveHotkey(find("obs_scoreboard.clock_start"), e)));
}

TEST_CASE("every hotkey and team label is translated") {
    for (const char* file : {"en-US.ini", "it-IT.ini"}) {
        CHECK(localeHasKey(file, "Hotkey.Team.Home"));
        CHECK(localeHasKey(file, "Hotkey.Team.Away"));
        for (const HotkeySpec& s : hotkeyCatalog()) {
            INFO(file << ": " << s.action);
            CHECK(localeHasKey(file, "Hotkey." + s.action));
        }
    }
}
