// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"

#include "VendorProtocol.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace sb;
using nlohmann::json;

namespace {

MatchEngine engineFor(const char* id) {
    return MatchEngine(MatchSettings::forSport(*findSport(id)));
}

VendorRequest parse(const char* type, const char* payload = "{}") {
    return parseVendorRequest(type, payload);
}

// One of the shared golden fixtures in tests/fixtures/vendor, handed in by CMake as SB_FIXTURE_DIR.
std::string readFixture(const char* name) {
    std::ifstream in(std::string(SB_FIXTURE_DIR) + "/" + name, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "missing fixture " << name);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

TEST_CASE("every command request parses into the matching command") {
    CHECK(parse("ClockToggle").kind == VendorRequest::Kind::Command);
    CHECK(std::holds_alternative<cmd::ClockToggle>(parse("ClockToggle").command));
    CHECK(std::holds_alternative<cmd::ClockStart>(parse("ClockStart").command));
    CHECK(std::holds_alternative<cmd::ClockStop>(parse("ClockStop").command));
    CHECK(std::holds_alternative<cmd::ClockReset>(parse("ClockReset").command));
    CHECK(std::holds_alternative<cmd::EndMatch>(parse("EndMatch").command));

    const VendorRequest adjust = parse("ClockAdjust", R"({"deltaTenths":-10})");
    REQUIRE(std::holds_alternative<cmd::ClockAdjust>(adjust.command));
    CHECK(std::get<cmd::ClockAdjust>(adjust.command).delta == -10);

    const VendorRequest set = parse("ClockSet", R"({"tenths":600})");
    REQUIRE(std::holds_alternative<cmd::ClockSet>(set.command));
    CHECK(std::get<cmd::ClockSet>(set.command).value == 600);

    CHECK_FALSE(std::get<cmd::PeriodNext>(parse("PeriodNext").command).confirmed);
    CHECK(
        std::get<cmd::PeriodNext>(parse("PeriodNext", R"({"confirmed":true})").command).confirmed);
    CHECK_FALSE(std::get<cmd::PeriodPrev>(parse("PeriodPrev").command).confirmed);
    CHECK(
        std::get<cmd::PeriodPrev>(parse("PeriodPrev", R"({"confirmed":true})").command).confirmed);

    CHECK(std::get<cmd::StoppageSet>(parse("StoppageSet", R"({"minutes":3})").command).minutes ==
          3);
    CHECK(std::get<cmd::StoppageAdd>(parse("StoppageAdd", R"({"minutes":-2})").command).minutes ==
          -2);
}

TEST_CASE("AddStat carries team, stat and delta") {
    const VendorRequest r = parse("AddStat", R"({"team":"away","stat":"fouls","delta":-2})");
    REQUIRE(std::holds_alternative<cmd::AddStat>(r.command));
    const cmd::AddStat s = std::get<cmd::AddStat>(r.command);
    CHECK(s.team == Team::Away);
    CHECK(s.stat == Stat::Fouls);
    CHECK(s.delta == -2);
    for (const char* stat : {"score", "shots", "fouls", "fouls2"}) {
        const std::string payload =
            std::string("{\"team\":\"home\",\"stat\":\"") + stat + "\",\"delta\":1}";
        CHECK(parse("AddStat", payload.c_str()).kind == VendorRequest::Kind::Command);
    }
    CHECK(parse("AddStat", R"({"team":"both","stat":"score","delta":1})").errorCode == "badField");
    CHECK(parse("AddStat", R"({"team":"home","stat":"cards","delta":1})").errorCode == "badField");
    CHECK(parse("AddStat", R"({"team":"home","stat":"score"})").errorCode == "badField");
}

TEST_CASE("SetTeamName and the penalty requests parse their fields") {
    const VendorRequest name = parse("SetTeamName", R"({"team":"home","name":"Casa"})");
    REQUIRE(std::holds_alternative<cmd::SetTeamName>(name.command));
    CHECK(std::get<cmd::SetTeamName>(name.command).name == "Casa");

    const VendorRequest add = parse(
        "PenaltyAdd",
        R"({"team":"home","player":"24","phases":[{"tenths":1200},{"tenths":1200,"reducesStrength":false}]})");
    REQUIRE(std::holds_alternative<cmd::PenaltyAdd>(add.command));
    const cmd::PenaltyAdd p = std::get<cmd::PenaltyAdd>(add.command);
    CHECK(p.team == Team::Home);
    CHECK(p.player == "24");
    REQUIRE(p.phases.size() == 2);
    CHECK(p.phases[0].duration == 1200);
    CHECK(p.phases[0].reducesStrength);
    CHECK(p.phases[1].duration == 1200);
    CHECK_FALSE(p.phases[1].reducesStrength);

    const VendorRequest edit = parse("PenaltyEdit", R"({"id":7,"tenths":600})");
    REQUIRE(std::holds_alternative<cmd::PenaltyEdit>(edit.command));
    CHECK(std::get<cmd::PenaltyEdit>(edit.command).id == 7);
    CHECK(std::get<cmd::PenaltyEdit>(edit.command).remaining == 600);

    const VendorRequest cancel = parse("PenaltyCancel", R"({"id":3})");
    REQUIRE(std::holds_alternative<cmd::PenaltyCancel>(cancel.command));
    CHECK(std::get<cmd::PenaltyCancel>(cancel.command).id == 3);

    const VendorRequest active = parse("PenaltyCancelActive", R"({"team":"away","slot":1})");
    REQUIRE(std::holds_alternative<cmd::PenaltyCancelActive>(active.command));
    CHECK(std::get<cmd::PenaltyCancelActive>(active.command).team == Team::Away);
    CHECK(std::get<cmd::PenaltyCancelActive>(active.command).slot == 1);

    const VendorRequest stoppage = parse("StoppageSet", R"({"minutes":3})");
    REQUIRE(std::holds_alternative<cmd::StoppageSet>(stoppage.command));
    CHECK(std::get<cmd::StoppageSet>(stoppage.command).minutes == 3);
}

TEST_CASE("NewMatch names a built-in sport and carries the confirmation flag") {
    const VendorRequest r = parse("NewMatch", R"({"sport":"basketball"})");
    CHECK(r.kind == VendorRequest::Kind::NewMatch);
    CHECK(r.sport == "basketball");
    CHECK_FALSE(r.confirmed);
    CHECK(parse("NewMatch", R"({"sport":"basketball","confirmed":true})").confirmed);
    CHECK(parse("NewMatch", R"({"sport":"quidditch"})").errorCode == "unknownSport");
    CHECK(parse("NewMatch", "{}").errorCode == "badField");
    CHECK(parse("NewMatch", R"({"sport":"basketball","confirmed":"yes"})").errorCode == "badField");
}

TEST_CASE("GetState and GetFields are recognised without a command") {
    CHECK(parse("GetState").kind == VendorRequest::Kind::GetState);
    CHECK(parse("GetFields").kind == VendorRequest::Kind::GetFields);
}

TEST_CASE("a malformed request is a stable error, never a crash") {
    CHECK(parse("NoSuchRequest").errorCode == "unknownRequest");
    CHECK(parse("ClockStart", "not json").errorCode == "badRequest");
    CHECK(parse("ClockStart", "[1,2,3]").errorCode == "badRequest");
    CHECK(parse("ClockAdjust", R"({"deltaTenths":"soon"})").errorCode == "badField");
    CHECK(parse("PeriodNext", R"({"confirmed":"yes"})").errorCode == "badField");
    CHECK(parse("PenaltyAdd", R"({"team":"home","player":"24","phases":[]})").errorCode ==
          "badField");
    CHECK(parse("PenaltyAdd", R"({"team":"home","player":"24","phases":[{"tenths":-1}]})")
              .errorCode == "badField");
    CHECK(parse("PenaltyEdit", R"({"id":-1,"tenths":600})").errorCode == "badField");
    CHECK(parse("StoppageSet", R"({"minutes":-1})").errorCode == "badField");
    CHECK(parse("StoppageAdd", R"({"minutes":"soon"})").errorCode == "badField");
    CHECK(parse("PenaltyCancelActive", R"({"team":"home","slot":-1})").errorCode == "badField");
}

TEST_CASE("results and errors become the documented envelopes") {
    CHECK(json::parse(vendorResultJson(CommandResult::ok())) == json{{"result", "ok"}});
    CHECK(json::parse(vendorResultJson(CommandResult::rejected("period.clockRunning"))) ==
          json{{"result", "rejected"}, {"reason", "period.clockRunning"}});
    CHECK(json::parse(vendorResultJson(CommandResult::confirm("period.midPeriod"))) ==
          json{{"result", "confirmation"}, {"reason", "period.midPeriod"}});
    CHECK(json::parse(vendorErrorJson("unknownRequest")) ==
          json{{"result", "error"}, {"error", "unknownRequest"}});
}

TEST_CASE("GetFields exposes every field under its stable key") {
    MatchEngine e = engineFor("ice_hockey");
    e.apply(cmd::AddStat{Team::Home, Stat::Score, 2}, 0);
    const json j = json::parse(vendorFieldsJson(e.fields()));
    CHECK(j["result"] == "ok");
    REQUIRE(j["fields"].is_object());
    CHECK(j["fields"].size() == kFieldCount);
    CHECK(j["fields"]["clock"] == "20:00");
    CHECK(j["fields"]["home.score"] == "2");
    CHECK(j["fields"]["strength"] == "");
    CHECK(j["fields"].contains("playtime"));
    CHECK(j["fields"].contains("stoppage.announced"));
    for (std::size_t i = 0; i < kFieldCount; ++i)
        CHECK(j["fields"].contains(std::string(fieldKey(static_cast<FieldId>(i)))));
}

TEST_CASE("GetState summarises the match, including the penalty list") {
    MatchEngine e = engineFor("ice_hockey");
    e.apply(cmd::AddStat{Team::Home, Stat::Score, 1}, 0);
    e.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}}}, 0);
    const json j = json::parse(vendorStateJson(e));
    CHECK(j["result"] == "ok");
    const json& s = j["state"];
    CHECK(s["protocol"] == 1);
    CHECK(s["sport"] == "ice_hockey");
    CHECK(s["period"] == 1);
    CHECK(s["periodLabel"] == "1");
    CHECK(s["regulationPeriods"] == 3);
    CHECK(s["clock"] == "20:00");
    CHECK(s["clockTenths"] == 12000);
    CHECK(s["clockRunning"] == false);
    CHECK(s["clockDirection"] == "down");
    CHECK(s["homeName"] == "HOME");
    CHECK(s["homeScore"] == 1);
    CHECK(s["awayScore"] == 0);
    CHECK(s["strength"] == "4-5");
    CHECK(s["matchEnded"] == false);
    CHECK(s["stoppageAnnounced"] == "");
    CHECK(s["stoppageAnnouncedMinutes"] == 0);
    REQUIRE(s["penalties"]["home"].size() == 1);
    CHECK(s["penalties"]["home"][0]["player"] == "24");
    CHECK(s["penalties"]["home"][0]["time"] == "2:00");
    CHECK(s["penalties"]["home"][0]["queued"] == false);
    CHECK(s["penalties"]["away"].empty());

    e.apply(cmd::StoppageAdd{3}, 0);
    CHECK(json::parse(vendorStateJson(e))["state"]["stoppageAnnouncedMinutes"] == 3);
    e.apply(cmd::StoppageAdd{-5}, 0);
    CHECK(json::parse(vendorStateJson(e))["state"]["stoppageAnnouncedMinutes"] == 0);

    e.apply(cmd::EndMatch{}, 0);
    CHECK(json::parse(vendorStateJson(e))["state"]["matchEnded"] == true);
}

TEST_CASE("StateChanged lists only the fields that changed") {
    const FieldValues before = engineFor("ice_hockey").fields();
    FieldValues after = before;
    after[static_cast<std::size_t>(FieldId::Clock)] = "19:59";
    after[static_cast<std::size_t>(FieldId::HomeScore)] = "1";
    const json j = json::parse(vendorStateChangedJson(before, after, true, false));
    REQUIRE(j["changed"].is_object());
    CHECK(j["changed"].size() == 2);
    CHECK(j["changed"]["clock"] == "19:59");
    CHECK(j["changed"]["home.score"] == "1");
    CHECK(j["clockRunning"] == true);
    CHECK(j["matchEnded"] == false);
}

TEST_CASE("a full StateChanged lists every field, empty ones included") {
    const FieldValues fields = engineFor("basketball").fields();
    const json j = json::parse(vendorFullStateJson(fields, false, false));
    REQUIRE(j["changed"].is_object());
    CHECK(j["changed"].size() == kFieldCount);
    // basketball has no strength/shots: the empty text must still be published so a client that
    // was showing hockey's "4-5" clears it.
    CHECK(j["changed"]["strength"] == "");
    CHECK(j["changed"].contains("playtime"));
    for (std::size_t i = 0; i < kFieldCount; ++i)
        CHECK(j["changed"].contains(std::string(fieldKey(static_cast<FieldId>(i)))));
}

TEST_CASE("the events carry the documented payloads") {
    MatchEvent periodEnd;
    periodEnd.type = EventType::PeriodEnd;
    periodEnd.period = 2;
    periodEnd.periodLabel = "2";
    CHECK(json::parse(vendorPeriodEndedJson(periodEnd)) == json{{"period", 2}, {"label", "2"}});

    MatchEvent matchEnd;
    matchEnd.type = EventType::MatchEnd;
    matchEnd.period = 3;
    matchEnd.periodLabel = "OT";
    CHECK(json::parse(vendorMatchEndedJson(matchEnd)) == json{{"period", 3}, {"label", "OT"}});

    CHECK(json::parse(vendorScoreChangedJson(Team::Away, 3, 1, 3)) ==
          json{{"team", "away"}, {"score", 3}, {"homeScore", 1}, {"awayScore", 3}});
}

// The shared fixtures are also imported by the two TypeScript suites (Stream Deck, Companion); the
// doctest reproduces them from the codec so a wire change cannot silently drift from the clients.
TEST_CASE("the shared fixtures match the codec output") {
    CHECK(json::parse(readFixture("envelope-ok.json")) ==
          json::parse(vendorResultJson(CommandResult::ok())));

    MatchEngine state = engineFor("ice_hockey");
    state.apply(cmd::AddStat{Team::Home, Stat::Score, 1}, 0);
    state.apply(cmd::PenaltyAdd{Team::Home, "24", {{minutes(2), true}}}, 0);
    CHECK(json::parse(readFixture("state-ice-hockey.json")) == json::parse(vendorStateJson(state)));

    MatchEngine fields = engineFor("ice_hockey");
    fields.apply(cmd::AddStat{Team::Home, Stat::Score, 2}, 0);
    CHECK(json::parse(readFixture("fields.json")) ==
          json::parse(vendorFieldsJson(fields.fields())));

    const FieldValues before = engineFor("ice_hockey").fields();
    FieldValues after = before;
    after[static_cast<std::size_t>(FieldId::Clock)] = "19:59";
    after[static_cast<std::size_t>(FieldId::HomeScore)] = "1";
    CHECK(json::parse(readFixture("event-state-changed.json")) ==
          json::parse(vendorStateChangedJson(before, after, true, false)));
}
