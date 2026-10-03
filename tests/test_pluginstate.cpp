// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "PluginState.hpp"

#include <string>
#include <vector>

using namespace sb;

TEST_CASE("bindings round-trip through JSON") {
    const std::vector<FieldBinding> in = {
        {FieldId::Clock, "uuid-1", "Clock text"},
        {FieldId::HomeScore, "uuid-2", "Città – home"},
    };
    const auto out = bindingsFromJson(bindingsToJson(in));
    REQUIRE(out.has_value());
    REQUIRE(out->size() == 2);
    CHECK((*out)[0].field == FieldId::Clock);
    CHECK((*out)[0].uuid == "uuid-1");
    CHECK((*out)[1].field == FieldId::HomeScore);
    CHECK((*out)[1].name == "Città – home");
}

TEST_CASE("an empty binding list round-trips") {
    const auto out = bindingsFromJson(bindingsToJson({}));
    REQUIRE(out.has_value());
    CHECK(out->empty());
}

TEST_CASE("broken binding documents are rejected without throwing") {
    std::string error;
    for (const char* text :
         {"", "not json", "[]", "{\"version\": 2, \"bindings\": []}",
          "{\"version\": \"1\", \"bindings\": []}", "{\"version\": 1}",
          "{\"version\": 1, \"bindings\": {}}", "{\"version\": 1, \"bindings\": [", "null"}) {
        CAPTURE(text);
        error.clear();
        std::optional<std::vector<FieldBinding>> out;
        CHECK_NOTHROW(out = bindingsFromJson(text, &error));
        CHECK_FALSE(out.has_value());
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("bad binding entries are skipped, good ones kept") {
    const char* text = R"({"version": 1, "bindings": [
        {"field": "clock", "uuid": "a", "name": "A"},
        {"field": "future.field", "uuid": "b", "name": "B"},
        {"field": "period", "uuid": 7, "name": "C"},
        {"field": "home.score", "uuid": "", "name": ""},
        "not an object",
        {"field": "clock", "uuid": "dup", "name": "Dup"},
        {"field": "away.score", "uuid": "", "name": "By name only"}
    ]})";
    const auto out = bindingsFromJson(text);
    REQUIRE(out.has_value());
    REQUIRE(out->size() == 2);
    CHECK((*out)[0].field == FieldId::Clock);
    CHECK((*out)[0].uuid == "a");
    CHECK((*out)[1].field == FieldId::AwayScore);
    CHECK((*out)[1].name == "By name only");
}

TEST_CASE("prefs round-trip and fall back on bad input") {
    GlobalPrefs p;
    p.lastSport = "ice_hockey";
    const auto out = prefsFromJson(prefsToJson(p));
    REQUIRE(out.has_value());
    CHECK(out->lastSport == "ice_hockey");

    std::string error;
    CHECK_FALSE(prefsFromJson("{\"version\": 1, \"lastSport\": 5}", &error).has_value());
    CHECK_FALSE(error.empty());
    CHECK_FALSE(prefsFromJson("{\"version\": 9}").has_value());
    CHECK_FALSE(prefsFromJson("garbage").has_value());

    const auto empty = prefsFromJson("{\"version\": 1, \"lastSport\": \"\"}");
    REQUIRE(empty.has_value());
    CHECK(empty->lastSport == "futsal");
    const auto missing = prefsFromJson("{\"version\": 1}");
    REQUIRE(missing.has_value());
    CHECK(missing->lastSport == "futsal");
}
