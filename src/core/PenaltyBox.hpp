// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Types.hpp"

namespace sb {

using PenaltyId = std::uint32_t;

struct PenaltyPhase {
    Tenths duration = 0;
    bool reducesStrength = true; // a 10' misconduct does not
};

struct Penalty {
    PenaltyId id = 0;
    std::string player;
    std::vector<PenaltyPhase> phases; // 2+2 is two phases
    std::size_t phase = 0;
    Tenths remaining = 0; // of the current phase

    const PenaltyPhase& current() const { return phases[phase]; }
    bool hasNextPhase() const { return phase + 1 < phases.size(); }
};

// One team's timed penalties. Penalties are addressed by id, never by position: the list moves
// while the clock runs.
//
// Running vs. queued: a phase that reduces strength competes for one of the kMaxActive "on-ice"
// slots, in list (insertion) order; a phase that does not reduce strength (e.g. a 10' misconduct)
// always runs alongside, taking no slot. So a box can have more than kMaxActive penalties running
// at once, but never more than kMaxActive of them reducing strength.
//
// elapse()/restore() undo log: elapse() advances an internal monotonic amount of game time and
// may trigger automatic transitions (a phase ending -> the next phase or removal starts; a
// queued penalty being promoted to running as a slot frees up). Every such step is pushed onto an
// in-memory undo log. restore() pops that log in reverse, so that for any x >= y >= 0,
// `elapse(x); restore(y);` leaves the box in exactly the state of a fresh `elapse(x - y)`. Once
// restore() has unwound the whole log, any further time is given back by simply topping up
// currently running penalties (capped at their phase duration) -- the same fallback used when
// nothing was ever elapsed. Any manual edit (add, edit, cancel, restoreFrom) clears the log: it
// only undoes automatic transitions since the last manual change. The log is pure runtime state,
// never serialized.
class PenaltyBox {
public:
    static constexpr std::size_t kMaxPenalties = 8;
    // Max number of strength-reducing phases running at once; non-reducing phases are unlimited.
    static constexpr std::size_t kMaxActive = 2;

    bool add(PenaltyId id, std::string player, std::vector<PenaltyPhase> phases);
    // Sets the current phase's remaining time (clamped to its duration); <= 0 ends the phase.
    bool edit(PenaltyId id, Tenths remaining);
    // Ends the current phase: the next phase starts, or the penalty is removed.
    bool cancel(PenaltyId id);
    // Game time passed; expired phases hand their leftover time to what starts next.
    void elapse(Tenths t);
    // Game time given back (clock arrows): undoes elapse()'s automatic transitions in reverse: see
    // the class comment. Beyond the logged span, tops up running penalties, capped at duration.
    void restore(Tenths t);

    // At most kMaxActive entries, for the two on-air slots: running strength-reducing penalties
    // first, then running non-reducing ones, each group in list order.
    std::vector<const Penalty*> active() const;
    // Every running penalty (strength-reducing within the cap, and every non-reducing one), in
    // list order.
    std::vector<const Penalty*> running() const;
    const std::vector<Penalty>& all() const { return list_; }
    const Penalty* find(PenaltyId id) const;
    // Number of running phases that reduce strength (0..kMaxActive).
    int strengthReduction() const;
    bool full() const { return list_.size() >= kMaxPenalties; }
    void restoreFrom(std::vector<Penalty> list) {
        list_ = std::move(list);
        log_.clear();
    }

private:
    // A snapshot of an automatic transition elapse() performed on one penalty, sufficient to
    // reverse it exactly.
    struct EndingRecord {
        PenaltyId id = 0;
        bool removed = false;  // false: advanced to the next phase; true: removed (last phase)
        std::size_t index = 0; // list_ position at removal time; meaningful only when removed
        Penalty snapshot;      // full pre-removal state; meaningful only when removed
    };
    // One elapse() step: a run of game time during which a fixed set of penalties (runningIds)
    // was running, ending with zero or more of them reaching the end of their current phase.
    struct StepEntry {
        Tenths amount = 0;
        std::vector<PenaltyId> runningIds;
        std::vector<EndingRecord> endings;
    };

    std::size_t indexOf(PenaltyId id) const;
    void endPhase(std::size_t index);
    // Indices of every currently running penalty, ascending, computed fresh from list_: every
    // non-reducing phase, plus the first kMaxActive reducing ones in list order.
    std::vector<std::size_t> runningIndices() const;
    // Reverses a step's endings in place, in reverse recording order (so re-inserting a removed
    // penalty always lands back at its original index).
    void reverseEndings(std::vector<EndingRecord>& endings);

    std::vector<Penalty> list_;
    std::vector<StepEntry> log_;
};

} // namespace sb
