// SPDX-License-Identifier: GPL-2.0-or-later
#include <limits>

#include "doctest/doctest.h"
#include "Clock.hpp"

using namespace sb;

namespace {
constexpr Micros ms(std::int64_t v) {
    return v * 1000;
}
constexpr Micros sec(std::int64_t v) {
    return v * 1000000;
}
} // namespace

TEST_CASE("a countdown starts at its duration and counts down") {
    Clock c;
    c.configure(Direction::Down, minutes(20), true);
    CHECK(c.value() == minutes(20));
    CHECK(c.atStart());
    c.start(0);
    const Clock::Tick t = c.advance(sec(1));
    CHECK(c.value() == minutes(20) - seconds(1));
    CHECK(t.elapsed == 10);
    CHECK_FALSE(t.reachedLimit);
}

TEST_CASE("sub-tenth ticks lose no time") {
    Clock c;
    c.configure(Direction::Up, minutes(45), true);
    c.start(0);
    for (Micros now = ms(30); now <= sec(3); now += ms(30)) c.advance(now);
    CHECK(c.value() == seconds(3));
}

TEST_CASE("irregular ticks over 45 minutes do not drift") {
    Clock c;
    c.configure(Direction::Up, minutes(45), true);
    c.start(0);
    const Micros steps[] = {ms(17), ms(61), ms(20), ms(33), ms(49)};
    Micros now = 0;
    std::size_t i = 0;
    while (now < sec(45 * 60)) {
        now += steps[i++ % 5];
        if (now > sec(45 * 60)) now = sec(45 * 60);
        c.advance(now);
    }
    CHECK(c.value() == minutes(45));
}

TEST_CASE("a countdown stops by itself at zero") {
    Clock c;
    c.configure(Direction::Down, seconds(5), true);
    c.start(0);
    const Clock::Tick t = c.advance(sec(6));
    CHECK(c.value() == 0);
    CHECK_FALSE(c.running());
    CHECK(t.reachedLimit);
    CHECK(t.elapsed == seconds(5));
    c.start(sec(7));
    CHECK_FALSE(c.running());
}

TEST_CASE("a count-up clock stops at its duration unless told to run past it") {
    Clock stops;
    stops.configure(Direction::Up, seconds(10), true);
    stops.start(0);
    CHECK(stops.advance(sec(12)).reachedLimit);
    CHECK(stops.value() == seconds(10));
    CHECK(stops.atLimit());

    Clock runs;
    runs.configure(Direction::Up, seconds(10), false);
    runs.start(0);
    CHECK_FALSE(runs.advance(sec(12)).reachedLimit);
    CHECK(runs.value() == seconds(12));
    CHECK(runs.running());
    CHECK(runs.atLimit());
}

TEST_CASE("a count-up clock without duration never stops") {
    Clock c;
    c.configure(Direction::Up, 0, true);
    c.start(0);
    CHECK_FALSE(c.advance(sec(10000)).reachedLimit);
    CHECK(c.value() == seconds(10000));
    CHECK_FALSE(c.atLimit());
    CHECK(c.adjust(-seconds(20000), sec(10000)) == -seconds(10000));
    CHECK(c.value() == 0);
}

TEST_CASE("arrows never pass the duration nor zero, in both directions") {
    Clock down;
    down.configure(Direction::Down, minutes(20), true);
    CHECK(down.adjust(seconds(1), 0) == 0);
    CHECK(down.value() == minutes(20));
    CHECK(down.adjust(-minutes(25), 0) == -minutes(20));
    CHECK(down.value() == 0);

    Clock up;
    up.configure(Direction::Up, minutes(20), true);
    CHECK(up.adjust(-seconds(1), 0) == 0);
    CHECK(up.adjust(minutes(25), 0) == minutes(20));
    CHECK(up.value() == minutes(20));
}

TEST_CASE("adjusting a running clock keeps the sub-tenth phase") {
    Clock c;
    c.configure(Direction::Down, minutes(20), true);
    c.start(0);
    c.advance(ms(1050));
    CHECK(c.value() == minutes(20) - seconds(1));
    CHECK(c.adjust(-minutes(1), ms(1050)) == -minutes(1));
    c.advance(sec(2));
    CHECK(c.value() == minutes(20) - minutes(1) - seconds(2));
}

TEST_CASE("an arrow that brings a running countdown to zero stops it") {
    Clock c;
    c.configure(Direction::Down, seconds(30), true);
    c.start(0);
    c.adjust(-seconds(40), ms(10));
    CHECK(c.value() == 0);
    CHECK_FALSE(c.running());
}

TEST_CASE("past the duration, arrows can go back but not further forward") {
    Clock c;
    c.configure(Direction::Up, minutes(45), false);
    c.set(minutes(44) + seconds(59), 0);
    c.start(0);
    c.advance(sec(2));
    CHECK(c.value() == minutes(45) + seconds(1));
    CHECK(c.adjust(seconds(1), sec(2)) == 0);
    CHECK(c.adjust(-seconds(1), sec(2)) == -seconds(1));
    CHECK(c.value() == minutes(45));
}

TEST_CASE("stop freezes the value and a restart continues from it") {
    Clock c;
    c.configure(Direction::Up, minutes(45), true);
    c.start(0);
    c.stop(sec(10));
    CHECK(c.value() == seconds(10));
    CHECK_FALSE(c.running());
    c.advance(sec(20));
    CHECK(c.value() == seconds(10));
    c.start(sec(30));
    c.advance(sec(35));
    CHECK(c.value() == seconds(15));
}

TEST_CASE("time going backwards is treated as no time") {
    Clock c;
    c.configure(Direction::Up, minutes(45), true);
    c.start(sec(10));
    c.advance(sec(5));
    CHECK(c.value() == 0);
}

TEST_CASE("reset and restore") {
    Clock c;
    c.configure(Direction::Down, minutes(20), true);
    c.set(minutes(5), 0);
    c.resetToStart();
    CHECK(c.value() == minutes(20));
    c.restore(minutes(50));
    CHECK(c.value() == minutes(50));
    c.restore(-5);
    CHECK(c.value() == 0);
    c.restore(minutes(3));
    c.start(0);
    c.restore(minutes(9)); // ignored while running
    CHECK(c.value() == minutes(3));
}

TEST_CASE("adjust saturates an extreme delta instead of overflowing value_ + delta") {
    Clock down;
    down.configure(Direction::Down, minutes(20), true); // value_ starts at 12000 (its duration)
    // 12000 + Tenths::max() overflows a raw std::int64_t addition; the saturating version must
    // clamp instead of invoking that undefined behavior.
    CHECK(down.adjust(std::numeric_limits<Tenths>::max(), 0) == 0); // already at the ceiling
    CHECK(down.value() == minutes(20));
    CHECK(down.adjust(std::numeric_limits<Tenths>::min(), 0) == -minutes(20));
    CHECK(down.value() == 0);

    Clock up;
    up.configure(Direction::Up, 0,
                 true); // unbounded count-up: upperBoundForAdjust() is Tenths::max()
    CHECK(up.adjust(std::numeric_limits<Tenths>::max(), 0) == std::numeric_limits<Tenths>::max());
    CHECK(up.value() == std::numeric_limits<Tenths>::max());
    // value_ is already Tenths::max(): adding it again is the same overflow shape as above.
    CHECK(up.adjust(std::numeric_limits<Tenths>::max(), 0) == 0);
    CHECK(up.value() == std::numeric_limits<Tenths>::max());
    CHECK(up.adjust(std::numeric_limits<Tenths>::min(), 0) == -std::numeric_limits<Tenths>::max());
    CHECK(up.value() == 0);
}

TEST_CASE("set saturates an extreme target instead of overflowing target - value_") {
    Clock c;
    c.configure(Direction::Down, minutes(20), true);
    c.set(std::numeric_limits<Tenths>::max(), 0);
    CHECK(c.value() == minutes(20));
    c.set(std::numeric_limits<Tenths>::min(), 0);
    CHECK(c.value() == 0);
}

TEST_CASE("reset while running is ignored, and works again once stopped") {
    Clock c;
    c.configure(Direction::Up, minutes(45), true);
    c.start(0);
    c.advance(sec(100));
    c.resetToStart(); // ignored while running: value and anchor stay as they were
    CHECK(c.value() == seconds(100));
    c.advance(sec(101));
    CHECK(c.value() == seconds(101));
    c.stop(sec(101));
    c.resetToStart(); // now applies
    CHECK(c.value() == 0);
}
