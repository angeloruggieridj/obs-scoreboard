// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "Fields.hpp"

#include <set>
#include <string>

using namespace sb;

TEST_CASE("every field has a unique stable key that round-trips") {
    std::set<std::string> keys;
    for (std::size_t i = 0; i < kFieldCount; ++i) {
        const auto id = static_cast<FieldId>(i);
        const std::string key(fieldKey(id));
        CHECK_FALSE(key.empty());
        CHECK(keys.insert(key).second);
        CHECK(fieldFromKey(key) == id);
    }
    CHECK(fieldKey(FieldId::Clock) == "clock");
    CHECK(fieldKey(FieldId::AwayPenalty2Label) == "away.penalty2.label");
    CHECK_FALSE(fieldFromKey("nope").has_value());
}
