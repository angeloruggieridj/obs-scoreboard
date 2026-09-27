// SPDX-License-Identifier: GPL-2.0-or-later
#include "Types.hpp"

namespace sb {

std::string_view teamKey(Team t) {
    return t == Team::Home ? "home" : "away";
}

std::optional<Team> teamFromKey(std::string_view key) {
    if (key == "home") return Team::Home;
    if (key == "away") return Team::Away;
    return std::nullopt;
}

} // namespace sb
