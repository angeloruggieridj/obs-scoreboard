// SPDX-License-Identifier: GPL-2.0-or-later
#include "Format.hpp"

#include <algorithm>

namespace sb {
namespace {

std::string pad2(std::int64_t v) {
    return (v < 10 ? "0" : "") + std::to_string(v);
}

std::string minutesSeconds(std::int64_t totalSeconds, bool padMinutes) {
    const std::int64_t m = totalSeconds / 60;
    const std::int64_t s = totalSeconds % 60;
    return (padMinutes ? pad2(m) : std::to_string(m)) + ":" + pad2(s);
}

} // namespace

std::string formatClock(Tenths value, Direction dir, bool twoDigitMinutes,
                        bool tenthsInLastMinute) {
    value = std::max<Tenths>(0, value);
    if (tenthsInLastMinute && dir == Direction::Down && value < kTenthsPerMinute) {
        return std::to_string(value / kTenthsPerSecond) + "." +
               std::to_string(value % kTenthsPerSecond);
    }
    const std::int64_t secs = dir == Direction::Down
                                  ? (value + kTenthsPerSecond - 1) / kTenthsPerSecond
                                  : value / kTenthsPerSecond;
    return minutesSeconds(secs, twoDigitMinutes);
}

std::string formatDuration(Tenths value, TimeFormat format) {
    const std::int64_t secs = std::max<Tenths>(0, value) / kTenthsPerSecond;
    if (format == TimeFormat::MinutesSeconds) return minutesSeconds(secs, true);
    return std::to_string(secs / 3600) + ":" + pad2((secs / 60) % 60) + ":" + pad2(secs % 60);
}

std::string formatPenaltyTime(Tenths remaining) {
    const std::int64_t secs =
        (std::max<Tenths>(0, remaining) + kTenthsPerSecond - 1) / kTenthsPerSecond;
    return minutesSeconds(secs, false);
}

std::string formatStoppage(Tenths overrun) {
    if (overrun <= 0) return {};
    return "+" + minutesSeconds(overrun / kTenthsPerSecond, false);
}

std::string applyTemplate(std::string_view tpl, const TemplateVars& vars) {
    std::string out;
    out.reserve(tpl.size());
    std::size_t i = 0;
    while (i < tpl.size()) {
        if (tpl[i] == '{') {
            const std::size_t close = tpl.find('}', i + 1);
            if (close != std::string_view::npos) {
                const std::string_view name = tpl.substr(i + 1, close - i - 1);
                const auto it = std::find_if(vars.begin(), vars.end(),
                                             [&](const auto& v) { return v.first == name; });
                if (it != vars.end()) {
                    out += it->second;
                    i = close + 1;
                    continue;
                }
            }
        }
        out += tpl[i];
        ++i;
    }
    return out;
}

} // namespace sb
