// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "MatchEngine.hpp"

namespace sb {

constexpr int kMatchJsonVersion = 1;

// Pretty-printed (2-space indent) JSON with a top-level "version".
std::string toJson(const MatchSnapshot& snapshot);
// Never throws: any malformed, incomplete, inconsistent or future-version input returns
// nullopt and, when `error` is given, a one-line reason.
std::optional<MatchSnapshot> snapshotFromJson(std::string_view text, std::string* error);

// The match settings alone, as a compact JSON object (the same fields toJson() stores under
// "settings"). Used by GlobalPrefs to remember the last settings of each sport.
std::string settingsToJson(const MatchSettings& settings);
// Never throws; the same sanity limits as snapshotFromJson, plus the score buttons and player
// counts, since these settings come from a user-editable file.
std::optional<MatchSettings> settingsFromJson(std::string_view text, std::string* error);

} // namespace sb
