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

// One team's timed penalties. At most kMaxActive run at once, in insertion order; the rest wait.
// Penalties are addressed by id, never by position: the list moves while the clock runs.
class PenaltyBox {
public:
    static constexpr std::size_t kMaxPenalties = 8;
    static constexpr std::size_t kMaxActive = 2;

    bool add(PenaltyId id, std::string player, std::vector<PenaltyPhase> phases);
    // Sets the current phase's remaining time (clamped to its duration); <= 0 ends the phase.
    bool edit(PenaltyId id, Tenths remaining);
    // Ends the current phase: the next phase starts, or the penalty is removed.
    bool cancel(PenaltyId id);
    // Game time passed; expired phases hand their leftover time to what starts next.
    void elapse(Tenths t);
    // Game time given back (clock arrows): running penalties only, capped at the phase duration.
    void restore(Tenths t);

    std::vector<const Penalty*> active() const;
    const std::vector<Penalty>& all() const { return list_; }
    const Penalty* find(PenaltyId id) const;
    int strengthReduction() const;
    bool full() const { return list_.size() >= kMaxPenalties; }
    void restoreFrom(std::vector<Penalty> list) { list_ = std::move(list); }

private:
    std::size_t activeCount() const {
        return list_.size() < kMaxActive ? list_.size() : kMaxActive;
    }
    std::size_t indexOf(PenaltyId id) const;
    void endPhase(std::size_t index);

    std::vector<Penalty> list_;
};

} // namespace sb
