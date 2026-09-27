// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "PenaltyBox.hpp"

using namespace sb;

namespace {
// Not named minor(): that collides with a BSD/glibc macro on macOS/Linux.
std::vector<PenaltyPhase> minorPenalty() {
    return {{minutes(2), true}};
}
} // namespace

TEST_CASE("add rejects a full box, empty or zero phases, and duplicate ids") {
    PenaltyBox box;
    for (PenaltyId id = 1; id <= 8; ++id) CHECK(box.add(id, std::to_string(id), minorPenalty()));
    CHECK(box.full());
    CHECK_FALSE(box.add(9, "9", minorPenalty()));
    PenaltyBox other;
    CHECK_FALSE(other.add(1, "4", {}));
    CHECK_FALSE(other.add(1, "4", {{0, true}}));
    CHECK(other.add(1, "4", minorPenalty()));
    CHECK_FALSE(other.add(1, "5", minorPenalty()));
}

TEST_CASE("only two run; the next starts when one expires, carrying the remainder") {
    PenaltyBox box;
    box.add(1, "A", minorPenalty());
    box.add(2, "B", {{minutes(5), true}});
    box.add(3, "C", minorPenalty());
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
    box.add(3, "C", minorPenalty());
    box.elapse(minutes(1));
    REQUIRE(box.find(1) == nullptr);
    CHECK(box.edit(3, seconds(50)));
    CHECK(box.find(3)->remaining == seconds(50));
    CHECK(box.find(2)->remaining == minutes(2));
    CHECK_FALSE(box.edit(1, seconds(10)));
}

TEST_CASE("edit clamps to the phase duration and zero ends the phase") {
    PenaltyBox box;
    box.add(1, "A", minorPenalty());
    CHECK(box.edit(1, minutes(9)));
    CHECK(box.find(1)->remaining == minutes(2));
    CHECK(box.edit(1, 0));
    CHECK(box.find(1) == nullptr);
}

TEST_CASE("restore gives time back to running penalties only, up to the phase duration") {
    PenaltyBox box;
    box.add(1, "A", minorPenalty());
    box.add(2, "B", minorPenalty());
    box.add(3, "C", minorPenalty());
    box.elapse(seconds(30));
    box.restore(seconds(10));
    CHECK(box.find(1)->remaining == minutes(2) - seconds(20));
    CHECK(box.find(3)->remaining == minutes(2));
    box.restore(minutes(5));
    CHECK(box.find(1)->remaining == minutes(2));
    box.restore(-seconds(5));
    CHECK(box.find(1)->remaining == minutes(2));
}

TEST_CASE("a misconduct phase takes no slot: two reducing penalties still run beside it") {
    PenaltyBox box;
    box.add(1, "A", {{minutes(2), true}, {minutes(10), false}});
    box.elapse(minutes(2)); // A's minor ends; its misconduct phase (non-reducing) starts running
    REQUIRE(box.find(1)->phase == 1);
    CHECK_FALSE(box.find(1)->current().reducesStrength);

    box.add(2, "C", minorPenalty());
    box.add(3, "D", minorPenalty());
    CHECK(box.running().size() == 3);    // A's misconduct, plus both C and D
    CHECK(box.strengthReduction() == 2); // only C and D reduce strength
    REQUIRE(box.active().size() ==
            2); // the two on-air slots: the reducing ones, not the misconduct
    CHECK(box.active()[0]->id == 2);
    CHECK(box.active()[1]->id == 3);

    box.elapse(minutes(1));
    CHECK(box.find(2)->remaining == minutes(1)); // C ran
    CHECK(box.find(3)->remaining ==
          minutes(1)); // D ran too: not stuck queued behind the misconduct
}

TEST_CASE("restoreFrom replaces the whole list") {
    PenaltyBox box;
    Penalty p;
    p.id = 4;
    p.player = "9";
    p.phases = minorPenalty();
    p.remaining = seconds(40);
    box.restoreFrom({p});
    CHECK(box.find(4)->remaining == seconds(40));
    CHECK(box.find(4)->current().duration == minutes(2));
    CHECK_FALSE(box.find(4)->hasNextPhase());
}

TEST_CASE("two penalties expiring in the same instant both end, and restoring undoes both") {
    PenaltyBox box;
    box.add(1, "A", minorPenalty());
    box.add(2, "B", minorPenalty());
    box.elapse(minutes(2));
    CHECK(box.find(1) == nullptr);
    CHECK(box.find(2) == nullptr);
    CHECK(box.all().empty());

    box.restore(minutes(2));
    REQUIRE(box.find(1) != nullptr);
    REQUIRE(box.find(2) != nullptr);
    CHECK(box.find(1)->remaining == minutes(2));
    CHECK(box.find(2)->remaining == minutes(2));
}

TEST_CASE("a chain of expiries promotes queued penalties one after another in a single advance") {
    PenaltyBox box;
    box.add(1, "A", {{seconds(10), true}});
    box.add(2, "B", {{seconds(40), true}});
    box.add(3, "C", {{seconds(10), true}});
    box.add(4, "D", {{seconds(10), true}});
    box.elapse(seconds(40));
    CHECK(box.all().empty());

    box.restore(seconds(40));
    REQUIRE(box.all().size() == 4);
    CHECK(box.find(1)->remaining == seconds(10));
    CHECK(box.find(2)->remaining == seconds(40));
    CHECK(box.find(3)->remaining == seconds(10));
    CHECK(box.find(4)->remaining == seconds(10));
}

TEST_CASE("cancelling a compound penalty in its first phase advances it and keeps its slot") {
    PenaltyBox box;
    box.add(1, "A", {{minutes(2), true}, {minutes(2), true}});
    box.add(2, "X", minorPenalty());
    box.add(3, "Y", minorPenalty());
    CHECK(box.cancel(1));
    CHECK(box.find(1)->phase == 1);
    CHECK(box.find(1)->remaining == minutes(2));
    CHECK(box.find(3)->remaining == minutes(2)); // Y still queued, untouched

    box.elapse(seconds(30));
    CHECK(box.find(1)->remaining == minutes(2) - seconds(30));
    CHECK(box.find(2)->remaining == minutes(2) - seconds(30));
    CHECK(box.find(3)->remaining == minutes(2)); // A kept its slot instead of freeing it
}

TEST_CASE(
    "cancelling a compound penalty in its last phase frees its slot for the next queued one") {
    PenaltyBox box;
    box.add(1, "A", {{minutes(2), true}, {minutes(2), true}});
    box.add(2, "X", minorPenalty());
    box.add(3, "Y", minorPenalty());
    CHECK(box.cancel(1)); // -> phase 1, still running
    CHECK(box.cancel(1)); // last phase ends -> A removed, Y takes its slot
    CHECK(box.find(1) == nullptr);

    box.elapse(seconds(20));
    CHECK(box.find(2)->remaining == minutes(2) - seconds(20));
    CHECK(box.find(3)->remaining == minutes(2) - seconds(20)); // Y now running
}

TEST_CASE("edit and cancel work on a penalty that has not started running yet") {
    PenaltyBox box;
    box.add(1, "A", minorPenalty());
    box.add(2, "B", minorPenalty());
    box.add(3, "C", minorPenalty()); // queued behind A and B
    CHECK(box.edit(3, seconds(90)));
    CHECK(box.find(3)->remaining == seconds(90));
    CHECK(box.cancel(3));
    CHECK(box.find(3) == nullptr);
}

TEST_CASE("restore can cross back over a compound penalty's phase boundary (F1 repro)") {
    PenaltyBox box;
    box.add(1, "A", {{minutes(2), true}, {minutes(2), true}});
    box.elapse(minutes(2) + seconds(10));
    REQUIRE(box.find(1)->phase == 1);
    CHECK(box.find(1)->remaining == minutes(2) - seconds(10));

    box.restore(seconds(30));
    CHECK(box.find(1)->phase == 0);
    CHECK(box.find(1)->remaining == seconds(20));
}

TEST_CASE("elapse(x); restore(y) matches a fresh box that only elapsed x - y (F1 repro)") {
    PenaltyBox reference;
    reference.add(1, "A", minorPenalty());
    reference.add(2, "B", {{minutes(5), true}});
    reference.add(3, "C", minorPenalty());
    reference.elapse(minutes(2)); // x - y = 130 s - 10 s = 120 s

    PenaltyBox box;
    box.add(1, "A", minorPenalty());
    box.add(2, "B", {{minutes(5), true}});
    box.add(3, "C", minorPenalty());
    box.elapse(minutes(2) + seconds(10)); // x = 130 s
    box.restore(seconds(10));             // y = 10 s

    CHECK(box.find(1) == nullptr);
    CHECK(reference.find(1) == nullptr);
    REQUIRE(box.find(2) != nullptr);
    REQUIRE(reference.find(2) != nullptr);
    CHECK(box.find(2)->remaining == reference.find(2)->remaining);
    REQUIRE(box.find(3) != nullptr);
    REQUIRE(reference.find(3) != nullptr);
    CHECK(box.find(3)->remaining == reference.find(3)->remaining);
}
