// SPDX-License-Identifier: GPL-2.0-or-later
#include "PenaltyBox.hpp"

#include <algorithm>
#include <utility>

namespace sb {
namespace {
constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);
} // namespace

bool PenaltyBox::add(PenaltyId id, std::string player, std::vector<PenaltyPhase> phases) {
    if (full() || phases.empty() || indexOf(id) != kNotFound) return false;
    for (const PenaltyPhase& p : phases)
        if (p.duration <= 0) return false;
    Penalty penalty;
    penalty.id = id;
    penalty.player = std::move(player);
    penalty.remaining = phases.front().duration;
    penalty.phases = std::move(phases);
    list_.push_back(std::move(penalty));
    return true;
}

std::size_t PenaltyBox::indexOf(PenaltyId id) const {
    for (std::size_t i = 0; i < list_.size(); ++i)
        if (list_[i].id == id) return i;
    return kNotFound;
}

const Penalty* PenaltyBox::find(PenaltyId id) const {
    const std::size_t i = indexOf(id);
    return i == kNotFound ? nullptr : &list_[i];
}

void PenaltyBox::endPhase(std::size_t index) {
    Penalty& p = list_[index];
    if (p.hasNextPhase()) {
        ++p.phase;
        p.remaining = p.current().duration;
    } else {
        list_.erase(list_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

bool PenaltyBox::edit(PenaltyId id, Tenths remaining) {
    const std::size_t i = indexOf(id);
    if (i == kNotFound) return false;
    if (remaining <= 0) {
        endPhase(i);
        return true;
    }
    list_[i].remaining = std::min(remaining, list_[i].current().duration);
    return true;
}

bool PenaltyBox::cancel(PenaltyId id) {
    const std::size_t i = indexOf(id);
    if (i == kNotFound) return false;
    endPhase(i);
    return true;
}

void PenaltyBox::elapse(Tenths t) {
    while (t > 0 && !list_.empty()) {
        const std::size_t n = activeCount();
        Tenths step = t;
        for (std::size_t i = 0; i < n; ++i) step = std::min(step, list_[i].remaining);
        for (std::size_t i = 0; i < n; ++i) list_[i].remaining -= step;
        t -= step;
        // Backwards, so erasing an entry keeps the earlier indices valid.
        for (std::size_t i = n; i-- > 0;)
            if (list_[i].remaining <= 0) endPhase(i);
    }
}

void PenaltyBox::restore(Tenths t) {
    if (t <= 0) return;
    for (std::size_t i = 0; i < activeCount(); ++i)
        list_[i].remaining = std::min(list_[i].remaining + t, list_[i].current().duration);
}

std::vector<const Penalty*> PenaltyBox::active() const {
    std::vector<const Penalty*> out;
    for (std::size_t i = 0; i < activeCount(); ++i) out.push_back(&list_[i]);
    return out;
}

int PenaltyBox::strengthReduction() const {
    int n = 0;
    for (std::size_t i = 0; i < activeCount(); ++i)
        if (list_[i].current().reducesStrength) ++n;
    return n;
}

} // namespace sb
