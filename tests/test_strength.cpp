// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Strength.hpp"

using namespace sb;

TEST_CASE("players on field respect the minimum") {
    CHECK(playersOnField(5, 3, 0) == 5);
    CHECK(playersOnField(5, 3, 1) == 4);
    CHECK(playersOnField(5, 3, 4) == 3);
    CHECK(playersOnField(11, 7, -2) == 11);
    CHECK(playersOnField(0, 3, 1) == 0);
    CHECK(playersOnField(4, 9, 1) == 4); // a minimum above the side size is capped
}

TEST_CASE("strength text uses the even or the uneven format") {
    StrengthFormat f;
    CHECK(formatStrength(5, 5, f) == "");
    CHECK(formatStrength(5, 4, f) == "5-4");
    f.even = "{home} ON {away}";
    f.uneven = "PP {home}v{away}";
    CHECK(formatStrength(5, 5, f) == "5 ON 5");
    CHECK(formatStrength(4, 5, f) == "PP 4v5");
}
