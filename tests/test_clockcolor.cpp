// SPDX-License-Identifier: GPL-2.0-or-later
#include <doctest/doctest.h>

#include "ClockColor.hpp"
#include "PanelModel.hpp"
#include "PluginState.hpp"

using namespace sb;

namespace {

ClockColorSettings byTime(bool blend) {
    ClockColorSettings s;
    s.mode = ClockColorMode::ByTime;
    s.base = "#FFFFFF";
    s.blend = blend;
    s.rules = {{30, "#FFFFFF"}, {0, "#FF0000"}};
    return s;
}

} // namespace

TEST_CASE("the theme mode leaves the digits alone") {
    ClockColorSettings s;
    s.base = "#00FF00";
    CHECK(clockColorAt(s, seconds(5)).empty());
}

TEST_CASE("a single color is the same whatever the time") {
    ClockColorSettings s;
    s.mode = ClockColorMode::Single;
    s.base = "#00ff00";
    CHECK(clockColorAt(s, seconds(5)) == "#00FF00");
    CHECK(clockColorAt(s, minutes(10)) == "#00FF00");
    s.base = "not a color";
    CHECK(clockColorAt(s, seconds(5)).empty());
}

TEST_CASE("by time without blending: base above the rules, a step at each rule") {
    ClockColorSettings s;
    s.mode = ClockColorMode::ByTime;
    s.base = "#FFFFFF";
    s.rules = {{10, "#FF0000"}, {60, "#FFAA00"}}; // out of order on purpose
    CHECK(clockColorAt(s, minutes(5)) == "#FFFFFF");
    CHECK(clockColorAt(s, seconds(61)) == "#FFFFFF");
    CHECK(clockColorAt(s, seconds(60)) == "#FFAA00");
    CHECK(clockColorAt(s, seconds(11)) == "#FFAA00");
    CHECK(clockColorAt(s, seconds(10)) == "#FF0000");
    CHECK(clockColorAt(s, 0) == "#FF0000");
}

TEST_CASE("by time with blending fades from one rule to the next") {
    const ClockColorSettings s = byTime(true);
    CHECK(clockColorAt(s, seconds(45)) == "#FFFFFF"); // before the first rule: the base
    CHECK(clockColorAt(s, seconds(30)) == "#FFFFFF"); // the first rule starts here
    CHECK(clockColorAt(s, seconds(15)) == "#FF8080"); // halfway to red
    CHECK(clockColorAt(s, 0) == "#FF0000");           // reached at zero
}

TEST_CASE("blending keeps the last rule's color and ignores bad rules") {
    ClockColorSettings s = byTime(true);
    s.rules.push_back({5, "oops"});     // invalid color: ignored
    s.rules.push_back({-3, "#00FF00"}); // negative: ignored
    CHECK(clockColorAt(s, 0) == "#FF0000");
    s.rules = {{20, "#FFAA00"}};
    CHECK(clockColorAt(s, seconds(3)) == "#FFAA00"); // one rule: nothing to fade into
}

TEST_CASE("a period without a length never triggers a rule") {
    const ClockColorSettings s = byTime(false);
    CHECK(clockColorAt(s, -1) == "#FFFFFF");
}

TEST_CASE("time left and its share of the period") {
    CHECK(timeLeft(minutes(5), minutes(20), Direction::Down) == minutes(5));
    CHECK(timeLeft(minutes(5), minutes(20), Direction::Up) == minutes(15));
    CHECK(timeLeft(minutes(25), minutes(20), Direction::Up) == 0); // past the length
    CHECK(timeLeft(minutes(5), 0, Direction::Up) == -1);
    CHECK(shareLeft(minutes(5), minutes(20), Direction::Down) == doctest::Approx(0.25));
    CHECK(shareLeft(minutes(5), minutes(20), Direction::Up) == doctest::Approx(0.75));
    CHECK(shareLeft(minutes(5), 0, Direction::Down) == doctest::Approx(-1.0));
}

TEST_CASE("the new preferences default sensibly and round-trip") {
    const GlobalPrefs defaults;
    CHECK(defaults.clockColor.mode == ClockColorMode::Theme);
    CHECK(defaults.periodBar == PeriodBarMode::Remaining);
    CHECK(defaults.homeColor.empty());

    GlobalPrefs p;
    p.homeColor = "#112233";
    p.awayColor = "#445566";
    p.periodBar = PeriodBarMode::Consumed;
    p.periodBarColor = "#00FF00";
    p.clockColor = byTime(true);
    const auto out = prefsFromJson(prefsToJson(p));
    REQUIRE(out.has_value());
    CHECK(out->homeColor == "#112233");
    CHECK(out->awayColor == "#445566");
    CHECK(out->periodBar == PeriodBarMode::Consumed);
    CHECK(out->periodBarColor == "#00FF00");
    CHECK(out->clockColor == p.clockColor);
}

TEST_CASE("broken new preferences fall back without failing the file") {
    const auto p = prefsFromJson(
        R"({"version":3,"homeColor":5,"periodBar":"sideways","clockColor":{"mode":"byTime",)"
        R"("base":"zzz","rules":[{"secondsLeft":"x","color":"#FF0000"},)"
        R"({"secondsLeft":9999,"color":"#FF0000"},{"secondsLeft":10,"color":"#FF0000"},)"
        R"(1,2,3,4,5]}})");
    REQUIRE(p.has_value());
    CHECK(p->homeColor.empty());
    CHECK(p->periodBar == PeriodBarMode::Remaining);
    CHECK(p->clockColor.mode == ClockColorMode::ByTime);
    CHECK(p->clockColor.base.empty());
    REQUIRE(p->clockColor.rules.size() == 1);
    CHECK(p->clockColor.rules[0].secondsLeft == 10);
}
