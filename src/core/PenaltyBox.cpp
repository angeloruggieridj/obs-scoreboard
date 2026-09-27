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
    log_.clear();
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
    log_.clear();
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
    log_.clear();
    endPhase(i);
    return true;
}

std::vector<std::size_t> PenaltyBox::runningIndices() const {
    std::vector<std::size_t> out;
    std::size_t reducingRunning = 0;
    for (std::size_t i = 0; i < list_.size(); ++i) {
        if (!list_[i].current().reducesStrength) {
            out.push_back(i);
        } else if (reducingRunning < kMaxActive) {
            out.push_back(i);
            ++reducingRunning;
        }
    }
    return out;
}

void PenaltyBox::elapse(Tenths t) {
    while (t > 0 && !list_.empty()) {
        const std::vector<std::size_t> runningIdx = runningIndices();
        if (runningIdx.empty()) break; // nothing can run; nothing more can happen

        Tenths step = t;
        for (std::size_t idx : runningIdx) step = std::min(step, list_[idx].remaining);
        for (std::size_t idx : runningIdx) list_[idx].remaining -= step;
        t -= step;

        StepEntry entry;
        entry.amount = step;
        entry.runningIds.reserve(runningIdx.size());
        for (std::size_t idx : runningIdx) entry.runningIds.push_back(list_[idx].id);

        // Descending index order: erasing a removed penalty shifts later indices, never earlier
        // ones still to be checked.
        for (auto it = runningIdx.rbegin(); it != runningIdx.rend(); ++it) {
            const std::size_t idx = *it;
            if (list_[idx].remaining > 0) continue;
            EndingRecord rec;
            rec.id = list_[idx].id;
            if (list_[idx].hasNextPhase()) {
                rec.removed = false;
                endPhase(idx);
            } else {
                rec.removed = true;
                rec.index = idx;
                rec.snapshot = list_[idx];
                endPhase(idx);
            }
            entry.endings.push_back(std::move(rec));
        }
        log_.push_back(std::move(entry));
    }
}

void PenaltyBox::reverseEndings(std::vector<EndingRecord>& endings) {
    // Reverse of the recording order: endings were appended from the highest running index down
    // to the lowest, so undoing them lowest-first keeps every recorded index valid at the moment
    // it is used to re-insert a removed penalty.
    for (auto it = endings.rbegin(); it != endings.rend(); ++it) {
        const EndingRecord& e = *it;
        if (e.removed) {
            list_.insert(list_.begin() + static_cast<std::ptrdiff_t>(e.index), e.snapshot);
        } else {
            const std::size_t idx = indexOf(e.id);
            if (idx != kNotFound) {
                --list_[idx].phase;
                list_[idx].remaining = 0;
            }
        }
    }
}

void PenaltyBox::restore(Tenths t) {
    if (t <= 0) return;
    while (t > 0 && !log_.empty()) {
        StepEntry& step = log_.back();
        const Tenths give = std::min(t, step.amount);
        if (!step.endings.empty()) {
            reverseEndings(step.endings);
            step.endings.clear();
        }
        for (PenaltyId id : step.runningIds) {
            const std::size_t idx = indexOf(id);
            if (idx != kNotFound)
                list_[idx].remaining =
                    std::min(list_[idx].remaining + give, list_[idx].current().duration);
        }
        step.amount -= give;
        t -= give;
        if (step.amount <= 0) log_.pop_back();
    }
    if (t <= 0) return;
    for (std::size_t idx : runningIndices())
        list_[idx].remaining = std::min(list_[idx].remaining + t, list_[idx].current().duration);
}

std::vector<const Penalty*> PenaltyBox::active() const {
    std::vector<const Penalty*> reducing;
    std::vector<const Penalty*> nonReducing;
    for (std::size_t idx : runningIndices()) {
        if (list_[idx].current().reducesStrength)
            reducing.push_back(&list_[idx]);
        else
            nonReducing.push_back(&list_[idx]);
    }
    std::vector<const Penalty*> out;
    for (const Penalty* p : reducing) {
        if (out.size() >= kMaxActive) break;
        out.push_back(p);
    }
    for (const Penalty* p : nonReducing) {
        if (out.size() >= kMaxActive) break;
        out.push_back(p);
    }
    return out;
}

std::vector<const Penalty*> PenaltyBox::running() const {
    std::vector<const Penalty*> out;
    for (std::size_t idx : runningIndices()) out.push_back(&list_[idx]);
    return out;
}

int PenaltyBox::strengthReduction() const {
    int n = 0;
    for (std::size_t idx : runningIndices())
        if (list_[idx].current().reducesStrength) ++n;
    return n;
}

} // namespace sb
