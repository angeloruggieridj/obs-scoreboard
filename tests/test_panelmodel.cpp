// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "PanelModel.hpp"

using namespace sb;

TEST_CASE("colors parse and format") {
    CHECK(parseColor("#F2C94C") == 0xF2C94Cu);
    CHECK(parseColor("#f2c94c") == 0xF2C94Cu);
    CHECK(formatColor(0xF2C94Cu) == "#F2C94C");
    CHECK(formatColor(0x000A0Bu) == "#000A0B");
    for (const char* bad : {"", "F2C94C", "#F2C94", "#F2C94C0", "#GGGGGG", "red", "# F2C94"}) {
        CAPTURE(bad);
        CHECK_FALSE(parseColor(bad).has_value());
    }
    CHECK(parseColor(kDefaultScoreColor).has_value());
    CHECK(parseColor(kDefaultFoulColor).has_value());
}

TEST_CASE("text color follows the background") {
    CHECK(darkTextOn(0xF2C94Cu)); // default yellow
    CHECK(darkTextOn(0xEF7D57u)); // default orange
    CHECK(darkTextOn(0xFFFFFFu));
    CHECK_FALSE(darkTextOn(0x000000u));
    CHECK_FALSE(darkTextOn(0x0000FFu));
}

TEST_CASE("score button lists") {
    CHECK(parseScoreValues("6, 1, 2, 3") == std::vector<int>{6, 1, 2, 3});
    CHECK(parseScoreValues("1 2;3") == std::vector<int>{1, 2, 3});
    CHECK(parseScoreValues("99") == std::vector<int>{99});
    CHECK(formatScoreValues({6, 1, 2, 3}) == "6, 1, 2, 3");
    CHECK(formatScoreValues({1}) == "1");
    CHECK(formatScoreValues({}) == "");
    for (const char* bad :
         {"", "  ", "0", "100", "-1", "1,x", "1x", "1,1", "1,2,3,4,5,6,7,8,9", "1.5"}) {
        CAPTURE(bad);
        CHECK_FALSE(parseScoreValues(bad).has_value());
    }
    CHECK(parseScoreValues("1,2,3,4,5,6,7,8").has_value()); // exactly 8 is fine
}

TEST_CASE("label lists keep empty entries") {
    CHECK(parseLabelList("1T, 2T") == std::vector<std::string>{"1T", "2T"});
    CHECK(parseLabelList("a,,b") == std::vector<std::string>{"a", "", "b"});
    CHECK(parseLabelList("  ").empty());
    CHECK(parseLabelList("").empty());
    CHECK(parseLabelList(" 1T ") == std::vector<std::string>{"1T"});
    CHECK(formatLabelList({"1T", "2T"}) == "1T, 2T");
    CHECK(formatLabelList({}) == "");
}

TEST_CASE("penalty rows: running first, queued last, phase 2 shown") {
    PenaltyBox box;
    REQUIRE(box.add(1, "24", {{minutes(2), true}}));
    REQUIRE(box.add(2, "11", {{minutes(2), true}, {minutes(2), true}}));
    REQUIRE(box.add(3, "7", {{minutes(2), true}}));   // third strength-reducing: queued
    REQUIRE(box.add(4, "9", {{minutes(10), false}})); // misconduct: runs alongside
    const auto rows = penaltyRows(box);
    REQUIRE(rows.size() == 4);
    CHECK(rows[0].id == 1);
    CHECK(rows[1].id == 2);
    CHECK(rows[2].id == 4);
    CHECK(rows[3].id == 3);
    CHECK_FALSE(rows[0].queued);
    CHECK_FALSE(rows[1].queued);
    CHECK_FALSE(rows[2].queued);
    CHECK(rows[3].queued);
    CHECK(rows[0].player == "24");
    CHECK(rows[0].time == "2:00");
    CHECK(rows[0].phase2.empty());
    CHECK(rows[1].phase2 == "+2:00");
    CHECK(rows[2].time == "10:00");
}

TEST_CASE("an empty box has no rows") {
    PenaltyBox box;
    CHECK(penaltyRows(box).empty());
}
