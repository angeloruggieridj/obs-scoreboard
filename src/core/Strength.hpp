// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>

namespace sb {

struct StrengthFormat {
    std::string even; // shown when both sides have the same number (default: nothing)
    std::string uneven = "{home}-{away}"; // placeholders {home} and {away}
};

int playersOnField(int players, int minPlayers, int reduction);
std::string formatStrength(int home, int away, const StrengthFormat& format);

} // namespace sb
