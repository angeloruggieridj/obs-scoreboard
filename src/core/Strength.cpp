// SPDX-License-Identifier: GPL-2.0-or-later
#include "Strength.hpp"

#include <algorithm>

#include "Format.hpp"

namespace sb {

int playersOnField(int players, int minPlayers, int reduction) {
    if (players <= 0) return 0;
    const int floor = std::min(minPlayers, players);
    return std::max(floor, players - std::max(0, reduction));
}

std::string formatStrength(int home, int away, const StrengthFormat& format) {
    const std::string& tpl = home == away ? format.even : format.uneven;
    return applyTemplate(tpl, {{"home", std::to_string(home)}, {"away", std::to_string(away)}});
}

} // namespace sb
