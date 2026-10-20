// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebState.hpp"

#include <nlohmann/json.hpp>

#include "ClockColor.hpp"
#include "PanelLayout.hpp"
#include "PanelModel.hpp"
#include "VendorProtocol.hpp"

namespace sb {

namespace {

const char* barKey(PeriodBarMode mode) {
    switch (mode) {
    case PeriodBarMode::Off:
        return "off";
    case PeriodBarMode::Consumed:
        return "consumed";
    case PeriodBarMode::Remaining:
        break;
    }
    return "remaining";
}

} // namespace

std::string webStateJson(const MatchEngine& engine, bool canControl, const GlobalPrefs& prefs) {
    using json = nlohmann::json;
    json doc = json::parse(vendorStateJson(engine));
    json& state = doc["state"];
    const MatchSettings& settings = engine.settings();

    const PanelLayout layout = panelLayout(settings);
    state["layout"] = {{"shots", layout.shots},
                       {"fouls", layout.fouls},
                       {"fouls2", layout.fouls2},
                       {"penalties", layout.penalties},
                       {"strength", layout.strength},
                       {"stoppage", layout.stoppage},
                       {"scoreValues", layout.scoreValues},
                       {"foulsLabel", layout.foulsLabel},
                       {"fouls2Label", layout.fouls2Label},
                       {"scoreLabel", layout.scoreLabel},
                       {"segmentLabel", layout.segmentLabel}};
    state["control"] = canControl;

    // The penalty rows again, with the share of time left (the meter under each row of the dock).
    for (Team team : {Team::Home, Team::Away}) {
        json rows = json::array();
        for (const PenaltyRow& r : penaltyRows(engine.penalties(team)))
            rows.push_back({{"id", r.id},
                            {"player", r.player},
                            {"time", r.time},
                            {"phase2", r.phase2},
                            {"queued", r.queued},
                            {"percent", r.percent}});
        state["penalties"][std::string(teamKey(team))] = rows;
    }

    const Clock& clock = engine.clock();
    const Direction direction = settings.sport.direction;
    const Tenths left = timeLeft(clock.value(), clock.duration(), direction);
    // What the page needs to run the clock by itself between two answers (and to show tenths
    // smoothly): the value, whether it runs, and the rules of MatchEngine::fields().
    state["clockModel"] = {
        {"value", clock.value()},
        {"offset", engine.periods().displayOffset()},
        {"running", clock.running()},
        {"down", direction == Direction::Down},
        {"duration", clock.duration()},
        {"stopAtLimit", settings.stoppage == StoppageMode::StopAtDuration},
        {"secondary", settings.stoppage == StoppageMode::SecondaryCounter &&
                          direction == Direction::Up && clock.duration() > 0},
        {"twoDigit", settings.twoDigitMinutes},
        {"tenths", settings.tenthsInLastMinute},
    };
    state["look"] = {
        {"periodBar", barKey(prefs.periodBar)},
        {"periodBarColor", prefs.periodBarColor},
        {"share", shareLeft(clock.value(), clock.duration(), direction)},
        {"homeColor", prefs.homeColor},
        {"awayColor", prefs.awayColor},
        {"scoreColor",
         prefs.scoreColor.empty() ? std::string(kDefaultScoreColor) : prefs.scoreColor},
        {"foulColor", prefs.foulColor.empty() ? std::string(kDefaultFoulColor) : prefs.foulColor},
        {"clockColor", clockColorAt(prefs.clockColor, left)},
    };
    return doc.dump(-1, ' ', false, json::error_handler_t::replace);
}

} // namespace sb
