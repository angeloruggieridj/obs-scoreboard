// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>

#include "PluginState.hpp"

// settings.json in the module's config folder. An unreadable file is logged and ignored.
class GlobalSettings {
public:
    void load();
    bool save();
    const sb::GlobalPrefs& prefs() const { return prefs_; }
    void setLastSport(std::string sportId);

private:
    sb::GlobalPrefs prefs_;
};
