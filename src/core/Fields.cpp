// SPDX-License-Identifier: GPL-2.0-or-later
#include "Fields.hpp"

namespace sb {
namespace {
constexpr std::array<std::string_view, kFieldCount> kKeys = {
    "clock",
    "period",
    "home.name",
    "away.name",
    "home.score",
    "away.score",
    "home.shots",
    "away.shots",
    "home.fouls",
    "away.fouls",
    "home.fouls2",
    "away.fouls2",
    "home.penalty1.player",
    "home.penalty1.time",
    "home.penalty1.label",
    "home.penalty2.player",
    "home.penalty2.time",
    "home.penalty2.label",
    "away.penalty1.player",
    "away.penalty1.time",
    "away.penalty1.label",
    "away.penalty2.player",
    "away.penalty2.time",
    "away.penalty2.label",
    "strength",
    "stoppage",
    "stoppage.announced",
    "playtime",
};
} // namespace

std::string_view fieldKey(FieldId id) {
    return kKeys[static_cast<std::size_t>(id)];
}

std::optional<FieldId> fieldFromKey(std::string_view key) {
    for (std::size_t i = 0; i < kFieldCount; ++i)
        if (kKeys[i] == key) return static_cast<FieldId>(i);
    return std::nullopt;
}

} // namespace sb
