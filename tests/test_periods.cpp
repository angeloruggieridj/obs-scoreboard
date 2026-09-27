// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Periods.hpp"

using namespace sb;

namespace {
PeriodConfig soccer() {
    PeriodConfig c;
    c.regulationPeriods = 2;
    c.regulationDuration = minutes(45);
    c.overtimePeriods = 2;
    c.overtimeDuration = minutes(15);
    c.continuousDisplay = true;
    return c;
}
} // namespace

TEST_CASE("default labels are numbers then OT, OT2, ...") {
    PeriodConfig c;
    c.regulationPeriods = 4;
    c.regulationDuration = minutes(10);
    c.overtimePeriods = kUnlimited;
    c.overtimeDuration = minutes(5);
    Periods p;
    p.configure(c);
    CHECK(p.index() == 1);
    CHECK(p.label() == "1");
    for (int i = 0; i < 4; ++i) CHECK(p.next());
    CHECK(p.isOvertime());
    CHECK(p.overtimeNumber() == 1);
    CHECK(p.label() == "OT");
    CHECK(p.duration() == minutes(5));
    CHECK(p.next());
    CHECK(p.label() == "OT2");
    CHECK(p.hasNext()); // unlimited overtime
}

TEST_CASE("custom labels, with fallback when the list is short") {
    PeriodConfig c = soccer();
    c.regulationLabels = {"1T", "2T"};
    c.overtimeLabels = {"1TS"};
    Periods p;
    p.configure(c);
    CHECK(p.label() == "1T");
    p.next();
    CHECK(p.label() == "2T");
    p.next();
    CHECK(p.label() == "1TS");
    p.next();
    CHECK(p.label() == "OT2");
}

TEST_CASE("bounds on next and prev") {
    Periods p;
    p.configure(soccer());
    CHECK_FALSE(p.hasPrev());
    CHECK_FALSE(p.prev());
    CHECK(p.setIndex(4));
    CHECK_FALSE(p.hasNext());
    CHECK_FALSE(p.next());
    CHECK(p.index() == 4);
    CHECK(p.prev());
    CHECK(p.index() == 3);
    CHECK_FALSE(p.setIndex(5));
    CHECK_FALSE(p.setIndex(0));
    CHECK(p.index() == 3);
}

TEST_CASE("continuous display offsets count-up periods") {
    Periods p;
    p.configure(soccer());
    CHECK(p.displayOffset() == 0);
    p.next();
    CHECK(p.displayOffset() == minutes(45));
    p.next();
    CHECK(p.displayOffset() == minutes(90));
    p.next();
    CHECK(p.displayOffset() == minutes(105));
    CHECK(p.durationOf(1) == minutes(45));
    CHECK(p.durationOf(4) == minutes(15));
}

TEST_CASE("no offset without continuous display, and at least one period") {
    PeriodConfig c = soccer();
    c.continuousDisplay = false;
    c.regulationPeriods = 0;
    Periods p;
    p.configure(c);
    CHECK(p.config().regulationPeriods == 1);
    p.next();
    CHECK(p.displayOffset() == 0);
}
