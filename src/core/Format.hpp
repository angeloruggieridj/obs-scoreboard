// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Types.hpp"

namespace sb {

using TemplateVars = std::vector<std::pair<std::string, std::string>>;

// A countdown shows whole seconds rounded UP (00:00 only once time is really out), a count-up
// rounded DOWN. With tenthsInLastMinute a countdown under one minute shows "59.9".
std::string formatClock(Tenths value, Direction dir, bool twoDigitMinutes, bool tenthsInLastMinute);
// Cumulative play time: "MM:SS" (minutes may exceed 99) or "H:MM:SS"; seconds rounded down.
std::string formatDuration(Tenths value, TimeFormat format);
// Penalty time remaining: "M:SS", rounded up like a countdown.
std::string formatPenaltyTime(Tenths remaining);
// Stoppage counter: "+M:SS", empty when there is none.
std::string formatStoppage(Tenths overrun);
// Replaces "{name}" with the value of `name`; unknown or unterminated placeholders stay as written.
std::string applyTemplate(std::string_view tpl, const TemplateVars& vars);

} // namespace sb
