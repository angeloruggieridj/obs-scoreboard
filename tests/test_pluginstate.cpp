// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "MatchEngine.hpp"
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

TEST_CASE("writers never throw on invalid UTF-8") {
    const std::string bad = "\xC3\x28";
    std::string json;
    CHECK_NOTHROW(json = bindingsToJson({{FieldId::Clock, "u", bad}}));
    const auto b = bindingsFromJson(json);
    REQUIRE(b.has_value());
    REQUIRE(b->size() == 1);
    CHECK((*b)[0].name.find("\xEF\xBF\xBD") != std::string::npos);

    GlobalPrefs p;
    p.lastSport = bad;
    CHECK_NOTHROW(json = prefsToJson(p));
    const auto q = prefsFromJson(json);
    REQUIRE(q.has_value());
    CHECK(q->lastSport.find("\xEF\xBF\xBD") != std::string::npos);
}

TEST_CASE("a version 1 settings file is still read") {
    const auto p = prefsFromJson("{\"version\": 1, \"lastSport\": \"ice_hockey\"}");
    REQUIRE(p.has_value());
    CHECK(p->lastSport == "ice_hockey");
    CHECK(p->sportSettings.empty());
    CHECK(p->scoreColor.empty());
    CHECK(p->foulColor.empty());
    CHECK_FALSE(p->sportChosen); // the Phase 4 default was never a choice
}

TEST_CASE("version 2 prefs round-trip with per-sport settings and colors") {
    GlobalPrefs p;
    p.lastSport = "soccer";
    p.scoreColor = "#112233";
    p.foulColor = "#AABBCC";
    p.sportChosen = true;
    MatchSettings soccer = MatchSettings::forSport(*findSport("soccer"));
    soccer.twoDigitMinutes = false;
    soccer.sport.periodDuration = minutes(40);
    p.sportSettings["soccer"] = soccer;
    p.sportSettings["futsal"] = MatchSettings::forSport(*findSport("futsal"));

    std::string error;
    const auto out = prefsFromJson(prefsToJson(p), &error);
    REQUIRE_MESSAGE(out.has_value(), error);
    CHECK(out->lastSport == "soccer");
    CHECK(out->scoreColor == "#112233");
    CHECK(out->foulColor == "#AABBCC");
    CHECK(out->sportChosen);
    REQUIRE(out->sportSettings.size() == 2);
    CHECK(out->sportSettings.at("soccer").sport.periodDuration == minutes(40));
    CHECK_FALSE(out->sportSettings.at("soccer").twoDigitMinutes);
}

TEST_CASE("bad colors fall back to the default without failing the file") {
    const auto lower = prefsFromJson("{\"version\": 2, \"scoreColor\": \"#aabbcc\"}");
    REQUIRE(lower.has_value());
    CHECK(lower->scoreColor == "#AABBCC");
    for (const char* text :
         {"{\"version\": 2, \"scoreColor\": \"red\"}", "{\"version\": 2, \"scoreColor\": 5}",
          "{\"version\": 2, \"scoreColor\": \"#12345\"}"}) {
        CAPTURE(text);
        const auto out = prefsFromJson(text);
        REQUIRE(out.has_value());
        CHECK(out->scoreColor.empty());
    }
}

TEST_CASE("broken per-sport entries are skipped, the rest is kept") {
    GlobalPrefs p;
    p.sportSettings["futsal"] = MatchSettings::forSport(*findSport("futsal"));
    p.sportSettings["soccer"] = MatchSettings::forSport(*findSport("soccer"));
    std::string json = prefsToJson(p);
    // Make the soccer entry invalid (zero periods) and keep the futsal one.
    const auto soccerStart = json.find("\"soccer\"");
    REQUIRE(soccerStart != std::string::npos);
    const auto periods = json.find("\"periods\": 2", soccerStart);
    REQUIRE(periods != std::string::npos);
    json.replace(periods, std::string("\"periods\": 2").size(), "\"periods\": 0");
    const auto out = prefsFromJson(json);
    REQUIRE(out.has_value());
    CHECK(out->sportSettings.count("futsal") == 1);
    CHECK(out->sportSettings.count("soccer") == 0);
}

TEST_CASE("an entry stored under the wrong sport id is skipped") {
    GlobalPrefs p;
    p.sportSettings["futsal"] = MatchSettings::forSport(*findSport("futsal"));
    std::string json = prefsToJson(p);
    const auto key = json.find("\"futsal\": {");
    REQUIRE(key != std::string::npos);
    json.replace(key, std::string("\"futsal\"").size(), "\"soccer\"");
    const auto out = prefsFromJson(json);
    REQUIRE(out.has_value());
    CHECK(out->sportSettings.empty());
}

TEST_CASE("sportSettings of the wrong JSON type is ignored") {
    const auto out = prefsFromJson("{\"version\": 2, \"sportSettings\": [1, 2]}");
    REQUIRE(out.has_value());
    CHECK(out->sportSettings.empty());
}

TEST_CASE("sportChosen is true only for a real boolean") {
    CHECK_FALSE(GlobalPrefs{}.sportChosen);
    CHECK(prefsFromJson("{\"version\": 2, \"sportChosen\": true}")->sportChosen);
    CHECK_FALSE(prefsFromJson("{\"version\": 2, \"sportChosen\": false}")->sportChosen);
    CHECK_FALSE(prefsFromJson("{\"version\": 2, \"sportChosen\": \"yes\"}")->sportChosen);
    CHECK_FALSE(prefsFromJson("{\"version\": 2}")->sportChosen);
}

TEST_CASE("file output preferences default to off and round-trip") {
    CHECK_FALSE(GlobalPrefs{}.fileOutputEnabled);
    CHECK(GlobalPrefs{}.fileOutputDir.empty());

    GlobalPrefs p;
    p.fileOutputEnabled = true;
    p.fileOutputDir = "C:/ScoreBoard output";
    const auto out = prefsFromJson(prefsToJson(p));
    REQUIRE(out.has_value());
    CHECK(out->fileOutputEnabled);
    CHECK(out->fileOutputDir == "C:/ScoreBoard output");
}

TEST_CASE("a version 2 settings file has no file output") {
    const auto p = prefsFromJson("{\"version\": 2, \"lastSport\": \"soccer\"}");
    REQUIRE(p.has_value());
    CHECK_FALSE(p->fileOutputEnabled);
    CHECK(p->fileOutputDir.empty());
}

TEST_CASE("a broken file output entry falls back without failing the file") {
    const auto wrongEnabled = prefsFromJson("{\"version\": 3, \"fileOutputEnabled\": \"yes\"}");
    REQUIRE(wrongEnabled.has_value());
    CHECK_FALSE(wrongEnabled->fileOutputEnabled);

    const auto wrongDir =
        prefsFromJson("{\"version\": 3, \"fileOutputEnabled\": true, \"fileOutputDir\": 5}");
    REQUIRE(wrongDir.has_value());
    CHECK(wrongDir->fileOutputEnabled);
    CHECK(wrongDir->fileOutputDir.empty());

    const auto ok = prefsFromJson(
        "{\"version\": 3, \"fileOutputEnabled\": true, \"fileOutputDir\": \"/tmp/x\"}");
    REQUIRE(ok.has_value());
    CHECK(ok->fileOutputEnabled);
    CHECK(ok->fileOutputDir == "/tmp/x");
}

TEST_CASE("settingsForSport returns the remembered settings or the preset") {
    GlobalPrefs p;
    CHECK_FALSE(p.sportChosen);
    const SportPreset& futsal = *findSport("futsal");
    CHECK(settingsForSport(p, futsal).sport.periodDuration == futsal.periodDuration);

    MatchSettings custom = MatchSettings::forSport(futsal);
    custom.sport.periodDuration = minutes(25);
    custom.homeName = "Roseville";
    rememberSettings(p, custom);
    CHECK(p.lastSport == "futsal");
    CHECK(p.sportChosen);
    const MatchSettings back = settingsForSport(p, futsal);
    CHECK(back.sport.periodDuration == minutes(25));
    CHECK(back.homeName == MatchSettings{}.homeName); // names are match state, not a preference
    CHECK(back.awayName == MatchSettings{}.awayName);
    // Other sports are untouched.
    CHECK(settingsForSport(p, *findSport("soccer")).sport.periodDuration == minutes(45));
}
