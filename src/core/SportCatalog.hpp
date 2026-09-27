// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "PenaltyBox.hpp"
#include "Types.hpp"

namespace sb {

struct PenaltyOption {
    std::string label; // shown on the button: "2'", "2+10", "20\""
    std::vector<PenaltyPhase> phases;
};

// Defaults for one sport. Every value can be changed by the user in the match settings.
// Label fields are locale key suffixes resolved by the plugin (e.g. "Period" -> "Segment.Period").
struct SportPreset {
    std::string id;
    std::string segment = "Segment";
    int periods = 1;
    Tenths periodDuration = 0; // 0 = no limit
    Direction direction = Direction::Up;
    int overtimePeriods = 0; // kUnlimited allowed
    Tenths overtimeDuration = 0;
    bool continuousDisplay = false;
    bool shots = false;
    bool fouls = false;
    bool fouls2 = false;
    FoulReset foulReset = FoulReset::Never;
    std::string foulsLabel = "Fouls";
    std::string fouls2Label = "Fouls2";
    std::string scoreLabel = "Score";
    std::vector<int> scoreValues{1};
    std::vector<PenaltyOption> penaltyOptions; // empty = no timed penalties
    int playersPerSide = 0;
    int minPlayers = 0;
    StrengthSource strengthSource = StrengthSource::None;
    StoppageMode stoppage = StoppageMode::StopAtDuration;

    bool hasPenalties() const { return !penaltyOptions.empty(); }
};

const std::vector<SportPreset>& builtInSports();
const SportPreset* findSport(std::string_view id);

} // namespace sb
