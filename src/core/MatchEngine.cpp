// SPDX-License-Identifier: GPL-2.0-or-later
#include "MatchEngine.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "Format.hpp"

namespace sb {

MatchEngine::MatchEngine(MatchSettings settings) : settings_(std::move(settings)) {
    const SportPreset& sp = settings_.sport;
    PeriodConfig pc;
    pc.regulationPeriods = sp.periods;
    pc.regulationDuration = sp.periodDuration;
    pc.overtimePeriods = sp.overtimePeriods;
    pc.overtimeDuration = sp.overtimeDuration;
    pc.continuousDisplay = sp.continuousDisplay;
    pc.regulationLabels = settings_.regulationLabels;
    pc.overtimeLabels = settings_.overtimeLabels;
    periods_.configure(pc);
    counters_.setFoulReset(sp.foulReset);
    enterPeriod(1);
}

CommandResult MatchEngine::apply(const Command& command, Micros now) {
    // Bring the clock (and everything it drives: penalties, the period-end check) up to `now`
    // before a handler acts, so a handler that does not itself call advance() (PenaltyEdit,
    // PenaltyCancel, AddStat...) never sees stale state left over from the caller's last explicit
    // advance() call. Harmless when the clock is stopped (Clock::advance() is a no-op then) and
    // idempotent for handlers that call advance() again themselves: a second call for the same
    // `now` measures zero elapsed time.
    advance(now);
    return std::visit([&](const auto& c) { return handle(c, now); }, command);
}

void MatchEngine::advance(Micros now) {
    const Clock::Tick tick = clock_.advance(now);
    if (tick.elapsed > 0)
        for (PenaltyBox& box : boxes_) box.elapse(tick.elapsed);
    if (tick.reachedLimit && !periodEnded(periods_.index()))
        logEvent(EventType::PeriodEnd, now, std::nullopt, {});
}

void MatchEngine::enterPeriod(int index) {
    periods_.setIndex(index);
    counters_.setPeriod(periods_.index(), settings_.sport.periods);
    clock_.configure(settings_.sport.direction, periods_.duration(),
                     settings_.stoppage == StoppageMode::StopAtDuration);
    // A stoppage announcement only makes sense for the period it was made in.
    stoppageAnnounced_ = 0;
    const auto it = periodEndValues_.find(periods_.index());
    if (it != periodEndValues_.end()) clock_.restore(it->second);
}

bool MatchEngine::periodEnded(int period) const {
    const auto& ev = events_.events();
    return std::any_of(ev.begin(), ev.end(), [period](const MatchEvent& e) {
        return e.period == period && e.type == EventType::PeriodEnd;
    });
}

bool MatchEngine::matchEnded() const {
    const auto& ev = events_.events();
    return std::any_of(ev.begin(), ev.end(),
                       [](const MatchEvent& e) { return e.type == EventType::MatchEnd; });
}

Tenths MatchEngine::elapsedIn(int index, Tenths clockValue) const {
    if (settings_.sport.direction == Direction::Up) return clockValue;
    return std::max<Tenths>(0, periods_.durationOf(index) - clockValue);
}

Tenths MatchEngine::playTime() const {
    Tenths total = 0;
    for (int i = 1; i < periods_.index(); ++i) {
        const auto it = periodEndValues_.find(i);
        if (it != periodEndValues_.end()) total += elapsedIn(i, it->second);
    }
    return total + elapsedIn(periods_.index(), clock_.value());
}

void MatchEngine::shiftPenalties(Tenths gameDelta) {
    for (PenaltyBox& box : boxes_) {
        if (gameDelta > 0)
            box.elapse(gameDelta);
        else
            box.restore(-gameDelta);
    }
}

void MatchEngine::logEvent(EventType type, Micros now, std::optional<Team> team,
                           std::string detail) {
    MatchEvent e;
    e.type = type;
    e.at = now;
    e.playTime = playTime();
    e.period = periods_.index();
    e.periodLabel = periods_.label();
    e.team = team;
    e.homeScore = counters_.get(Team::Home, Stat::Score);
    e.awayScore = counters_.get(Team::Away, Stat::Score);
    e.detail = std::move(detail);
    events_.add(std::move(e));
}

CommandResult MatchEngine::handle(const cmd::ClockToggle&, Micros now) {
    return clock_.running() ? handle(cmd::ClockStop{}, now) : handle(cmd::ClockStart{}, now);
}

CommandResult MatchEngine::handle(const cmd::ClockStart&, Micros now) {
    if (clock_.running()) return CommandResult::ok();
    clock_.start(now);
    if (!clock_.running()) return CommandResult::rejected("clock.atLimit");
    if (startedPeriods_.insert(periods_.index()).second)
        logEvent(EventType::PeriodStart, now, std::nullopt, {});
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::ClockStop&, Micros now) {
    advance(now); // penalties must see the time that passed up to the stop
    clock_.stop(now);
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::ClockReset&, Micros) {
    if (clock_.running()) return CommandResult::rejected("clock.running");
    clock_.resetToStart();
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::ClockAdjust& c, Micros now) {
    advance(now);
    const bool wasRunning = clock_.running();
    const Tenths applied = clock_.adjust(c.delta, now);
    // Moving a countdown up gives game time back; moving a count-up clock up adds game time.
    shiftPenalties(settings_.sport.direction == Direction::Down ? -applied : applied);
    // An arrow can drive a running clock straight to its limit, same as advance() reaching it.
    if (wasRunning && !clock_.running() && !periodEnded(periods_.index()))
        logEvent(EventType::PeriodEnd, now, std::nullopt, {});
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::ClockSet& c, Micros now) {
    advance(now);
    return handle(cmd::ClockAdjust{clock_.deltaTo(c.value)}, now);
}

CommandResult MatchEngine::changePeriod(int target, bool confirmed, Micros now) {
    if (clock_.running()) return CommandResult::rejected("period.clockRunning");
    if (!confirmed && !clock_.atStart() && !clock_.atLimit())
        return CommandResult::confirm("period.midPeriod");
    const int current = periods_.index();
    // Search the whole log for this period's PeriodEnd, not just the last event: something else
    // (a goal, a penalty) may have been logged after the period actually ended.
    if (startedPeriods_.count(current) != 0 && !periodEnded(current))
        logEvent(EventType::PeriodEnd, now, std::nullopt, {});
    periodEndValues_[current] = clock_.value();
    enterPeriod(target);
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::PeriodNext& c, Micros now) {
    if (!clock_.running() && !periods_.hasNext()) return CommandResult::rejected("period.noNext");
    return changePeriod(periods_.index() + 1, c.confirmed, now);
}

CommandResult MatchEngine::handle(const cmd::PeriodPrev& c, Micros now) {
    if (!clock_.running() && !periods_.hasPrev()) return CommandResult::rejected("period.noPrev");
    return changePeriod(periods_.index() - 1, c.confirmed, now);
}

CommandResult MatchEngine::handle(const cmd::AddStat& c, Micros now) {
    const SportPreset& sp = settings_.sport;
    const bool enabled = c.stat == Stat::Score || (c.stat == Stat::Shots && sp.shots) ||
                         (c.stat == Stat::Fouls && sp.fouls) ||
                         (c.stat == Stat::Fouls2 && sp.fouls2);
    if (!enabled) return CommandResult::rejected("stat.disabled");
    // Clamp the delta so current + delta cannot overflow int: Counters::add computes that sum
    // itself and does not guard against callers (e.g. obs-websocket) sending extreme values.
    const int current = counters_.get(c.team, c.stat);
    const std::int64_t target = std::clamp<std::int64_t>(static_cast<std::int64_t>(current) +
                                                             static_cast<std::int64_t>(c.delta),
                                                         0, std::numeric_limits<int>::max());
    const int safeDelta = static_cast<int>(target - current);
    const int applied = counters_.add(c.team, c.stat, safeDelta);
    if (c.stat == Stat::Score && applied > 0) logEvent(EventType::Score, now, c.team, {});
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::SetTeamName& c, Micros) {
    (c.team == Team::Home ? settings_.homeName : settings_.awayName) = c.name;
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::PenaltyAdd& c, Micros now) {
    if (!settings_.sport.hasPenalties()) return CommandResult::rejected("penalty.disabled");
    PenaltyBox& box = boxes_[teamIndex(c.team)];
    if (box.full()) return CommandResult::rejected("penalty.full");
    if (!box.add(nextPenaltyId_, c.player, c.phases))
        return CommandResult::rejected("penalty.invalid");
    ++nextPenaltyId_;
    logEvent(EventType::Penalty, now, c.team, c.player);
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::PenaltyEdit& c, Micros) {
    for (PenaltyBox& box : boxes_)
        if (box.edit(c.id, c.remaining)) return CommandResult::ok();
    return CommandResult::rejected("penalty.notFound");
}

CommandResult MatchEngine::handle(const cmd::PenaltyCancel& c, Micros) {
    for (PenaltyBox& box : boxes_)
        if (box.cancel(c.id)) return CommandResult::ok();
    return CommandResult::rejected("penalty.notFound");
}

CommandResult MatchEngine::handle(const cmd::PenaltyCancelActive& c, Micros now) {
    const auto active = boxes_[teamIndex(c.team)].active();
    if (c.slot < 0 || static_cast<std::size_t>(c.slot) >= active.size())
        return CommandResult::rejected("penalty.notFound");
    return handle(cmd::PenaltyCancel{active[static_cast<std::size_t>(c.slot)]->id}, now);
}

CommandResult MatchEngine::handle(const cmd::StoppageSet& c, Micros) {
    stoppageAnnounced_ = std::max(0, c.minutes);
    return CommandResult::ok();
}

CommandResult MatchEngine::handle(const cmd::EndMatch&, Micros now) {
    if (matchEnded()) return CommandResult::rejected("match.ended");
    advance(now);
    clock_.stop(now);
    logEvent(EventType::MatchEnd, now, std::nullopt, {});
    return CommandResult::ok();
}

int MatchEngine::strengthReduction(Team team) const {
    switch (settings_.sport.strengthSource) {
    case StrengthSource::Penalties:
        return boxes_[teamIndex(team)].strengthReduction();
    case StrengthSource::SecondFouls:
        return counters_.get(team, Stat::Fouls2);
    case StrengthSource::None:
        break;
    }
    return 0;
}

FieldValues MatchEngine::fields() const {
    FieldValues f;
    const SportPreset& sp = settings_.sport;
    const Tenths v = clock_.value();
    const Tenths duration = clock_.duration();
    const bool secondary = settings_.stoppage == StoppageMode::SecondaryCounter &&
                           sp.direction == Direction::Up && duration > 0;
    const Tenths shown = secondary ? std::min(v, duration) : v;

    fieldAt(f, FieldId::Clock) =
        formatClock(periods_.displayOffset() + shown, sp.direction, settings_.twoDigitMinutes,
                    settings_.tenthsInLastMinute);
    fieldAt(f, FieldId::Stoppage) =
        secondary && v > duration ? formatStoppage(v - duration) : std::string();
    fieldAt(f, FieldId::StoppageAnnounced) =
        stoppageAnnounced_ > 0 ? "+" + std::to_string(stoppageAnnounced_) : std::string();
    fieldAt(f, FieldId::Period) = periods_.label();
    fieldAt(f, FieldId::HomeName) = settings_.homeName;
    fieldAt(f, FieldId::AwayName) = settings_.awayName;

    const auto count = [&](Team t, Stat s, bool enabled) {
        return enabled ? std::to_string(counters_.get(t, s)) : std::string();
    };
    fieldAt(f, FieldId::HomeScore) = count(Team::Home, Stat::Score, true);
    fieldAt(f, FieldId::AwayScore) = count(Team::Away, Stat::Score, true);
    fieldAt(f, FieldId::HomeShots) = count(Team::Home, Stat::Shots, sp.shots);
    fieldAt(f, FieldId::AwayShots) = count(Team::Away, Stat::Shots, sp.shots);
    fieldAt(f, FieldId::HomeFouls) = count(Team::Home, Stat::Fouls, sp.fouls);
    fieldAt(f, FieldId::AwayFouls) = count(Team::Away, Stat::Fouls, sp.fouls);
    fieldAt(f, FieldId::HomeFouls2) = count(Team::Home, Stat::Fouls2, sp.fouls2);
    fieldAt(f, FieldId::AwayFouls2) = count(Team::Away, Stat::Fouls2, sp.fouls2);

    for (Team team : {Team::Home, Team::Away}) {
        const auto base = static_cast<std::size_t>(
            team == Team::Home ? FieldId::HomePenalty1Player : FieldId::AwayPenalty1Player);
        const auto active = boxes_[teamIndex(team)].active();
        for (std::size_t slot = 0; slot < active.size(); ++slot) {
            const Penalty& p = *active[slot];
            const std::string time = formatPenaltyTime(p.remaining);
            const std::string phase2 = p.hasNextPhase()
                                           ? "+" + formatPenaltyTime(p.phases[p.phase + 1].duration)
                                           : std::string();
            f[base + slot * 3] = p.player;
            f[base + slot * 3 + 1] = time;
            f[base + slot * 3 + 2] =
                applyTemplate(settings_.penaltyLabelFormat,
                              {{"player", p.player}, {"time", time}, {"phase2", phase2}});
        }
    }

    if (sp.strengthSource != StrengthSource::None) {
        const int home =
            playersOnField(sp.playersPerSide, sp.minPlayers, strengthReduction(Team::Home));
        const int away =
            playersOnField(sp.playersPerSide, sp.minPlayers, strengthReduction(Team::Away));
        fieldAt(f, FieldId::Strength) = formatStrength(home, away, settings_.strength);
    }
    fieldAt(f, FieldId::PlayTime) = formatDuration(playTime(), settings_.playTimeFormat);
    return f;
}

MatchSnapshot MatchEngine::snapshot() const {
    MatchSnapshot s;
    s.settings = settings_;
    s.period = periods_.index();
    s.clockValue = clock_.value();
    s.periodEndValues = periodEndValues_;
    for (Team t : {Team::Home, Team::Away}) {
        const std::size_t i = teamIndex(t);
        s.score[i] = counters_.get(t, Stat::Score);
        s.shots[i] = counters_.get(t, Stat::Shots);
        s.fouls2[i] = counters_.get(t, Stat::Fouls2);
        s.penalties[i] = boxes_[i].all();
    }
    s.fouls = counters_.foulBuckets();
    s.nextPenaltyId = nextPenaltyId_;
    s.stoppageAnnounced = stoppageAnnounced_;
    s.startedPeriods = startedPeriods_;
    s.events = events_.events();
    return s;
}

MatchEngine MatchEngine::fromSnapshot(const MatchSnapshot& s) {
    MatchEngine e(s.settings);
    e.periodEndValues_ = s.periodEndValues;
    e.startedPeriods_ = s.startedPeriods;
    e.enterPeriod(s.period);
    e.clock_.restore(s.clockValue);
    e.counters_.restore(s.score, s.shots, s.fouls2, s.fouls);
    for (std::size_t i = 0; i < 2; ++i) e.boxes_[i].restoreFrom(s.penalties[i]);
    e.nextPenaltyId_ = s.nextPenaltyId;
    e.stoppageAnnounced_ = std::max(0, s.stoppageAnnounced);
    e.events_.restoreFrom(s.events);
    return e;
}

} // namespace sb
