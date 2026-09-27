// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <map>

#include "Types.hpp"

namespace sb {

enum class Stat { Score, Shots, Fouls, Fouls2 };

// Score, shots and the two foul counters for both teams. Fouls are kept per "bucket": with
// FoulReset::EachRegulationPeriod every regulation period has its own bucket and overtime
// shares the last one, so going back a period shows that period's fouls again.
class Counters {
public:
    using Pair = std::array<int, 2>;

    void setFoulReset(FoulReset reset) { reset_ = reset; }
    void setPeriod(int periodIndex, int regulationPeriods);
    int get(Team team, Stat stat) const;
    // Adds delta, never going below zero; returns the change actually applied.
    int add(Team team, Stat stat, int delta);
    const std::map<int, Pair>& foulBuckets() const { return fouls_; }
    void restore(Pair score, Pair shots, Pair fouls2, std::map<int, Pair> fouls);

private:
    int& slot(Team team, Stat stat);

    FoulReset reset_ = FoulReset::Never;
    Pair score_{};
    Pair shots_{};
    Pair fouls2_{};
    std::map<int, Pair> fouls_;
    int bucket_ = 0;
};

} // namespace sb
