// SPDX-License-Identifier: GPL-2.0-or-later
#include "MatchStateSaver.hpp"

#include <utility>

#include <util/base.h>

#include "AtomicFile.hpp"
#include "ClockDriver.hpp"
#include "MatchJson.hpp"
#include "plugin-support.h"

namespace {
constexpr int kMinIntervalMs = 2000;
}

MatchStateSaver::MatchStateSaver(ClockDriver* driver, std::string path, QObject* parent)
    : QObject(parent), driver_(driver), path_(std::move(path)) {
    timer_.setSingleShot(true);
    timer_.setInterval(kMinIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &MatchStateSaver::flush);
    if (driver_) {
        connect(driver_, &ClockDriver::commandApplied, this, [this]() {
            arm();
            markDirty();
        });
        connect(driver_, &ClockDriver::fieldsChanged, this,
                [this](const sb::FieldValues&) { markDirty(); });
        connect(driver_, &ClockDriver::runningChanged, this, [this](bool) { markDirty(); });
    }
}

void MatchStateSaver::arm() {
    armed_ = true;
}

void MatchStateSaver::markDirty() {
    if (!armed_) return;
    dirty_ = true;
    if (!timer_.isActive()) timer_.start();
}

void MatchStateSaver::flush() {
    if (!armed_ || !dirty_ || !driver_) return;
    std::string error;
    if (writeFileAtomic(path_, sb::toJson(driver_->engine().snapshot()), &error))
        dirty_ = false;
    else
        obs_log(LOG_WARNING, "could not save the match: %s", error.c_str());
}
