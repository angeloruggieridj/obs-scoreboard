// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// What the remote page draws, as JSON: the vendor state of the match, what the dock shows for the
// sport, and the look the user chose for the dock (colors, the progress border, the clock color),
// so the page can match the plugin. No Qt, no sockets.

#include <string>

#include "MatchEngine.hpp"
#include "PluginState.hpp"

namespace sb {

// `canControl`: whether this viewer may command. `prefs`: the look settings of the plugin.
std::string webStateJson(const MatchEngine& engine, bool canControl, const GlobalPrefs& prefs);

} // namespace sb
