// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "PenaltyBox.hpp"

using namespace sb;

namespace {
std::vector<PenaltyPhase> minor() {
    return {{minutes(2), true}};
}
} // namespace

TEST_CASE("add rejects a full box, empty or zero phases, and duplicate ids") {
    PenaltyBox box;
    for (PenaltyId id = 1; id <= 8; ++id) CHECK(box.add(id, std::to_string(id), minor()));
    CHECK(box.full());
    CHECK_FALSE(box.add(9, "9", minor()));
    PenaltyBox other;
    CHECK_FALSE(other.add(1, "4", {}));
    CHECK_FALSE(other.add(1, "4", {{0, true}}));
    CHECK(other.add(1, "4", minor()));
    CHECK_FALSE(other.add(1, "5", minor()));
}

TEST_CASE("only two run; the next starts when one expires, carrying the remainder") {
    PenaltyBox box;
    box.add(1, "A", minor());
    box.add(2, "B", {{minutes(5), true}});
    box.add(3, "C", minor());
    box.elapse(minutes(2) + seconds(30));
    REQUIRE(box.all().size() == 2);
    CHECK(box.find(1) == nullptr);
    CHECK(box.find(2)->remaining == minutes(2) + seconds(30));
    CHECK(box.find(3)->remaining == minutes(1) + seconds(30));
    CHECK(box.active().size() == 2);
}

TEST_CASE("a compound penalty runs its second phase after the first ends or is cancelled") {
    PenaltyBox box;
    box.add(1, "7", {{minutes(2), true}, {minutes(2), true}});
    box.elapse(minutes(2) + seconds(10));
    REQUIRE(box.find(1) != nullptr);
    CHECK(box.find(1)->phase == 1);
    CHECK(box.find(1)->remaining == minutes(2) - seconds(10));

    PenaltyBox cancelled;
    cancelled.add(1, "7", {{minutes(2), true}, {minutes(10), false}});
    cancelled.elapse(seconds(30));
    CHECK(cancelled.cancel(1));
    CHECK(cancelled.find(1)->phase == 1);
    CHECK(cancelled.find(1)->remaining == minutes(10));
    CHECK(cancelled.cancel(1));
    CHECK(cancelled.find(1) == nullptr);
    CHECK_FALSE(cancelled.cancel(1));
}

TEST_CASE("edit targets the id even after the queue moved") {
    PenaltyBox box;
    box.add(1, "A", {{minutes(1), true}});
    box.add(2, "B", {{minutes(3), true}});
    box.add(3, "C", minor());
    box.elapse(minutes(1));
    REQUIRE(box.find(1) == nullptr);
    CHECK(box.edit(3, seconds(50)));
    CHECK(box.find(3)->remaining == seconds(50));
    CHECK(box.find(2)->remaining == minutes(2));
    CHECK_FALSE(box.edit(1, seconds(10)));
}

TEST_CASE("edit clamps to the phase duration and zero ends the phase") {
    PenaltyBox box;
    box.add(1, "A", minor());
    CHECK(box.edit(1, minutes(9)));
    CHECK(box.find(1)->remaining == minutes(2));
    CHECK(box.edit(1, 0));
    CHECK(box.find(1) == nullptr);
}

TEST_CASE("restore gives time back to running penalties only, up to the phase duration") {
    PenaltyBox box;
    box.add(1, "A", minor());
    box.add(2, "B", minor());
    box.add(3, "C", minor());
    box.elapse(seconds(30));
    box.restore(seconds(10));
    CHECK(box.find(1)->remaining == minutes(2) - seconds(20));
    CHECK(box.find(3)->remaining == minutes(2));
    box.restore(minutes(5));
    CHECK(box.find(1)->remaining == minutes(2));
    box.restore(-seconds(5));
    CHECK(box.find(1)->remaining == minutes(2));
}

TEST_CASE("strength reduction counts running phases that reduce strength") {
    PenaltyBox box;
    box.add(1, "A", minor());
    box.add(2, "B", {{minutes(10), false}});
    box.add(3, "C", minor());
    CHECK(box.strengthReduction() == 1);
    box.elapse(0);
    CHECK(box.active().size() == 2);
}

TEST_CASE("restoreFrom replaces the whole list") {
    PenaltyBox box;
    Penalty p;
    p.id = 4;
    p.player = "9";
    p.phases = minor();
    p.remaining = seconds(40);
    box.restoreFrom({p});
    CHECK(box.find(4)->remaining == seconds(40));
    CHECK(box.find(4)->current().duration == minutes(2));
    CHECK_FALSE(box.find(4)->hasNextPhase());
}
