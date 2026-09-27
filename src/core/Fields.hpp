// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace sb {

// Every scoreboard value that can be bound to an OBS Text source. The order is part of the
// FieldValues layout; the string keys are the stable names used in saved bindings and the API.
enum class FieldId : std::size_t {
    Clock,
    Period,
    HomeName,
    AwayName,
    HomeScore,
    AwayScore,
    HomeShots,
    AwayShots,
    HomeFouls,
    AwayFouls,
    HomeFouls2,
    AwayFouls2,
    HomePenalty1Player,
    HomePenalty1Time,
    HomePenalty1Label,
    HomePenalty2Player,
    HomePenalty2Time,
    HomePenalty2Label,
    AwayPenalty1Player,
    AwayPenalty1Time,
    AwayPenalty1Label,
    AwayPenalty2Player,
    AwayPenalty2Time,
    AwayPenalty2Label,
    Strength,
    Stoppage,
    StoppageAnnounced,
    PlayTime,
    Count
};

constexpr std::size_t kFieldCount = static_cast<std::size_t>(FieldId::Count);
using FieldValues = std::array<std::string, kFieldCount>;

std::string_view fieldKey(FieldId id);
std::optional<FieldId> fieldFromKey(std::string_view key);
inline std::string& fieldAt(FieldValues& values, FieldId id) {
    return values[static_cast<std::size_t>(id)];
}

} // namespace sb
