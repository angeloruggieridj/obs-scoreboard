// SPDX-License-Identifier: GPL-2.0-or-later
#include "ClockDriver.hpp"

#include <utility>

#include <util/platform.h>

namespace {
constexpr int kTickMs = 20;
} // namespace

ClockDriver::ClockDriver(sb::MatchSettings settings, QObject* parent)
    : QObject(parent), engine_(std::move(settings)), published_(engine_.fields()) {
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(kTickMs);
    connect(&timer_, &QTimer::timeout, this, &ClockDriver::onTick);
}

sb::Micros ClockDriver::nowMicros() {
    return static_cast<sb::Micros>(os_gettime_ns() / 1000);
}

sb::CommandResult ClockDriver::apply(const sb::Command& command) {
    return applyAt(command, nowMicros());
}

sb::CommandResult ClockDriver::applyAt(const sb::Command& command, sb::Micros now) {
    if (shutDown_) return sb::CommandResult::rejected("plugin.shuttingDown");
    const sb::CommandResult result = engine_.apply(command, now);
    publish();
    syncTimer();
    return result;
}

void ClockDriver::shutdown() {
    shutDown_ = true;
    timer_.stop();
}

void ClockDriver::onTick() {
    engine_.advance(nowMicros());
    publish();
    syncTimer();
}

void ClockDriver::publish() {
    sb::FieldValues current = engine_.fields();
    if (current == published_) return;
    published_ = std::move(current);
    emit fieldsChanged(published_);
}

void ClockDriver::syncTimer() {
    const bool running = engine_.clock().running();
    if (running && !timer_.isActive() && !shutDown_) timer_.start();
    if (!running && timer_.isActive()) timer_.stop();
    if (running != running_) {
        running_ = running;
        emit runningChanged(running_);
    }
}
