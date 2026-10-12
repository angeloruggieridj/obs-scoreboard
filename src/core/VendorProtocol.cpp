// SPDX-License-Identifier: GPL-2.0-or-later
#include "VendorProtocol.hpp"

#include <cstdint>
#include <limits>
#include <utility>

#include <nlohmann/json.hpp>

#include "PanelModel.hpp"

namespace sb {

using nlohmann::json;

namespace {

VendorRequest invalidRequest(const char* code) {
    VendorRequest r;
    r.kind = VendorRequest::Kind::Invalid;
    r.errorCode = code;
    return r;
}

VendorRequest commandRequest(Command command) {
    VendorRequest r;
    r.kind = VendorRequest::Kind::Command;
    r.command = std::move(command);
    return r;
}

bool intField(const json& obj, const char* key, std::int64_t& out) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number_integer()) return false;
    out = it->get<std::int64_t>();
    return true;
}

// A non-negative integer that fits a PenaltyId (std::uint32_t).
bool idField(const json& obj, const char* key, std::int64_t& out) {
    std::int64_t v = 0;
    if (!intField(obj, key, v)) return false;
    if (v < 0 || static_cast<std::uint64_t>(v) > std::numeric_limits<std::uint32_t>::max())
        return false;
    out = v;
    return true;
}

bool stringField(const json& obj, const char* key, std::string& out) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string()) return false;
    out = it->get<std::string>();
    return true;
}

// An absent key leaves `out` untouched and succeeds; a present key must be a boolean.
bool optionalBoolField(const json& obj, const char* key, bool& out) {
    const auto it = obj.find(key);
    if (it == obj.end()) return true;
    if (!it->is_boolean()) return false;
    out = it->get<bool>();
    return true;
}

bool teamField(const json& obj, Team& out) {
    std::string s;
    if (!stringField(obj, "team", s)) return false;
    const std::optional<Team> t = teamFromKey(s);
    if (!t) return false;
    out = *t;
    return true;
}

bool statField(const json& obj, Stat& out) {
    std::string s;
    if (!stringField(obj, "stat", s)) return false;
    if (s == "score")
        out = Stat::Score;
    else if (s == "shots")
        out = Stat::Shots;
    else if (s == "fouls")
        out = Stat::Fouls;
    else if (s == "fouls2")
        out = Stat::Fouls2;
    else
        return false;
    return true;
}

bool hasMatchEnded(const MatchEngine& engine) {
    for (const MatchEvent& e : engine.events().events())
        if (e.type == EventType::MatchEnd) return true;
    return false;
}

} // namespace

VendorRequest parseVendorRequest(std::string_view type, std::string_view requestJson) {
    json j = json::parse(requestJson.begin(), requestJson.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return invalidRequest("badRequest");

    if (type == "ClockToggle") return commandRequest(cmd::ClockToggle{});
    if (type == "ClockStart") return commandRequest(cmd::ClockStart{});
    if (type == "ClockStop") return commandRequest(cmd::ClockStop{});
    if (type == "ClockReset") return commandRequest(cmd::ClockReset{});
    if (type == "EndMatch") return commandRequest(cmd::EndMatch{});
    if (type == "GetState") {
        VendorRequest r;
        r.kind = VendorRequest::Kind::GetState;
        return r;
    }
    if (type == "GetFields") {
        VendorRequest r;
        r.kind = VendorRequest::Kind::GetFields;
        return r;
    }
    if (type == "ClockAdjust") {
        std::int64_t delta = 0;
        if (!intField(j, "deltaTenths", delta)) return invalidRequest("badField");
        return commandRequest(cmd::ClockAdjust{static_cast<Tenths>(delta)});
    }
    if (type == "ClockSet") {
        std::int64_t value = 0;
        if (!intField(j, "tenths", value)) return invalidRequest("badField");
        return commandRequest(cmd::ClockSet{static_cast<Tenths>(value)});
    }
    if (type == "PeriodNext") {
        bool confirmed = false;
        if (!optionalBoolField(j, "confirmed", confirmed)) return invalidRequest("badField");
        return commandRequest(cmd::PeriodNext{confirmed});
    }
    if (type == "PeriodPrev") {
        bool confirmed = false;
        if (!optionalBoolField(j, "confirmed", confirmed)) return invalidRequest("badField");
        return commandRequest(cmd::PeriodPrev{confirmed});
    }
    if (type == "AddStat") {
        Team team = Team::Home;
        Stat stat = Stat::Score;
        std::int64_t delta = 0;
        if (!teamField(j, team) || !statField(j, stat) || !intField(j, "delta", delta))
            return invalidRequest("badField");
        if (delta < std::numeric_limits<int>::min() || delta > std::numeric_limits<int>::max())
            return invalidRequest("badField");
        return commandRequest(cmd::AddStat{team, stat, static_cast<int>(delta)});
    }
    if (type == "SetTeamName") {
        Team team = Team::Home;
        std::string name;
        if (!teamField(j, team) || !stringField(j, "name", name)) return invalidRequest("badField");
        return commandRequest(cmd::SetTeamName{team, std::move(name)});
    }
    if (type == "PenaltyAdd") {
        Team team = Team::Home;
        std::string player;
        if (!teamField(j, team) || !stringField(j, "player", player))
            return invalidRequest("badField");
        const auto it = j.find("phases");
        if (it == j.end() || !it->is_array() || it->empty()) return invalidRequest("badField");
        std::vector<PenaltyPhase> phases;
        for (const json& p : *it) {
            std::int64_t phaseTenths = 0;
            bool reduces = true;
            if (!p.is_object() || !intField(p, "tenths", phaseTenths))
                return invalidRequest("badField");
            if (phaseTenths < 0 || phaseTenths > std::numeric_limits<int>::max())
                return invalidRequest("badField");
            if (!optionalBoolField(p, "reducesStrength", reduces))
                return invalidRequest("badField");
            phases.push_back(PenaltyPhase{static_cast<Tenths>(phaseTenths), reduces});
        }
        return commandRequest(cmd::PenaltyAdd{team, std::move(player), std::move(phases)});
    }
    if (type == "PenaltyEdit") {
        std::int64_t id = 0;
        std::int64_t tenths = 0;
        if (!idField(j, "id", id) || !intField(j, "tenths", tenths))
            return invalidRequest("badField");
        return commandRequest(
            cmd::PenaltyEdit{static_cast<PenaltyId>(id), static_cast<Tenths>(tenths)});
    }
    if (type == "PenaltyCancel") {
        std::int64_t id = 0;
        if (!idField(j, "id", id)) return invalidRequest("badField");
        return commandRequest(cmd::PenaltyCancel{static_cast<PenaltyId>(id)});
    }
    if (type == "PenaltyCancelActive") {
        Team team = Team::Home;
        std::int64_t slot = 0;
        if (!teamField(j, team) || !intField(j, "slot", slot)) return invalidRequest("badField");
        if (slot < 0 || slot > std::numeric_limits<int>::max()) return invalidRequest("badField");
        return commandRequest(cmd::PenaltyCancelActive{team, static_cast<int>(slot)});
    }
    if (type == "StoppageSet") {
        std::int64_t minutes = 0;
        if (!intField(j, "minutes", minutes)) return invalidRequest("badField");
        if (minutes < 0 || minutes > std::numeric_limits<int>::max())
            return invalidRequest("badField");
        return commandRequest(cmd::StoppageSet{static_cast<int>(minutes)});
    }
    if (type == "StoppageAdd") {
        std::int64_t minutes = 0;
        if (!intField(j, "minutes", minutes)) return invalidRequest("badField");
        if (minutes < std::numeric_limits<int>::min() || minutes > std::numeric_limits<int>::max())
            return invalidRequest("badField");
        return commandRequest(cmd::StoppageAdd{static_cast<int>(minutes)});
    }
    if (type == "NewMatch") {
        std::string sport;
        if (!stringField(j, "sport", sport)) return invalidRequest("badField");
        if (!findSport(sport)) return invalidRequest("unknownSport");
        bool confirmed = false;
        if (!optionalBoolField(j, "confirmed", confirmed)) return invalidRequest("badField");
        VendorRequest r;
        r.kind = VendorRequest::Kind::NewMatch;
        r.sport = std::move(sport);
        r.confirmed = confirmed;
        return r;
    }
    return invalidRequest("unknownRequest");
}

std::string vendorResultJson(const CommandResult& result) {
    json j;
    switch (result.status) {
    case CommandResult::Status::Ok:
        j["result"] = "ok";
        break;
    case CommandResult::Status::Rejected:
        j["result"] = "rejected";
        j["reason"] = result.reason;
        break;
    case CommandResult::Status::NeedsConfirmation:
        j["result"] = "confirmation";
        j["reason"] = result.reason;
        break;
    }
    return j.dump();
}

std::string vendorErrorJson(std::string_view errorCode) {
    return json{{"result", "error"}, {"error", std::string(errorCode)}}.dump();
}

std::string vendorStateJson(const MatchEngine& engine) {
    const MatchSettings& s = engine.settings();
    const FieldValues f = engine.fields();
    const auto at = [&f](FieldId id) -> const std::string& {
        return f[static_cast<std::size_t>(id)];
    };

    json state;
    state["protocol"] = 1;
    state["sport"] = s.sport.id;
    state["period"] = engine.periods().index();
    state["periodLabel"] = engine.periods().label();
    state["regulationPeriods"] = s.sport.periods;
    state["clock"] = at(FieldId::Clock);
    state["clockTenths"] = engine.clock().value();
    state["clockRunning"] = engine.clock().running();
    state["clockDirection"] = s.sport.direction == Direction::Down ? "down" : "up";
    state["homeName"] = s.homeName;
    state["awayName"] = s.awayName;
    state["homeScore"] = engine.counters().get(Team::Home, Stat::Score);
    state["awayScore"] = engine.counters().get(Team::Away, Stat::Score);
    state["homeShots"] = engine.counters().get(Team::Home, Stat::Shots);
    state["awayShots"] = engine.counters().get(Team::Away, Stat::Shots);
    state["homeFouls"] = engine.counters().get(Team::Home, Stat::Fouls);
    state["awayFouls"] = engine.counters().get(Team::Away, Stat::Fouls);
    state["homeFouls2"] = engine.counters().get(Team::Home, Stat::Fouls2);
    state["awayFouls2"] = engine.counters().get(Team::Away, Stat::Fouls2);
    state["strength"] = at(FieldId::Strength);
    state["stoppage"] = at(FieldId::Stoppage);
    state["stoppageAnnounced"] = at(FieldId::StoppageAnnounced);
    state["stoppageAnnouncedMinutes"] = engine.stoppageAnnounced();
    state["playTime"] = at(FieldId::PlayTime);
    state["matchEnded"] = hasMatchEnded(engine);
    for (Team team : {Team::Home, Team::Away}) {
        json rows = json::array();
        for (const PenaltyRow& r : penaltyRows(engine.penalties(team)))
            rows.push_back({{"id", r.id},
                            {"player", r.player},
                            {"time", r.time},
                            {"phase2", r.phase2},
                            {"queued", r.queued}});
        state["penalties"][std::string(teamKey(team))] = rows;
    }
    return json{{"result", "ok"}, {"state", state}}.dump();
}

std::string vendorFieldsJson(const FieldValues& fields) {
    json out = json::object();
    for (std::size_t i = 0; i < kFieldCount; ++i)
        out[std::string(fieldKey(static_cast<FieldId>(i)))] = fields[i];
    return json{{"result", "ok"}, {"fields", out}}.dump();
}

std::string vendorStateChangedJson(const FieldValues& previous, const FieldValues& current,
                                   bool clockRunning, bool matchEnded) {
    json changed = json::object();
    for (std::size_t i = 0; i < kFieldCount; ++i)
        if (previous[i] != current[i])
            changed[std::string(fieldKey(static_cast<FieldId>(i)))] = current[i];
    return json{{"changed", changed}, {"clockRunning", clockRunning}, {"matchEnded", matchEnded}}
        .dump();
}

std::string vendorFullStateJson(const FieldValues& fields, bool clockRunning, bool matchEnded) {
    json changed = json::object();
    for (std::size_t i = 0; i < kFieldCount; ++i)
        changed[std::string(fieldKey(static_cast<FieldId>(i)))] = fields[i];
    return json{{"changed", changed}, {"clockRunning", clockRunning}, {"matchEnded", matchEnded}}
        .dump();
}

std::string vendorPeriodEndedJson(const MatchEvent& event) {
    return json{{"period", event.period}, {"label", event.periodLabel}}.dump();
}

std::string vendorMatchEndedJson(const MatchEvent& event) {
    return json{{"period", event.period}, {"label", event.periodLabel}}.dump();
}

std::string vendorScoreChangedJson(Team team, int score, int homeScore, int awayScore) {
    return json{{"team", std::string(teamKey(team))},
                {"score", score},
                {"homeScore", homeScore},
                {"awayScore", awayScore}}
        .dump();
}

} // namespace sb
