// SPDX-License-Identifier: GPL-2.0-or-later
#include "MatchJson.hpp"

#include <set>

#include <nlohmann/json.hpp>

namespace sb {

using nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(Team, {{Team::Home, "home"}, {Team::Away, "away"}})
NLOHMANN_JSON_SERIALIZE_ENUM(Direction, {{Direction::Up, "up"}, {Direction::Down, "down"}})
NLOHMANN_JSON_SERIALIZE_ENUM(StoppageMode, {{StoppageMode::StopAtDuration, "stop"},
                                            {StoppageMode::SecondaryCounter, "secondary"},
                                            {StoppageMode::RunPastDuration, "runPast"}})
NLOHMANN_JSON_SERIALIZE_ENUM(FoulReset, {{FoulReset::Never, "never"},
                                         {FoulReset::EachRegulationPeriod, "eachRegulationPeriod"}})
NLOHMANN_JSON_SERIALIZE_ENUM(StrengthSource, {{StrengthSource::None, "none"},
                                              {StrengthSource::Penalties, "penalties"},
                                              {StrengthSource::SecondFouls, "secondFouls"}})
NLOHMANN_JSON_SERIALIZE_ENUM(TimeFormat, {{TimeFormat::MinutesSeconds, "mmss"},
                                          {TimeFormat::HoursMinutesSeconds, "hmmss"}})
NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {{EventType::PeriodStart, "periodStart"},
                                         {EventType::PeriodEnd, "periodEnd"},
                                         {EventType::Score, "score"},
                                         {EventType::Penalty, "penalty"},
                                         {EventType::MatchEnd, "matchEnd"}})

void to_json(json& j, const PenaltyPhase& p) {
    j = {{"duration", p.duration}, {"reducesStrength", p.reducesStrength}};
}
void from_json(const json& j, PenaltyPhase& p) {
    j.at("duration").get_to(p.duration);
    j.at("reducesStrength").get_to(p.reducesStrength);
}

void to_json(json& j, const PenaltyOption& o) {
    j = {{"label", o.label}, {"phases", o.phases}};
}
void from_json(const json& j, PenaltyOption& o) {
    j.at("label").get_to(o.label);
    j.at("phases").get_to(o.phases);
}

void to_json(json& j, const SportPreset& s) {
    j = {{"id", s.id},
         {"segment", s.segment},
         {"periods", s.periods},
         {"periodDuration", s.periodDuration},
         {"direction", s.direction},
         {"overtimePeriods", s.overtimePeriods},
         {"overtimeDuration", s.overtimeDuration},
         {"continuousDisplay", s.continuousDisplay},
         {"shots", s.shots},
         {"fouls", s.fouls},
         {"fouls2", s.fouls2},
         {"foulReset", s.foulReset},
         {"foulsLabel", s.foulsLabel},
         {"fouls2Label", s.fouls2Label},
         {"scoreLabel", s.scoreLabel},
         {"scoreValues", s.scoreValues},
         {"penaltyOptions", s.penaltyOptions},
         {"playersPerSide", s.playersPerSide},
         {"minPlayers", s.minPlayers},
         {"strengthSource", s.strengthSource},
         {"stoppage", s.stoppage}};
}
void from_json(const json& j, SportPreset& s) {
    j.at("id").get_to(s.id);
    j.at("segment").get_to(s.segment);
    j.at("periods").get_to(s.periods);
    j.at("periodDuration").get_to(s.periodDuration);
    j.at("direction").get_to(s.direction);
    j.at("overtimePeriods").get_to(s.overtimePeriods);
    j.at("overtimeDuration").get_to(s.overtimeDuration);
    j.at("continuousDisplay").get_to(s.continuousDisplay);
    j.at("shots").get_to(s.shots);
    j.at("fouls").get_to(s.fouls);
    j.at("fouls2").get_to(s.fouls2);
    j.at("foulReset").get_to(s.foulReset);
    j.at("foulsLabel").get_to(s.foulsLabel);
    j.at("fouls2Label").get_to(s.fouls2Label);
    j.at("scoreLabel").get_to(s.scoreLabel);
    j.at("scoreValues").get_to(s.scoreValues);
    j.at("penaltyOptions").get_to(s.penaltyOptions);
    j.at("playersPerSide").get_to(s.playersPerSide);
    j.at("minPlayers").get_to(s.minPlayers);
    j.at("strengthSource").get_to(s.strengthSource);
    j.at("stoppage").get_to(s.stoppage);
}

void to_json(json& j, const StrengthFormat& f) {
    j = {{"even", f.even}, {"uneven", f.uneven}};
}
void from_json(const json& j, StrengthFormat& f) {
    j.at("even").get_to(f.even);
    j.at("uneven").get_to(f.uneven);
}

void to_json(json& j, const MatchSettings& s) {
    j = {{"sport", s.sport},
         {"homeName", s.homeName},
         {"awayName", s.awayName},
         {"twoDigitMinutes", s.twoDigitMinutes},
         {"tenthsInLastMinute", s.tenthsInLastMinute},
         {"stoppage", s.stoppage},
         {"penaltyLabelFormat", s.penaltyLabelFormat},
         {"strength", s.strength},
         {"playTimeFormat", s.playTimeFormat},
         {"regulationLabels", s.regulationLabels},
         {"overtimeLabels", s.overtimeLabels}};
}
void from_json(const json& j, MatchSettings& s) {
    j.at("sport").get_to(s.sport);
    j.at("homeName").get_to(s.homeName);
    j.at("awayName").get_to(s.awayName);
    j.at("twoDigitMinutes").get_to(s.twoDigitMinutes);
    j.at("tenthsInLastMinute").get_to(s.tenthsInLastMinute);
    j.at("stoppage").get_to(s.stoppage);
    j.at("penaltyLabelFormat").get_to(s.penaltyLabelFormat);
    j.at("strength").get_to(s.strength);
    j.at("playTimeFormat").get_to(s.playTimeFormat);
    j.at("regulationLabels").get_to(s.regulationLabels);
    j.at("overtimeLabels").get_to(s.overtimeLabels);
}

void to_json(json& j, const Penalty& p) {
    j = {{"id", p.id},
         {"player", p.player},
         {"phases", p.phases},
         {"phase", p.phase},
         {"remaining", p.remaining}};
}
void from_json(const json& j, Penalty& p) {
    j.at("id").get_to(p.id);
    j.at("player").get_to(p.player);
    j.at("phases").get_to(p.phases);
    j.at("phase").get_to(p.phase);
    j.at("remaining").get_to(p.remaining);
}

void to_json(json& j, const MatchEvent& e) {
    j = {{"type", e.type},
         {"at", e.at},
         {"playTime", e.playTime},
         {"period", e.period},
         {"periodLabel", e.periodLabel},
         {"team", e.team ? json(*e.team) : json(nullptr)},
         {"homeScore", e.homeScore},
         {"awayScore", e.awayScore},
         {"detail", e.detail}};
}
void from_json(const json& j, MatchEvent& e) {
    j.at("type").get_to(e.type);
    j.at("at").get_to(e.at);
    j.at("playTime").get_to(e.playTime);
    j.at("period").get_to(e.period);
    j.at("periodLabel").get_to(e.periodLabel);
    const json& team = j.at("team");
    e.team = team.is_null() ? std::nullopt : std::optional<Team>(team.get<Team>());
    j.at("homeScore").get_to(e.homeScore);
    j.at("awayScore").get_to(e.awayScore);
    j.at("detail").get_to(e.detail);
}

namespace {

// Structural checks the types alone cannot express. Returns an empty string when valid.
// Every rule here is a precondition PenaltyBox::restoreFrom (called downstream by
// MatchEngine::fromSnapshot) relies on but does not itself check.
std::string validate(const MatchSnapshot& s) {
    const SportPreset& sp = s.settings.sport;
    if (sp.periods < 1) return "sport has no periods";
    if (s.period < 1) return "period out of range";
    if (sp.overtimePeriods != kUnlimited && s.period > sp.periods + sp.overtimePeriods)
        return "period out of range";
    for (const auto& box : s.penalties) {
        if (box.size() > PenaltyBox::kMaxPenalties) return "too many penalties";
        std::set<PenaltyId> ids;
        for (const Penalty& p : box) {
            if (!ids.insert(p.id).second) return "duplicate penalty id";
            if (p.phases.empty() || p.phase >= p.phases.size()) return "penalty phase out of range";
            for (const PenaltyPhase& phase : p.phases)
                if (phase.duration <= 0) return "penalty phase duration out of range";
            if (p.remaining <= 0 || p.remaining > p.phases[p.phase].duration)
                return "penalty time out of range";
            if (p.id >= s.nextPenaltyId) return "penalty id not below nextPenaltyId";
        }
    }
    return {};
}

} // namespace

std::string toJson(const MatchSnapshot& s) {
    const json j = {{"version", kMatchJsonVersion},
                    {"settings", s.settings},
                    {"period", s.period},
                    {"clockValue", s.clockValue},
                    {"periodEndValues", s.periodEndValues},
                    {"score", s.score},
                    {"shots", s.shots},
                    {"fouls2", s.fouls2},
                    {"fouls", s.fouls},
                    {"penalties", s.penalties},
                    {"nextPenaltyId", s.nextPenaltyId},
                    {"stoppageAnnounced", s.stoppageAnnounced},
                    {"startedPeriods", s.startedPeriods},
                    {"events", s.events}};
    return j.dump(2);
}

std::optional<MatchSnapshot> snapshotFromJson(std::string_view text, std::string* error) {
    const auto fail = [&](std::string message) -> std::optional<MatchSnapshot> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    try {
        const json j = json::parse(text.begin(), text.end());
        if (!j.is_object()) return fail("not a JSON object");
        const int version = j.at("version").get<int>();
        if (version != kMatchJsonVersion)
            return fail("unsupported version " + std::to_string(version));
        MatchSnapshot s;
        j.at("settings").get_to(s.settings);
        j.at("period").get_to(s.period);
        j.at("clockValue").get_to(s.clockValue);
        j.at("periodEndValues").get_to(s.periodEndValues);
        j.at("score").get_to(s.score);
        j.at("shots").get_to(s.shots);
        j.at("fouls2").get_to(s.fouls2);
        j.at("fouls").get_to(s.fouls);
        j.at("penalties").get_to(s.penalties);
        j.at("nextPenaltyId").get_to(s.nextPenaltyId);
        j.at("stoppageAnnounced").get_to(s.stoppageAnnounced);
        j.at("startedPeriods").get_to(s.startedPeriods);
        j.at("events").get_to(s.events);
        const std::string problem = validate(s);
        if (!problem.empty()) return fail(problem);
        return s;
    } catch (const json::exception& e) {
        return fail(e.what());
    }
}

} // namespace sb
