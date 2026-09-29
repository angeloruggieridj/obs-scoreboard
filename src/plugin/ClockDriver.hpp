// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QTimer>

#include "Fields.hpp"
#include "MatchEngine.hpp"

// Owns the match and drives its clock from OBS's monotonic clock (spec §2: QTimer 20 ms,
// Qt::PreciseTimer, UI thread). Every input channel goes through apply(); every consumer
// listens to fieldsChanged, which fires only when a field's text actually changed.
class ClockDriver : public QObject {
    Q_OBJECT
public:
    explicit ClockDriver(sb::MatchSettings settings, QObject* parent = nullptr);

    // os_gettime_ns() in microseconds: the single time base shared by the engine, the text
    // sinks and the selftest's frame timestamps.
    static sb::Micros nowMicros();

    sb::CommandResult apply(const sb::Command& command);
    // Same as apply() at an explicit instant (the selftest needs to know it exactly).
    sb::CommandResult applyAt(const sb::Command& command, sb::Micros now);

    const sb::MatchEngine& engine() const { return engine_; }
    const sb::FieldValues& fields() const { return published_; }

    // OBS is exiting: stop the timer for good and reject later commands.
    void shutdown();

signals:
    void fieldsChanged(const sb::FieldValues& fields);
    void runningChanged(bool running);

private:
    void onTick();
    void publish();
    void syncTimer();
    void armAligned();

    sb::MatchEngine engine_;
    QTimer timer_;
    QTimer aligned_; // single shot on the exact instant the clock text changes
    sb::FieldValues published_;
    bool running_ = false;
    bool shutDown_ = false;
};
