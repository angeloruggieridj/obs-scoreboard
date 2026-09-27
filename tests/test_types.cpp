// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Types.hpp"

using namespace sb;

TEST_CASE("time helpers convert to tenths") {
    CHECK(seconds(1) == 10);
    CHECK(minutes(20) == 12000);
    CHECK(kMicrosPerTenth == 100000);
}

TEST_CASE("team keys round-trip") {
    CHECK(teamKey(Team::Home) == "home");
    CHECK(teamKey(Team::Away) == "away");
    CHECK(teamFromKey("home") == Team::Home);
    CHECK(teamFromKey("away") == Team::Away);
    CHECK_FALSE(teamFromKey("visitors").has_value());
    CHECK(teamIndex(Team::Home) == 0);
    CHECK(teamIndex(Team::Away) == 1);
}
