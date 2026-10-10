// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Small pure helpers the dock and the match settings dialog need, kept out of the Qt code so they
// can be tested: colors, comma-separated lists, and the rows of a team's penalty list.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "PenaltyBox.hpp"

namespace sb {

constexpr std::string_view kDefaultScoreColor = "#F2C94C";
constexpr std::string_view kDefaultFoulColor = "#EF7D57";

std::optional<std::uint32_t> parseColor(std::string_view text);
std::string formatColor(std::uint32_t rgb);
// True when dark text reads better than light text on this background.
bool darkTextOn(std::uint32_t rgb);

std::optional<std::vector<int>> parseScoreValues(std::string_view text);
std::string formatScoreValues(const std::vector<int>& values);
std::vector<std::string> parseLabelList(std::string_view text);
std::string formatLabelList(const std::vector<std::string>& labels);

struct PenaltyRow {
    PenaltyId id = 0;
    std::string player;
    std::string time;   // current phase, "M:SS"
    std::string phase2; // "+2:00" when a next phase follows, else empty
    bool queued = false;
};

std::vector<PenaltyRow> penaltyRows(const PenaltyBox& box);

} // namespace sb
