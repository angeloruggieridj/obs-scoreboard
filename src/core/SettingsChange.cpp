// SPDX-License-Identifier: GPL-2.0-or-later
#include "SettingsChange.hpp"

#include <utility>

namespace sb {

namespace {

bool samePhases(const std::vector<PenaltyPhase>& a, const std::vector<PenaltyPhase>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].duration != b[i].duration || a[i].reducesStrength != b[i].reducesStrength)
            return false;
    return true;
}

bool sameOptions(const std::vector<PenaltyOption>& a, const std::vector<PenaltyOption>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].label != b[i].label || !samePhases(a[i].phases, b[i].phases)) return false;
    return true;
}

bool sameSport(const SportPreset& a, const SportPreset& b) {
    return a.id == b.id && a.segment == b.segment && a.periods == b.periods &&
           a.periodDuration == b.periodDuration && a.direction == b.direction &&
           a.overtimePeriods == b.overtimePeriods && a.overtimeDuration == b.overtimeDuration &&
           a.continuousDisplay == b.continuousDisplay && a.shots == b.shots && a.fouls == b.fouls &&
           a.fouls2 == b.fouls2 && a.foulReset == b.foulReset && a.foulsLabel == b.foulsLabel &&
           a.fouls2Label == b.fouls2Label && a.scoreLabel == b.scoreLabel &&
           a.scoreValues == b.scoreValues && sameOptions(a.penaltyOptions, b.penaltyOptions) &&
           a.playersPerSide == b.playersPerSide && a.minPlayers == b.minPlayers &&
           a.strengthSource == b.strengthSource && a.stoppage == b.stoppage;
}

bool sameLive(const MatchSettings& a, const MatchSettings& b) {
    return a.homeName == b.homeName && a.awayName == b.awayName &&
           a.twoDigitMinutes == b.twoDigitMinutes && a.tenthsInLastMinute == b.tenthsInLastMinute &&
           a.stoppage == b.stoppage && a.penaltyLabelFormat == b.penaltyLabelFormat &&
           a.strength.even == b.strength.even && a.strength.uneven == b.strength.uneven &&
           a.playTimeFormat == b.playTimeFormat && a.regulationLabels == b.regulationLabels &&
           a.overtimeLabels == b.overtimeLabels;
}

} // namespace

SettingsChange classifySettingsChange(const MatchSettings& oldSettings,
                                      const MatchSettings& newSettings, bool matchStarted) {
    if (!sameSport(oldSettings.sport, newSettings.sport))
        return matchStarted ? SettingsChange::NeedsNewMatch : SettingsChange::ReplaceNow;
    return sameLive(oldSettings, newSettings) ? SettingsChange::None : SettingsChange::Live;
}

bool matchStarted(const MatchEngine& engine) {
    if (engine.periods().index() != 1 || !engine.clock().atStart()) return true;
    if (!engine.events().events().empty()) return true;
    for (Team team : {Team::Home, Team::Away}) {
        if (!engine.penalties(team).all().empty()) return true;
        for (Stat stat : {Stat::Score, Stat::Shots, Stat::Fouls, Stat::Fouls2})
            if (engine.counters().get(team, stat) > 0) return true;
    }
    return false;
}

MatchEngine withSettings(const MatchSnapshot& snapshot, const MatchSettings& settings) {
    MatchSnapshot copy = snapshot;
    copy.settings = settings;
    return MatchEngine::fromSnapshot(copy);
}

std::string settingsProblem(const MatchSettings& settings) {
    const SportPreset& sport = settings.sport;
    if (sport.scoreValues.empty() || sport.scoreValues.size() > 8) return "scoreValues";
    for (int value : sport.scoreValues)
        if (value < 1 || value > 99) return "scoreValues";
    if (sport.direction == Direction::Down && sport.periodDuration <= 0) return "needsDuration";
    if (sport.minPlayers > sport.playersPerSide) return "minPlayers";
    return {};
}

} // namespace sb
