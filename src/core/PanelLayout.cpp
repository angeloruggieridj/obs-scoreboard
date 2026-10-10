// SPDX-License-Identifier: GPL-2.0-or-later
#include "PanelLayout.hpp"

#include <algorithm>

namespace sb {

bool operator==(const PanelLayout& a, const PanelLayout& b) {
    return a.shots == b.shots && a.fouls == b.fouls && a.fouls2 == b.fouls2 &&
           a.penalties == b.penalties && a.strength == b.strength && a.stoppage == b.stoppage &&
           a.regulationPeriods == b.regulationPeriods && a.segmentLabel == b.segmentLabel &&
           a.scoreLabel == b.scoreLabel && a.foulsLabel == b.foulsLabel &&
           a.fouls2Label == b.fouls2Label && a.scoreValues == b.scoreValues;
}

PanelLayout panelLayout(const MatchSettings& settings) {
    const SportPreset& sport = settings.sport;
    PanelLayout l;
    l.shots = sport.shots;
    l.fouls = sport.fouls;
    l.fouls2 = sport.fouls2;
    l.penalties = sport.hasPenalties();
    l.strength = sport.strengthSource != StrengthSource::None;
    l.stoppage = sport.direction == Direction::Up && sport.periodDuration > 0;
    l.regulationPeriods = sport.periods;
    l.segmentLabel = sport.segment;
    l.scoreLabel = sport.scoreLabel;
    l.foulsLabel = sport.foulsLabel;
    l.fouls2Label = sport.fouls2Label;
    l.scoreValues = sport.scoreValues;
    return l;
}

StoppageMode stoppageForToggle(const SportPreset& sport, bool runPastDuration) {
    if (runPastDuration) return StoppageMode::RunPastDuration;
    return sport.stoppage == StoppageMode::RunPastDuration ? StoppageMode::StopAtDuration
                                                           : sport.stoppage;
}

Command stoppageStep(const MatchEngine& engine, int delta) {
    return cmd::StoppageSet{std::max(0, engine.stoppageAnnounced() + delta)};
}

const std::vector<std::string_view>& commandReasonKeys() {
    static const std::vector<std::string_view> keys = {
        "clock.atLimit",    "clock.running",   "match.ended",      "penalty.disabled",
        "penalty.full",     "penalty.invalid", "penalty.notFound", "period.clockRunning",
        "period.midPeriod", "period.noNext",   "period.noPrev",    "plugin.shuttingDown",
        "stat.disabled"};
    return keys;
}

} // namespace sb
