// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Persistent formats of the plugin: field bindings (stored in the OBS scene collection) and
// global preferences (settings.json). Pure: no OBS, no Qt, no file access.

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ClockColor.hpp"
#include "WebRemote.hpp"
#include "Fields.hpp"
#include "MatchEngine.hpp"

namespace sb {

struct FieldBinding {
    FieldId field = FieldId::Clock;
    std::string uuid;
    std::string name;
};

// The border of the clock card as a progress bar of the period.
enum class PeriodBarMode { Off, Remaining, Consumed };

struct GlobalPrefs {
    std::string lastSport = "futsal";
    // False until the user picked a sport (the first launch shows the picker). Version 1 files
    // never recorded a choice, so they read as false.
    bool sportChosen = false;
    // The last settings used for each sport, so switching sport and back finds them again.
    std::map<std::string, MatchSettings> sportSettings;
    // "#RRGGBB" or empty = the default color.
    std::string scoreColor;
    std::string foulColor;
    // Optional .txt output (spec §6, CLAUDE.md §4.12): OFF by default, and empty until the user
    // picks a folder. See FileOutput.
    bool fileOutputEnabled = false;
    std::string fileOutputDir;
    // Ask GitHub once per OBS start whether a newer release exists (UpdateChecker). ON by default,
    // switchable from the dock menu; nothing but a version number ever leaves the machine.
    bool updateCheck = true;
    // Taller buttons and a little more air in the dock and the windows (DockStyle density).
    bool comfortableDock = false;
    // Accent color of each team's card ("#RRGGBB" or empty = the default blue / orange).
    std::string homeColor;
    std::string awayColor;
    // The border of the clock card fills as the period goes (on by default, in the theme's
    // highlight color unless a color is set).
    PeriodBarMode periodBar = PeriodBarMode::Remaining;
    std::string periodBarColor;
    // The color of the clock digits, optionally by the time left. Off by default.
    ClockColorSettings clockColor;
    // The remote web page: off by default. Two random links, one that only watches and one that
    // also commands; the keys are made on first use (empty until then).
    RemoteMode remoteMode = RemoteMode::Off;
    int remotePort = kDefaultRemotePort;
    std::string remoteViewKey;
    std::string remoteControlKey;
};

constexpr int kBindingsJsonVersion = 1;
constexpr int kPrefsJsonVersion = 3; // version 1 and 2 files are still read

std::string bindingsToJson(const std::vector<FieldBinding>& bindings);
// nullopt (with *error) for a document that is not a supported bindings object; entries that are
// individually broken or unknown are skipped so the rest still loads.
std::optional<std::vector<FieldBinding>> bindingsFromJson(std::string_view json,
                                                          std::string* error = nullptr);

std::string prefsToJson(const GlobalPrefs& prefs);
std::optional<GlobalPrefs> prefsFromJson(std::string_view json, std::string* error = nullptr);

// The settings to start a match of `sport` with: the remembered ones, or the preset's defaults.
MatchSettings settingsForSport(const GlobalPrefs& prefs, const SportPreset& sport);
// Remembers `settings` under its sport id (team names reset to the defaults) and makes that
// sport the last one used.
void rememberSettings(GlobalPrefs& prefs, const MatchSettings& settings);

} // namespace sb
