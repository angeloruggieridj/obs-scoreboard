// SPDX-License-Identifier: GPL-2.0-or-later
#include "MatchJson.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

namespace sb {

using nlohmann::json;

namespace {

// -- Strict enum codec -------------------------------------------------------------------------
// NLOHMANN_JSON_SERIALIZE_ENUM maps an unrecognized or non-string value to the *first* mapped
// enumerator, silently: e.g. {"stoppage": "bogus"} would quietly become StopAtDuration. The two
// helpers below give the same string spellings on output but make an unknown or wrongly typed
// value a hard parse error on input, never a silent fallback.
template <typename Enum> struct EnumName {
    Enum value;
    const char* name;
};

template <typename Enum, std::size_t N>
json enumToJsonValue(Enum value, const EnumName<Enum> (&names)[N]) {
    for (const auto& entry : names)
        if (entry.value == value) return json(entry.name);
    return json(names[0].name); // unreachable for a value this codec ever produced
}

template <typename Enum, std::size_t N>
Enum enumFromJsonValue(const json& j, const EnumName<Enum> (&names)[N], const char* typeName) {
    if (!j.is_string())
        throw json::type_error::create(302, std::string(typeName) + " must be a string", &j);
    const std::string text = j.get<std::string>();
    for (const auto& entry : names)
        if (text == entry.name) return entry.value;
    throw json::type_error::create(
        302, std::string("unknown ") + typeName + " value \"" + text + "\"", &j);
}

constexpr EnumName<Team> kTeamNames[] = {{Team::Home, "home"}, {Team::Away, "away"}};
constexpr EnumName<Direction> kDirectionNames[] = {{Direction::Up, "up"},
                                                   {Direction::Down, "down"}};
constexpr EnumName<StoppageMode> kStoppageModeNames[] = {
    {StoppageMode::StopAtDuration, "stop"},
    {StoppageMode::SecondaryCounter, "secondary"},
    {StoppageMode::RunPastDuration, "runPast"}};
constexpr EnumName<FoulReset> kFoulResetNames[] = {
    {FoulReset::Never, "never"}, {FoulReset::EachRegulationPeriod, "eachRegulationPeriod"}};
constexpr EnumName<StrengthSource> kStrengthSourceNames[] = {
    {StrengthSource::None, "none"},
    {StrengthSource::Penalties, "penalties"},
    {StrengthSource::SecondFouls, "secondFouls"}};
constexpr EnumName<TimeFormat> kTimeFormatNames[] = {{TimeFormat::MinutesSeconds, "mmss"},
                                                     {TimeFormat::HoursMinutesSeconds, "hmmss"}};
constexpr EnumName<EventType> kEventTypeNames[] = {{EventType::PeriodStart, "periodStart"},
                                                   {EventType::PeriodEnd, "periodEnd"},
                                                   {EventType::Score, "score"},
                                                   {EventType::Penalty, "penalty"},
                                                   {EventType::MatchEnd, "matchEnd"}};

} // namespace

void to_json(json& j, Team t) {
    j = enumToJsonValue(t, kTeamNames);
}
void from_json(const json& j, Team& t) {
    t = enumFromJsonValue(j, kTeamNames, "team");
}
void to_json(json& j, Direction d) {
    j = enumToJsonValue(d, kDirectionNames);
}
void from_json(const json& j, Direction& d) {
    d = enumFromJsonValue(j, kDirectionNames, "direction");
}
void to_json(json& j, StoppageMode m) {
    j = enumToJsonValue(m, kStoppageModeNames);
}
void from_json(const json& j, StoppageMode& m) {
    m = enumFromJsonValue(j, kStoppageModeNames, "stoppage");
}
void to_json(json& j, FoulReset r) {
    j = enumToJsonValue(r, kFoulResetNames);
}
void from_json(const json& j, FoulReset& r) {
    r = enumFromJsonValue(j, kFoulResetNames, "foulReset");
}
void to_json(json& j, StrengthSource s) {
    j = enumToJsonValue(s, kStrengthSourceNames);
}
void from_json(const json& j, StrengthSource& s) {
    s = enumFromJsonValue(j, kStrengthSourceNames, "strengthSource");
}
void to_json(json& j, TimeFormat f) {
    j = enumToJsonValue(f, kTimeFormatNames);
}
void from_json(const json& j, TimeFormat& f) {
    f = enumFromJsonValue(j, kTimeFormatNames, "playTimeFormat");
}
void to_json(json& j, EventType t) {
    j = enumToJsonValue(t, kEventTypeNames);
}
void from_json(const json& j, EventType& t) {
    t = enumFromJsonValue(j, kEventTypeNames, "type");
}

namespace {

// -- Strict integer parsing ---------------------------------------------------------------------
// nlohmann's own get<T>() for an arithmetic T (get_arithmetic_value, json.hpp) converts a
// number_float JSON value through an unchecked static_cast<T>(double): out-of-range values (e.g.
// "clockValue": 1e300 into a 64-bit integer) are undefined behavior, and in-range-but-fractional
// values (e.g. 1.5) are silently truncated. It also never range-checks integer-to-integer
// narrowing (e.g. a huge unsigned value into `int`, or a negative value into `std::size_t`).
// strictInt<T> rejects both: only an actual JSON integer (never a float) is accepted, and it must
// fit in T's range.
template <typename T> T strictInt(const json& j, std::string_view fieldName) {
    if (!j.is_number_integer())
        throw json::type_error::create(302, std::string(fieldName) + " must be an integer", &j);
    if (j.is_number_unsigned()) {
        const auto v = j.get<std::uint64_t>();
        if (v > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
            throw json::type_error::create(302, std::string(fieldName) + " out of range", &j);
        return static_cast<T>(v);
    }
    const auto v = j.get<std::int64_t>();
    if constexpr (std::is_signed_v<T>) {
        if (v < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
            v > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
            throw json::type_error::create(302, std::string(fieldName) + " out of range", &j);
    } else {
        if (v < 0 || static_cast<std::uint64_t>(v) >
                         static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
            throw json::type_error::create(302, std::string(fieldName) + " out of range", &j);
    }
    return static_cast<T>(v);
}

template <typename T> std::vector<T> strictIntArray(const json& j, std::string_view fieldName) {
    if (!j.is_array())
        throw json::type_error::create(302, std::string(fieldName) + " must be an array", &j);
    std::vector<T> out;
    out.reserve(j.size());
    for (const auto& element : j) out.push_back(strictInt<T>(element, fieldName));
    return out;
}

std::set<int> strictIntSet(const json& j, std::string_view fieldName) {
    if (!j.is_array())
        throw json::type_error::create(302, std::string(fieldName) + " must be an array", &j);
    std::set<int> out;
    for (const auto& element : j) out.insert(strictInt<int>(element, fieldName));
    return out;
}

Counters::Pair strictIntPair(const json& j, std::string_view fieldName) {
    if (!j.is_array() || j.size() != 2)
        throw json::type_error::create(302, std::string(fieldName) + " must be a 2-element array",
                                       &j);
    return {strictInt<int>(j.at(0), fieldName), strictInt<int>(j.at(1), fieldName)};
}

// periodEndValues/fouls are std::map<int, V>: nlohmann has no string key for `int`, so it falls
// back to an array of [key, value] pairs. Both the key and the value go through strictInt/
// readValue rather than the library's own (unsafe for floats) pair conversion.
template <typename V, typename ReadValue>
std::map<int, V> strictIntKeyedMap(const json& j, std::string_view fieldName, ReadValue readValue) {
    if (!j.is_array())
        throw json::type_error::create(302, std::string(fieldName) + " must be an array", &j);
    std::map<int, V> out;
    for (const auto& entry : j) {
        if (!entry.is_array() || entry.size() != 2)
            throw json::type_error::create(
                302, std::string(fieldName) + " entry must be a [key, value] pair", &entry);
        out.emplace(strictInt<int>(entry.at(0), fieldName), readValue(entry.at(1), fieldName));
    }
    return out;
}

} // namespace

void to_json(json& j, const PenaltyPhase& p) {
    j = {{"duration", p.duration}, {"reducesStrength", p.reducesStrength}};
}
void from_json(const json& j, PenaltyPhase& p) {
    p.duration = strictInt<Tenths>(j.at("duration"), "penaltyPhase.duration");
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
    s.periods = strictInt<int>(j.at("periods"), "sport.periods");
    s.periodDuration = strictInt<Tenths>(j.at("periodDuration"), "sport.periodDuration");
    j.at("direction").get_to(s.direction);
    s.overtimePeriods = strictInt<int>(j.at("overtimePeriods"), "sport.overtimePeriods");
    s.overtimeDuration = strictInt<Tenths>(j.at("overtimeDuration"), "sport.overtimeDuration");
    j.at("continuousDisplay").get_to(s.continuousDisplay);
    j.at("shots").get_to(s.shots);
    j.at("fouls").get_to(s.fouls);
    j.at("fouls2").get_to(s.fouls2);
    j.at("foulReset").get_to(s.foulReset);
    j.at("foulsLabel").get_to(s.foulsLabel);
    j.at("fouls2Label").get_to(s.fouls2Label);
    j.at("scoreLabel").get_to(s.scoreLabel);
    s.scoreValues = strictIntArray<int>(j.at("scoreValues"), "sport.scoreValues");
    j.at("penaltyOptions").get_to(s.penaltyOptions);
    s.playersPerSide = strictInt<int>(j.at("playersPerSide"), "sport.playersPerSide");
    s.minPlayers = strictInt<int>(j.at("minPlayers"), "sport.minPlayers");
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
    p.id = strictInt<PenaltyId>(j.at("id"), "penalty.id");
    j.at("player").get_to(p.player);
    j.at("phases").get_to(p.phases);
    p.phase = strictInt<std::size_t>(j.at("phase"), "penalty.phase");
    p.remaining = strictInt<Tenths>(j.at("remaining"), "penalty.remaining");
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
    e.at = strictInt<Micros>(j.at("at"), "event.at");
    e.playTime = strictInt<Tenths>(j.at("playTime"), "event.playTime");
    e.period = strictInt<int>(j.at("period"), "event.period");
    j.at("periodLabel").get_to(e.periodLabel);
    const json& team = j.at("team");
    e.team = team.is_null() ? std::nullopt : std::optional<Team>(team.get<Team>());
    e.homeScore = strictInt<int>(j.at("homeScore"), "event.homeScore");
    e.awayScore = strictInt<int>(j.at("awayScore"), "event.awayScore");
    j.at("detail").get_to(e.detail);
}

namespace {

// An unlimited number of overtime periods (kUnlimited) does not mean an unbounded snapshot: cap
// it the same way the UI would ever offer, so a corrupted/huge period value can never make
// MatchEngine::playTime()/Periods::displayOffset() loop over an absurd period count.
constexpr int kUnlimitedOvertimeCap = 99;
// Upper bounds the UI would ever offer for a sport's period count and finite overtime count: a
// huge finite value (not just kUnlimited) must also be rejected, for the same reason.
constexpr int kMaxSportPeriods = 20;
constexpr int kMaxFiniteOvertimePeriods = 99;
// Every stored time-of-day-scale field (clock/period durations and values, penalty phase
// durations and remaining time, event play time) is a duration within a single match: 24 hours in
// tenths of a second is an extremely generous ceiling that still keeps
// MatchEngine::playTime()/Periods::displayOffset()/Clock::computeAt()/PenaltyBox::restore() far
// away from any risk of overflowing on a corrupted or attacker-supplied value.
constexpr Tenths kMaxSnapshotTime = 24 * 60 * 60 * kTenthsPerSecond; // 864000

bool inTimeRange(Tenths t) {
    return t >= 0 && t <= kMaxSnapshotTime;
}

// Structural checks the types alone cannot express. Returns an empty string when valid.
// Every rule here is a precondition PenaltyBox::restoreFrom (called downstream by
// MatchEngine::fromSnapshot) relies on but does not itself check.
std::string validate(const MatchSnapshot& s) {
    const SportPreset& sp = s.settings.sport;
    if (sp.periods < 1 || sp.periods > kMaxSportPeriods) return "sport periods out of range";
    if (sp.overtimePeriods != kUnlimited &&
        (sp.overtimePeriods < 0 || sp.overtimePeriods > kMaxFiniteOvertimePeriods))
        return "sport overtime periods out of range";
    if (!inTimeRange(sp.periodDuration)) return "sport period duration out of range";
    if (!inTimeRange(sp.overtimeDuration)) return "sport overtime duration out of range";
    if (s.period < 1) return "period out of range";
    const long long effectiveOvertime =
        sp.overtimePeriods == kUnlimited ? kUnlimitedOvertimeCap : sp.overtimePeriods;
    if (static_cast<long long>(s.period) > static_cast<long long>(sp.periods) + effectiveOvertime)
        return "period out of range";
    if (!inTimeRange(s.clockValue)) return "clock value out of range";
    for (const auto& entry : s.periodEndValues)
        if (!inTimeRange(entry.second)) return "period end value out of range";
    // Duplicate ids are checked across both teams' boxes, not per box: PenaltyEdit/PenaltyCancel
    // search Home first, so an id shared with Away would silently hit the wrong penalty.
    std::set<PenaltyId> ids;
    for (const auto& box : s.penalties) {
        if (box.size() > PenaltyBox::kMaxPenalties) return "too many penalties";
        for (const Penalty& p : box) {
            if (!ids.insert(p.id).second) return "duplicate penalty id";
            if (p.phases.empty() || p.phase >= p.phases.size()) return "penalty phase out of range";
            for (const PenaltyPhase& phase : p.phases)
                if (phase.duration <= 0 || !inTimeRange(phase.duration))
                    return "penalty phase duration out of range";
            if (p.remaining <= 0 || p.remaining > p.phases[p.phase].duration)
                return "penalty time out of range";
            if (p.id >= s.nextPenaltyId) return "penalty id not below nextPenaltyId";
        }
    }
    for (const MatchEvent& e : s.events) {
        if (e.at < 0) return "event time out of range";
        if (!inTimeRange(e.playTime)) return "event play time out of range";
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
        const int version = strictInt<int>(j.at("version"), "version");
        if (version != kMatchJsonVersion)
            return fail("unsupported version " + std::to_string(version));
        MatchSnapshot s;
        j.at("settings").get_to(s.settings);
        s.period = strictInt<int>(j.at("period"), "period");
        s.clockValue = strictInt<Tenths>(j.at("clockValue"), "clockValue");
        s.periodEndValues = strictIntKeyedMap<Tenths>(
            j.at("periodEndValues"), "periodEndValues",
            [](const json& v, std::string_view f) { return strictInt<Tenths>(v, f); });
        s.score = strictIntPair(j.at("score"), "score");
        s.shots = strictIntPair(j.at("shots"), "shots");
        s.fouls2 = strictIntPair(j.at("fouls2"), "fouls2");
        s.fouls = strictIntKeyedMap<Counters::Pair>(
            j.at("fouls"), "fouls",
            [](const json& v, std::string_view f) { return strictIntPair(v, f); });
        j.at("penalties").get_to(s.penalties);
        s.nextPenaltyId = strictInt<PenaltyId>(j.at("nextPenaltyId"), "nextPenaltyId");
        s.stoppageAnnounced = strictInt<int>(j.at("stoppageAnnounced"), "stoppageAnnounced");
        s.startedPeriods = strictIntSet(j.at("startedPeriods"), "startedPeriods");
        j.at("events").get_to(s.events);
        const std::string problem = validate(s);
        if (!problem.empty()) return fail(problem);
        return s;
    } catch (const json::exception& e) {
        return fail(e.what());
    }
}

} // namespace sb
