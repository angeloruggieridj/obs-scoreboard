// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Persistent formats of the plugin: field bindings (stored in the OBS scene collection) and
// global preferences (settings.json). Pure: no OBS, no Qt, no file access.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Fields.hpp"

namespace sb {

struct FieldBinding {
    FieldId field = FieldId::Clock;
    std::string uuid;
    std::string name;
};

struct GlobalPrefs {
    std::string lastSport = "futsal";
};

constexpr int kBindingsJsonVersion = 1;
constexpr int kPrefsJsonVersion = 1;

std::string bindingsToJson(const std::vector<FieldBinding>& bindings);
// nullopt (with *error) for a document that is not a supported bindings object; entries that are
// individually broken or unknown are skipped so the rest still loads.
std::optional<std::vector<FieldBinding>> bindingsFromJson(std::string_view json,
                                                          std::string* error = nullptr);

std::string prefsToJson(const GlobalPrefs& prefs);
std::optional<GlobalPrefs> prefsFromJson(std::string_view json, std::string* error = nullptr);

} // namespace sb
