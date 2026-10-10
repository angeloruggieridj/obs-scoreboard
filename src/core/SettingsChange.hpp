// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// How a settings edit reaches a running match (design §3.2). Pure: the dialog and the dock only
// carry out the answer.

#include "MatchEngine.hpp"

namespace sb {

enum class SettingsChange {
    None,         // nothing changed
    Live,         // rebuild the engine from a snapshot: scores and penalties stay
    ReplaceNow,   // structure changed on an unstarted match: a new engine is safe
    NeedsNewMatch // structure changed on a started match: ask the user before replacing it
};

SettingsChange classifySettingsChange(const MatchSettings& oldSettings,
                                      const MatchSettings& newSettings, bool matchStarted);

// Period 1, clock at its start value, no event, every counter zero and no penalty.
bool matchStarted(const MatchEngine& engine);

// The same match under new (live) settings. The clock comes back stopped, like any snapshot.
MatchEngine withSettings(const MatchSnapshot& snapshot, const MatchSettings& settings);

// Empty when the settings can be used; otherwise the suffix of the locale key
// "Settings.Problem.<suffix>" that explains the first problem found.
std::string settingsProblem(const MatchSettings& settings);

} // namespace sb
