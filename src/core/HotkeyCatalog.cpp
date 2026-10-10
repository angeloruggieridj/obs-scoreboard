// SPDX-License-Identifier: GPL-2.0-or-later
#include "HotkeyCatalog.hpp"

#include <algorithm>
#include <utility>

namespace sb {

namespace {

std::vector<HotkeySpec> makeCatalog() {
    std::vector<HotkeySpec> c;
    const auto add = [&c](std::optional<Team> team, std::string action, Command command,
                          bool repeats = false, int stoppageDelta = 0) {
        HotkeySpec s;
        s.name =
            "obs_scoreboard." + (team ? std::string(teamKey(*team)) + "." : std::string()) + action;
        s.action = std::move(action);
        s.team = team;
        s.command = std::move(command);
        s.repeats = repeats;
        s.stoppageDelta = stoppageDelta;
        c.push_back(std::move(s));
    };

    add(std::nullopt, "clock_toggle", cmd::ClockToggle{});
    add(std::nullopt, "clock_start", cmd::ClockStart{});
    add(std::nullopt, "clock_stop", cmd::ClockStop{});
    add(std::nullopt, "clock_reset", cmd::ClockReset{});
    add(std::nullopt, "clock_min_up", cmd::ClockAdjust{minutes(1)}, true);
    add(std::nullopt, "clock_min_down", cmd::ClockAdjust{-minutes(1)}, true);
    add(std::nullopt, "clock_sec_up", cmd::ClockAdjust{seconds(1)}, true);
    add(std::nullopt, "clock_sec_down", cmd::ClockAdjust{-seconds(1)}, true);
    add(std::nullopt, "period_next", cmd::PeriodNext{});
    add(std::nullopt, "period_prev", cmd::PeriodPrev{});

    for (Team team : {Team::Home, Team::Away}) {
        for (int value : {1, 2, 3, 5, 6})
            add(team, "score_plus" + std::to_string(value), cmd::AddStat{team, Stat::Score, value});
        add(team, "score_minus1", cmd::AddStat{team, Stat::Score, -1});
        add(team, "shots_plus1", cmd::AddStat{team, Stat::Shots, 1});
        add(team, "shots_minus1", cmd::AddStat{team, Stat::Shots, -1});
        add(team, "fouls_plus1", cmd::AddStat{team, Stat::Fouls, 1});
        add(team, "fouls_minus1", cmd::AddStat{team, Stat::Fouls, -1});
        add(team, "fouls2_plus1", cmd::AddStat{team, Stat::Fouls2, 1});
        add(team, "fouls2_minus1", cmd::AddStat{team, Stat::Fouls2, -1});
        add(team, "penalty_cancel1", cmd::PenaltyCancelActive{team, 0});
        add(team, "penalty_cancel2", cmd::PenaltyCancelActive{team, 1});
    }

    add(std::nullopt, "stoppage_plus1", cmd::StoppageSet{}, false, 1);
    add(std::nullopt, "stoppage_minus1", cmd::StoppageSet{}, false, -1);
    return c;
}

} // namespace

const std::vector<HotkeySpec>& hotkeyCatalog() {
    static const std::vector<HotkeySpec> catalog = makeCatalog();
    return catalog;
}

Command resolveHotkey(const HotkeySpec& spec, const MatchEngine& engine) {
    if (spec.stoppageDelta != 0) return stoppageStep(engine, spec.stoppageDelta);
    return spec.command;
}

bool isHotkeyApplicable(const Command& command, const PanelLayout& layout) {
    if (const auto* stat = std::get_if<cmd::AddStat>(&command)) {
        switch (stat->stat) {
        case Stat::Score:
            if (stat->delta <= 0) return true;
            return std::find(layout.scoreValues.begin(), layout.scoreValues.end(), stat->delta) !=
                   layout.scoreValues.end();
        case Stat::Shots:
            return layout.shots;
        case Stat::Fouls:
            return layout.fouls;
        case Stat::Fouls2:
            return layout.fouls2;
        }
        return false;
    }
    if (std::holds_alternative<cmd::PenaltyCancelActive>(command)) return layout.penalties;
    if (std::holds_alternative<cmd::StoppageSet>(command)) return layout.stoppage;
    return true;
}

} // namespace sb
