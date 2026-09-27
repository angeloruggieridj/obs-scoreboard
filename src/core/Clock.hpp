// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Types.hpp"

namespace sb {

// Game clock at tenth-of-a-second resolution. The value is always derived from the monotonic
// time elapsed since the last start (anchor), never by summing timer ticks, so irregular ticks
// cannot make it drift.
class Clock {
public:
    struct Tick {
        Tenths elapsed = 0;        // game time that passed since the previous advance
        bool reachedLimit = false; // true on the advance that stopped the clock at its limit
    };

    // duration 0 means "no limit" (count-up only). stopAtLimit false lets a count-up clock run
    // past its duration (stoppage time); a countdown always stops at zero.
    void configure(Direction dir, Tenths duration, bool stopAtLimit);
    void resetToStart();
    // Sets the value directly, without the arrows' limits (resuming a saved match). Ignored while
    // running.
    void restore(Tenths value);
    void start(Micros now);
    void stop(Micros now);
    Tick advance(Micros now);
    // Arrow keys: moves the value by delta within [0, duration] and returns what was applied.
    Tenths adjust(Tenths delta, Micros now);
    void set(Tenths value, Micros now);

    bool running() const { return running_; }
    Tenths value() const { return value_; }
    Tenths duration() const { return duration_; }
    Direction direction() const { return dir_; }
    bool atStart() const;
    bool atLimit() const;

private:
    bool limitStops() const { return dir_ == Direction::Down || stopAtLimit_; }
    Tenths computeAt(Micros now) const;
    Tenths upperBoundForAdjust() const;

    Direction dir_ = Direction::Up;
    Tenths duration_ = 0;
    bool stopAtLimit_ = true;
    bool running_ = false;
    Tenths value_ = 0;
    Tenths anchorValue_ = 0;
    Micros anchorTime_ = 0;
};

} // namespace sb
