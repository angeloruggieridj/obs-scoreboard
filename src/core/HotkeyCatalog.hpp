// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// The fixed set of hotkeys (design §4). It does not depend on the sport, so a user's key
// combinations survive a sport change; hotkeys the current sport does not use are ignored.

#include <optional>
#include <string>
#include <vector>

#include "MatchEngine.hpp"
#include "PanelLayout.hpp"

namespace sb {

struct HotkeySpec {
    std::string name;   // stable OBS hotkey name: "obs_scoreboard.[home.|away.]<action>"
    std::string action; // locale key suffix: "Hotkey.<action>"
    std::optional<Team> team;
    Command command;
    int stoppageDelta = 0; // != 0: the command is StoppageSet(announced + delta), see resolveHotkey
    bool repeats = false;  // the clock arrows: repeat while the key is held
};

const std::vector<HotkeySpec>& hotkeyCatalog();
Command resolveHotkey(const HotkeySpec& spec, const MatchEngine& engine);
bool isHotkeyApplicable(const Command& command, const PanelLayout& layout);

} // namespace sb
