// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>

#include "MatchEngine.hpp"
#include "PluginState.hpp"

// settings.json in the module's config folder. An unreadable file is logged and ignored.
class GlobalSettings {
public:
    void load();
    bool save();
    const sb::GlobalPrefs& prefs() const { return prefs_; }
    void setLastSport(std::string sportId);

    // Remembers these settings for their sport (team names excluded) and makes it the last sport.
    void rememberSettings(const sb::MatchSettings& settings);
    void setColors(std::string scoreColor, std::string foulColor);
    void setUpdateCheck(bool enabled);
    void setComfortableDock(bool comfortable);
    // Remembers the optional .txt output preferences (global; never in the scene collection).
    void setFileOutput(bool enabled, std::string dir);
    void setFileOutputEnabled(bool enabled);

private:
    sb::GlobalPrefs prefs_;
};
