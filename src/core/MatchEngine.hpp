// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Clock.hpp"
#include "Counters.hpp"
#include "EventLog.hpp"
#include "Fields.hpp"
#include "Periods.hpp"
#include "PenaltyBox.hpp"
#include "SportCatalog.hpp"
#include "Strength.hpp"
#include "Types.hpp"

namespace sb {

struct MatchSettings {
    SportPreset sport;
    std::string homeName = "HOME";
    std::string awayName = "AWAY";
    bool twoDigitMinutes = true;
    bool tenthsInLastMinute = false;
    StoppageMode stoppage = StoppageMode::StopAtDuration;
    std::string penaltyLabelFormat = "{player} {time}"; // placeholders {player} {time} {phase2}
    StrengthFormat strength;
    TimeFormat playTimeFormat = TimeFormat::MinutesSeconds;
    std::vector<std::string> regulationLabels;
    std::vector<std::string> overtimeLabels;

    static MatchSettings forSport(const SportPreset& sport) {
        MatchSettings s;
        s.sport = sport;
        s.stoppage = sport.stoppage;
        return s;
    }
};

namespace cmd {
struct ClockToggle {};
struct ClockStart {};
struct ClockStop {};
// Resets the period clock to its start value; ignored while running (see Clock::resetToStart).
// Does not touch penalties: it corrects the period clock itself, not game time already spent.
struct ClockReset {};
struct ClockAdjust {
    Tenths delta = 0;
};
// Moves the clock to an absolute value, like an arrow press of the matching size: running
// penalties are shifted by the same amount (see ClockAdjust), unlike ClockReset.
struct ClockSet {
    Tenths value = 0;
};
struct PeriodNext {
    bool confirmed = false;
};
struct PeriodPrev {
    bool confirmed = false;
};
struct AddStat {
    Team team = Team::Home;
    Stat stat = Stat::Score;
    int delta = 0;
};
struct SetTeamName {
    Team team = Team::Home;
    std::string name;
};
struct PenaltyAdd {
    Team team = Team::Home;
    std::string player;
    std::vector<PenaltyPhase> phases;
};
struct PenaltyEdit {
    PenaltyId id = 0;
    Tenths remaining = 0;
};
struct PenaltyCancel {
    PenaltyId id = 0;
};
struct PenaltyCancelActive {
    Team team = Team::Home;
    int slot = 0;
};
struct StoppageSet {
    int minutes = 0;
};
// Idempotent: rejected with "match.ended" once the match has already been ended.
struct EndMatch {};
} // namespace cmd

// Every input channel (dock, hotkeys, obs-websocket) produces these same commands.
using Command =
    std::variant<cmd::ClockToggle, cmd::ClockStart, cmd::ClockStop, cmd::ClockReset,
                 cmd::ClockAdjust, cmd::ClockSet, cmd::PeriodNext, cmd::PeriodPrev, cmd::AddStat,
                 cmd::SetTeamName, cmd::PenaltyAdd, cmd::PenaltyEdit, cmd::PenaltyCancel,
                 cmd::PenaltyCancelActive, cmd::StoppageSet, cmd::EndMatch>;

struct CommandResult {
    enum class Status { Ok, Rejected, NeedsConfirmation };
    Status status = Status::Ok;
    std::string reason; // stable key, localized by the plugin (e.g. "period.midPeriod")

    static CommandResult ok() { return {}; }
    static CommandResult rejected(std::string r) { return {Status::Rejected, std::move(r)}; }
    static CommandResult confirm(std::string r) {
        return {Status::NeedsConfirmation, std::move(r)};
    }
    bool isOk() const { return status == Status::Ok; }
};

// Everything needed to resume a match. The clock always resumes stopped.
struct MatchSnapshot {
    MatchSettings settings;
    int period = 1;
    Tenths clockValue = 0;
    std::map<int, Tenths> periodEndValues;
    Counters::Pair score{};
    Counters::Pair shots{};
    Counters::Pair fouls2{};
    std::map<int, Counters::Pair> fouls;
    std::array<std::vector<Penalty>, 2> penalties;
    PenaltyId nextPenaltyId = 1;
    int stoppageAnnounced = 0;
    std::set<int> startedPeriods;
    std::vector<MatchEvent> events;
};

// The single source of truth of a match: applies commands, advances time, formats every field.
class MatchEngine {
public:
    explicit MatchEngine(MatchSettings settings);

    CommandResult apply(const Command& command, Micros now);
    void advance(Micros now);
    FieldValues fields() const;
    // Instant at which the clock field's text next changes (whole seconds, or tenths in the last
    // minute of a countdown when enabled); nullopt when the clock will not change again.
    std::optional<Micros> nextClockChangeAt(Micros now) const;

    const MatchSettings& settings() const { return settings_; }
    const Clock& clock() const { return clock_; }
    const Periods& periods() const { return periods_; }
    const Counters& counters() const { return counters_; }
    const PenaltyBox& penalties(Team team) const { return boxes_[teamIndex(team)]; }
    const EventLog& events() const { return events_; }
    int stoppageAnnounced() const { return stoppageAnnounced_; }
    Tenths playTime() const;

    MatchSnapshot snapshot() const;
    static MatchEngine fromSnapshot(const MatchSnapshot& snapshot);

private:
    CommandResult handle(const cmd::ClockToggle&, Micros now);
    CommandResult handle(const cmd::ClockStart&, Micros now);
    CommandResult handle(const cmd::ClockStop&, Micros now);
    CommandResult handle(const cmd::ClockReset&, Micros now);
    CommandResult handle(const cmd::ClockAdjust&, Micros now);
    CommandResult handle(const cmd::ClockSet&, Micros now);
    CommandResult handle(const cmd::PeriodNext&, Micros now);
    CommandResult handle(const cmd::PeriodPrev&, Micros now);
    CommandResult handle(const cmd::AddStat&, Micros now);
    CommandResult handle(const cmd::SetTeamName&, Micros now);
    CommandResult handle(const cmd::PenaltyAdd&, Micros now);
    CommandResult handle(const cmd::PenaltyEdit&, Micros now);
    CommandResult handle(const cmd::PenaltyCancel&, Micros now);
    CommandResult handle(const cmd::PenaltyCancelActive&, Micros now);
    CommandResult handle(const cmd::StoppageSet&, Micros now);
    CommandResult handle(const cmd::EndMatch&, Micros now);

    CommandResult changePeriod(int target, bool confirmed, Micros now);
    void enterPeriod(int index);
    Tenths elapsedIn(int index, Tenths clockValue) const;
    void shiftPenalties(Tenths gameDelta);
    int strengthReduction(Team team) const;
    void logEvent(EventType type, Micros now, std::optional<Team> team, std::string detail);
    // Whether a PeriodEnd for that period is already in the event log: derived from the log
    // itself (not a separate snapshot field) so it survives a snapshot round trip for free and
    // stays correct even when other events were logged after the period actually ended.
    bool periodEnded(int period) const;
    bool matchEnded() const;

    MatchSettings settings_;
    Clock clock_;
    Periods periods_;
    Counters counters_;
    std::array<PenaltyBox, 2> boxes_;
    EventLog events_;
    PenaltyId nextPenaltyId_ = 1;
    int stoppageAnnounced_ = 0;
    std::map<int, Tenths> periodEndValues_;
    std::set<int> startedPeriods_;
};

} // namespace sb
