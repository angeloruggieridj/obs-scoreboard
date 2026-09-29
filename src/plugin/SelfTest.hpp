// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

class ClockDriver;
class FieldSinks;

// The fps selftest of spec §10.2, inside the real OBS. Does nothing unless
// OBS_SCOREBOARD_SELFTEST=1: normal users never run it.
namespace selftest {

// Call on OBS_FRONTEND_EVENT_FINISHED_LOADING (it may fire more than once; only the first counts).
void maybeStart(ClockDriver* driver, FieldSinks* sinks);
// Call on OBS_FRONTEND_EVENT_EXIT before the driver and sinks are deleted.
void stop();

} // namespace selftest
