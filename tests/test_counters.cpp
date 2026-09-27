// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Counters.hpp"

using namespace sb;

TEST_CASE("counters never go below zero and report what they applied") {
    Counters c;
    CHECK(c.add(Team::Home, Stat::Score, 3) == 3);
    CHECK(c.add(Team::Home, Stat::Score, -5) == -3);
    CHECK(c.get(Team::Home, Stat::Score) == 0);
    c.add(Team::Away, Stat::Shots, 2);
    c.add(Team::Away, Stat::Fouls2, 1);
    CHECK(c.get(Team::Away, Stat::Shots) == 2);
    CHECK(c.get(Team::Away, Stat::Fouls2) == 1);
    CHECK(c.get(Team::Home, Stat::Shots) == 0);
}

TEST_CASE("futsal: fouls restart in the 2nd half, overtime continues it, going back restores") {
    Counters c;
    c.setFoulReset(FoulReset::EachRegulationPeriod);
    c.setPeriod(1, 2);
    c.add(Team::Home, Stat::Fouls, 3);
    c.add(Team::Away, Stat::Fouls, 1);
    c.setPeriod(2, 2);
    CHECK(c.get(Team::Home, Stat::Fouls) == 0);
    c.add(Team::Home, Stat::Fouls, 2);
    c.setPeriod(3, 2); // first overtime
    CHECK(c.get(Team::Home, Stat::Fouls) == 2);
    c.add(Team::Home, Stat::Fouls, 1);
    c.setPeriod(2, 2);
    CHECK(c.get(Team::Home, Stat::Fouls) == 3);
    c.setPeriod(1, 2);
    CHECK(c.get(Team::Home, Stat::Fouls) == 3);
    CHECK(c.get(Team::Away, Stat::Fouls) == 1);
}

TEST_CASE("without a reset rule fouls are one running total") {
    Counters c;
    c.setPeriod(1, 2);
    c.add(Team::Home, Stat::Fouls, 2);
    c.setPeriod(2, 2);
    CHECK(c.get(Team::Home, Stat::Fouls) == 2);
}

TEST_CASE("restore puts every value back and clamps negatives") {
    Counters c;
    c.setFoulReset(FoulReset::EachRegulationPeriod);
    c.restore({4, 1}, {10, 12}, {0, 1}, {{1, {3, 2}}, {2, {-1, 5}}});
    c.setPeriod(2, 2);
    CHECK(c.get(Team::Home, Stat::Score) == 4);
    CHECK(c.get(Team::Away, Stat::Shots) == 12);
    CHECK(c.get(Team::Away, Stat::Fouls2) == 1);
    CHECK(c.get(Team::Home, Stat::Fouls) == 0);
    CHECK(c.get(Team::Away, Stat::Fouls) == 5);
    CHECK(c.foulBuckets().size() == 2);
}
