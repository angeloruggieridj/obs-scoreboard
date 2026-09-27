// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace sb {

// Game time in tenths of a second: the clock's internal resolution.
using Tenths = std::int64_t;
// Monotonic time in microseconds. Always supplied by the caller: the core never reads a clock,
// which is what makes every test deterministic.
using Micros = std::int64_t;

constexpr Tenths kTenthsPerSecond = 10;
constexpr Tenths kTenthsPerMinute = 600;
constexpr Micros kMicrosPerTenth = 100000;
// Number of overtime periods with no upper bound (basketball, sudden-death sports).
constexpr int kUnlimited = -1;

constexpr Tenths seconds(std::int64_t s) { return s * kTenthsPerSecond; }
constexpr Tenths minutes(std::int64_t m) { return m * kTenthsPerMinute; }

enum class Team { Home, Away };
enum class Direction { Up, Down };
// What a count-up clock does when it reaches the period duration.
enum class StoppageMode { StopAtDuration, SecondaryCounter, RunPastDuration };
// EachRegulationPeriod: fouls restart every regulation period; overtime keeps the last one's.
enum class FoulReset { Never, EachRegulationPeriod };
enum class StrengthSource { None, Penalties, SecondFouls };
enum class TimeFormat { MinutesSeconds, HoursMinutesSeconds };

constexpr std::size_t teamIndex(Team t) { return t == Team::Home ? 0 : 1; }
std::string_view teamKey(Team t);
std::optional<Team> teamFromKey(std::string_view key);

} // namespace sb
