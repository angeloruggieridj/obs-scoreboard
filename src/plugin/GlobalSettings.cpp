// SPDX-License-Identifier: GPL-2.0-or-later
#include "GlobalSettings.hpp"

#include <util/base.h>

#include <utility>

#include "AtomicFile.hpp"
#include "plugin-support.h"

namespace {
constexpr const char* kFile = "settings.json";
}

void GlobalSettings::load() {
    const std::string path = moduleConfigPath(kFile);
    const auto text = readFile(path);
    if (!text) {
        save(); // first run: write the defaults, so the file always exists after a load
        return;
    }
    std::string error;
    if (auto prefs = sb::prefsFromJson(*text, &error)) {
        prefs_ = std::move(*prefs);
    } else {
        obs_log(LOG_WARNING, "ignoring unreadable %s: %s (defaults written)", path.c_str(),
                error.c_str());
        save();
    }
}

bool GlobalSettings::save() {
    std::string error;
    const bool ok = writeFileAtomic(moduleConfigPath(kFile), sb::prefsToJson(prefs_), &error);
    if (!ok) obs_log(LOG_WARNING, "could not save %s: %s", kFile, error.c_str());
    return ok;
}

void GlobalSettings::setLastSport(std::string sportId) {
    if (prefs_.lastSport == sportId && prefs_.sportChosen) return;
    prefs_.lastSport = std::move(sportId);
    prefs_.sportChosen = true;
    save();
}

void GlobalSettings::rememberSettings(const sb::MatchSettings& settings) {
    const std::string before = sb::prefsToJson(prefs_);
    sb::rememberSettings(prefs_, settings); // the free function of PluginState.hpp
    if (sb::prefsToJson(prefs_) != before) save();
}

void GlobalSettings::setColors(std::string scoreColor, std::string foulColor) {
    if (prefs_.scoreColor == scoreColor && prefs_.foulColor == foulColor) return;
    prefs_.scoreColor = std::move(scoreColor);
    prefs_.foulColor = std::move(foulColor);
    save();
}

void GlobalSettings::setComfortableDock(bool comfortable) {
    if (prefs_.comfortableDock == comfortable) return;
    prefs_.comfortableDock = comfortable;
    save();
}

void GlobalSettings::setUpdateCheck(bool enabled) {
    if (prefs_.updateCheck == enabled) return;
    prefs_.updateCheck = enabled;
    save();
}

void GlobalSettings::setFileOutput(bool enabled, std::string dir) {
    if (prefs_.fileOutputEnabled == enabled && prefs_.fileOutputDir == dir) return;
    prefs_.fileOutputEnabled = enabled;
    prefs_.fileOutputDir = std::move(dir);
    save();
}

void GlobalSettings::setFileOutputEnabled(bool enabled) {
    setFileOutput(enabled, prefs_.fileOutputDir);
}
