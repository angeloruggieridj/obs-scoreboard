// SPDX-License-Identifier: GPL-2.0-or-later
#include "Clock.hpp"

#include <algorithm>
#include <limits>

namespace sb {

void Clock::configure(Direction dir, Tenths duration, bool stopAtLimit) {
    dir_ = dir;
    duration_ = std::max<Tenths>(0, duration);
    stopAtLimit_ = stopAtLimit;
    running_ = false;
    resetToStart();
}

void Clock::resetToStart() {
    if (running_) return;
    value_ = dir_ == Direction::Down ? duration_ : 0;
    anchorValue_ = value_;
}

void Clock::restore(Tenths value) {
    if (running_) return;
    value_ = std::max<Tenths>(0, value);
    anchorValue_ = value_;
}

bool Clock::atStart() const {
    return value_ == (dir_ == Direction::Down ? duration_ : 0);
}

bool Clock::atLimit() const {
    if (dir_ == Direction::Down) return value_ == 0;
    return duration_ > 0 && value_ >= duration_;
}

void Clock::start(Micros now) {
    if (running_) return;
    if (limitStops() && atLimit()) return;
    running_ = true;
    anchorValue_ = value_;
    anchorTime_ = now;
}

Tenths Clock::computeAt(Micros now) const {
    const Micros dt = std::max<Micros>(0, now - anchorTime_);
    const Tenths ticks = dt / kMicrosPerTenth;
    Tenths v = dir_ == Direction::Down ? anchorValue_ - ticks : anchorValue_ + ticks;
    if (dir_ == Direction::Down) {
        v = std::max<Tenths>(0, v);
    } else if (stopAtLimit_ && duration_ > 0) {
        v = std::min(v, duration_);
    }
    return v;
}

Clock::Tick Clock::advance(Micros now) {
    Tick tick;
    if (!running_) return tick;
    const Tenths v = computeAt(now);
    tick.elapsed = v > value_ ? v - value_ : value_ - v;
    value_ = v;
    if (limitStops() && atLimit()) {
        running_ = false;
        anchorValue_ = value_;
        tick.reachedLimit = true;
    }
    return tick;
}

void Clock::stop(Micros now) {
    if (!running_) return;
    advance(now);
    running_ = false;
    anchorValue_ = value_;
}

Tenths Clock::upperBoundForAdjust() const {
    if (duration_ == 0) return std::numeric_limits<Tenths>::max();
    return std::max(duration_, value_);
}

Tenths Clock::adjust(Tenths delta, Micros now) {
    if (running_) advance(now);
    const Tenths target = std::clamp<Tenths>(value_ + delta, 0, upperBoundForAdjust());
    const Tenths applied = target - value_;
    value_ = target;
    if (running_) {
        // Shift the anchor instead of re-anchoring at `now`: re-anchoring would drop the
        // fraction of a tenth already elapsed, and every arrow press would add drift.
        anchorValue_ += applied;
        if (limitStops() && atLimit()) {
            running_ = false;
            anchorValue_ = value_;
        }
    } else {
        anchorValue_ = value_;
    }
    return applied;
}

void Clock::set(Tenths value, Micros now) {
    if (running_) advance(now);
    adjust(value - value_, now);
}

} // namespace sb
