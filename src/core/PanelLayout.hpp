// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// What the dock shows for a match, decided here so the Qt code never branches on a sport.

#include <string>
#include <string_view>
#include <vector>

#include "MatchEngine.hpp"

namespace sb {

struct PanelLayout {
    bool shots = false;
    bool fouls = false;
    bool fouls2 = false;
    bool penalties = false;
    bool strength = false;
    // Count-up sport with a period duration: shows the stoppage row and the "keep running" toggle.
    bool stoppage = false;
    int regulationPeriods = 1;
    // Locale key suffixes taken from the preset ("Half" -> "Panel.Segment.Half").
    std::string segmentLabel;
    std::string scoreLabel;
    std::string foulsLabel;
    std::string fouls2Label;
    std::vector<int> scoreValues;
};

bool operator==(const PanelLayout& a, const PanelLayout& b);
inline bool operator!=(const PanelLayout& a, const PanelLayout& b) {
    return !(a == b);
}

PanelLayout panelLayout(const MatchSettings& settings);

// The stoppage mode the "keep running at the end of the period" toggle selects.
StoppageMode stoppageForToggle(const SportPreset& sport, bool runPastDuration);

// The StoppageSet a "+/- minutes" button or hotkey sends: announced minutes + delta, never below 0.
Command stoppageStep(const MatchEngine& engine, int delta);

// Every CommandResult::reason the engine and the plugin can produce; the plugin shows
// "Reason.<key>".
const std::vector<std::string_view>& commandReasonKeys();

} // namespace sb
