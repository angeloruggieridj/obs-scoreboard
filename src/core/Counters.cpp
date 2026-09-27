// SPDX-License-Identifier: GPL-2.0-or-later
#include "Counters.hpp"

#include <algorithm>
#include <utility>

namespace sb {
namespace {
Counters::Pair clampPair(Counters::Pair p) {
    return {std::max(0, p[0]), std::max(0, p[1])};
}
} // namespace

void Counters::setPeriod(int periodIndex, int regulationPeriods) {
    bucket_ = reset_ == FoulReset::Never ? 0 : std::min(periodIndex, regulationPeriods);
}

int Counters::get(Team team, Stat stat) const {
    const std::size_t i = teamIndex(team);
    switch (stat) {
    case Stat::Score:
        return score_[i];
    case Stat::Shots:
        return shots_[i];
    case Stat::Fouls2:
        return fouls2_[i];
    case Stat::Fouls: {
        const auto it = fouls_.find(bucket_);
        return it == fouls_.end() ? 0 : it->second[i];
    }
    }
    return 0;
}

int& Counters::slot(Team team, Stat stat) {
    const std::size_t i = teamIndex(team);
    switch (stat) {
    case Stat::Score:
        return score_[i];
    case Stat::Shots:
        return shots_[i];
    case Stat::Fouls2:
        return fouls2_[i];
    case Stat::Fouls:
        break;
    }
    return fouls_[bucket_][i];
}

int Counters::add(Team team, Stat stat, int delta) {
    int& value = slot(team, stat);
    const int before = value;
    value = std::max(0, value + delta);
    return value - before;
}

void Counters::restore(Pair score, Pair shots, Pair fouls2, std::map<int, Pair> fouls) {
    score_ = clampPair(score);
    shots_ = clampPair(shots);
    fouls2_ = clampPair(fouls2);
    fouls_.clear();
    for (const auto& [bucket, pair] : fouls) fouls_[bucket] = clampPair(pair);
}

} // namespace sb
