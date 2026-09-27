// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Format.hpp"

using namespace sb;

TEST_CASE("formatClock pads minutes on request") {
    CHECK(formatClock(minutes(9), Direction::Up, true, false) == "09:00");
    CHECK(formatClock(minutes(9), Direction::Up, false, false) == "9:00");
    CHECK(formatClock(minutes(20), Direction::Down, true, false) == "20:00");
    CHECK(formatClock(minutes(120), Direction::Up, true, false) == "120:00");
}

TEST_CASE("formatClock rounds a countdown up and a count-up down") {
    CHECK(formatClock(4, Direction::Down, true, false) == "00:01");
    CHECK(formatClock(0, Direction::Down, true, false) == "00:00");
    CHECK(formatClock(minutes(20) - 1, Direction::Down, true, false) == "20:00");
    CHECK(formatClock(minutes(20) - 10, Direction::Down, true, false) == "19:59");
    CHECK(formatClock(19, Direction::Up, true, false) == "00:01");
}

TEST_CASE("formatClock shows tenths in the last minute of a countdown when enabled") {
    CHECK(formatClock(599, Direction::Down, true, true) == "59.9");
    CHECK(formatClock(95, Direction::Down, true, true) == "9.5");
    CHECK(formatClock(0, Direction::Down, true, true) == "0.0");
    CHECK(formatClock(600, Direction::Down, true, true) == "01:00");
    CHECK(formatClock(599, Direction::Up, true, true) == "00:59");
    CHECK(formatClock(-3, Direction::Down, true, false) == "00:00");
}

TEST_CASE("formatDuration for cumulative play time") {
    CHECK(formatDuration(minutes(25), TimeFormat::MinutesSeconds) == "25:00");
    CHECK(formatDuration(minutes(125) + seconds(7) + 9, TimeFormat::MinutesSeconds) == "125:07");
    CHECK(formatDuration(minutes(125) + seconds(7), TimeFormat::HoursMinutesSeconds) == "2:05:07");
    CHECK(formatDuration(seconds(5), TimeFormat::HoursMinutesSeconds) == "0:00:05");
}

TEST_CASE("formatPenaltyTime rounds up and does not pad minutes") {
    CHECK(formatPenaltyTime(minutes(2)) == "2:00");
    CHECK(formatPenaltyTime(minutes(2) - 1) == "2:00");
    CHECK(formatPenaltyTime(seconds(59)) == "0:59");
    CHECK(formatPenaltyTime(minutes(10)) == "10:00");
}

TEST_CASE("formatStoppage") {
    CHECK(formatStoppage(0) == "");
    CHECK(formatStoppage(-5) == "");
    CHECK(formatStoppage(seconds(30) + 9) == "+0:30");
    CHECK(formatStoppage(minutes(3) + seconds(2)) == "+3:02");
}

TEST_CASE("applyTemplate replaces known placeholders only") {
    const TemplateVars vars{{"player", "12"}, {"time", "2:00"}, {"phase2", ""}};
    CHECK(applyTemplate("{player} {time}", vars) == "12 2:00");
    CHECK(applyTemplate("#{player} ({time}){phase2}", vars) == "#12 (2:00)");
    CHECK(applyTemplate("{unknown} {time", vars) == "{unknown} {time");
    CHECK(applyTemplate("", vars) == "");
}
