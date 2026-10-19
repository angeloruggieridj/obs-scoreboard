// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

class ClockDriver;
class FieldSinks;

// Development tool, only run when OBS_SCOREBOARD_GRAB=<folder> is set: renders the real dock to PNG
// files (several sports, the dark and a light theme, two densities and widths) with a match in
// progress, so a change of look can be checked without capturing the screen. Returns the number of
// pictures written and leaves `grab.done` in the folder.
namespace dockgrab {

int run(ClockDriver* driver, FieldSinks* sinks, const QString& directory);

} // namespace dockgrab
