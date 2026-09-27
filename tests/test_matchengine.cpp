// SPDX-License-Identifier: GPL-2.0-or-later
#include "doctest/doctest.h"
#include "MatchEngine.hpp"

using namespace sb;

namespace {
constexpr Micros sec(std::int64_t s) {
    return s * 1000000;
}
MatchEngine engineFor(const char* id) {
    return MatchEngine(MatchSettings::forSport(*findSport(id)));
}
std::string field(const MatchEngine& e, FieldId id) {
    return e.fields()[static_cast<std::size_t>(id)];
}
CommandResult run(MatchEngine& e, Command c, Micros now = 0) {
    return e.apply(c, now);
}
} // namespace

TEST_CASE("a new futsal match shows its starting values") {
    MatchEngine e = engineFor("futsal");
    CHECK(field(e, FieldId::Clock) == "20:00");
    CHECK(field(e, FieldId::Period) == "1");
    CHECK(field(e, FieldId::HomeName) == "HOME");
    CHECK(field(e, FieldId::AwayScore) == "0");
    CHECK(field(e, FieldId::HomeFouls) == "0");
    CHECK(field(e, FieldId::HomeShots) == "");
    CHECK(field(e, FieldId::Strength) == "");
    CHECK(field(e, FieldId::PlayTime) == "00:00");
}

TEST_CASE("the clock runs only between start and stop and ends the period at zero") {
    MatchEngine e = engineFor("futsal");
    CHECK(run(e, cmd::ClockStart{}).isOk());
    e.advance(sec(1));
    CHECK(field(e, FieldId::Clock) == "19:59");
    CHECK(run(e, cmd::ClockToggle{}, sec(2)).isOk());
    e.advance(sec(10));
    CHECK(field(e, FieldId::Clock) == "19:58");
    run(e, cmd::ClockStart{}, sec(10));
    e.advance(sec(10) + sec(1198));
    CHECK(field(e, FieldId::Clock) == "00:00");
    CHECK_FALSE(e.clock().running());
    REQUIRE_FALSE(e.events().events().empty());
    CHECK(e.events().events().front().type == EventType::PeriodStart);
    CHECK(e.events().events().back().type == EventType::PeriodEnd);
    const CommandResult r = run(e, cmd::ClockStart{}, sec(2000));
    CHECK(r.status == CommandResult::Status::Rejected);
    CHECK(r.reason == "clock.atLimit");
}

TEST_CASE("reset is refused while running") {
    MatchEngine e = engineFor("futsal");
    run(e, cmd::ClockStart{});
    CHECK(run(e, cmd::ClockReset{}, sec(1)).reason == "clock.running");
    run(e, cmd::ClockStop{}, sec(5));
    CHECK(run(e, cmd::ClockReset{}, sec(5)).isOk());
    CHECK(field(e, FieldId::Clock) == "20:00");
}

TEST_CASE("changing period: refused while running, confirmation mid-period, free at start or end") {
    MatchEngine e = engineFor("futsal");
    run(e, cmd::ClockStart{});
    CHECK(run(e, cmd::PeriodNext{}, sec(10)).reason == "period.clockRunning");
    run(e, cmd::ClockStop{}, sec(10));
    const CommandResult r = run(e, cmd::PeriodNext{}, sec(10));
    CHECK(r.status == CommandResult::Status::NeedsConfirmation);
    CHECK(r.reason == "period.midPeriod");
    CHECK(run(e, cmd::PeriodNext{true}, sec(10)).isOk());
    CHECK(field(e, FieldId::Period) == "2");
    CHECK(field(e, FieldId::Clock) == "20:00");
    CHECK(run(e, cmd::PeriodPrev{}, sec(10)).isOk()); // period 2 is at its start
    CHECK(field(e, FieldId::Clock) == "19:50");       // period 1 comes back where it was left
    CHECK(run(e, cmd::PeriodPrev{}, sec(10)).reason == "period.noPrev");
}

TEST_CASE("futsal fouls per half, overtime continues the 2nd, going back restores the 1st") {
    MatchEngine e = engineFor("futsal");
    run(e, cmd::AddStat{Team::Home, Stat::Fouls, 3});
    run(e, cmd::PeriodNext{});
    CHECK(field(e, FieldId::HomeFouls) == "0");
    run(e, cmd::AddStat{Team::Home, Stat::Fouls, 1});
    run(e, cmd::PeriodNext{});
    CHECK(field(e, FieldId::Period) == "OT");
    CHECK(field(e, FieldId::HomeFouls) == "1");
    run(e, cmd::PeriodPrev{});
    run(e, cmd::PeriodPrev{});
    CHECK(field(e, FieldId::HomeFouls) == "3");
    run(e, cmd::PeriodNext{});
    run(e, cmd::PeriodNext{});
    run(e, cmd::PeriodNext{});
    CHECK(field(e, FieldId::Period) == "OT2");
    CHECK(run(e, cmd::PeriodNext{}).reason == "period.noNext");
}

TEST_CASE("soccer: continuous time and the two stoppage modes") {
    MatchEngine secondary = engineFor("soccer");
    run(secondary, cmd::ClockSet{minutes(44) + seconds(50)});
    run(secondary, cmd::ClockStart{});
    secondary.advance(sec(40));
    CHECK(field(secondary, FieldId::Clock) == "45:00");
    CHECK(field(secondary, FieldId::Stoppage) == "+0:30");
    run(secondary, cmd::StoppageSet{3});
    CHECK(field(secondary, FieldId::StoppageAnnounced) == "+3");
    run(secondary, cmd::ClockStop{}, sec(40));
    CHECK(run(secondary, cmd::PeriodNext{}, sec(40)).isOk()); // past the duration: no confirmation
    CHECK(field(secondary, FieldId::Clock) == "45:00");
    CHECK(field(secondary, FieldId::Stoppage) == "");

    MatchSettings s = MatchSettings::forSport(*findSport("soccer"));
    s.stoppage = StoppageMode::RunPastDuration;
    MatchEngine past(s);
    run(past, cmd::ClockSet{minutes(44) + seconds(50)});
    run(past, cmd::ClockStart{});
    past.advance(sec(40));
    CHECK(field(past, FieldId::Clock) == "45:30");
    CHECK(field(past, FieldId::Stoppage) == "");
}

TEST_CASE("soccer red cards reduce the side; shots are disabled") {
    MatchEngine e = engineFor("soccer");
    run(e, cmd::AddStat{Team::Home, Stat::Fouls2, 1});
    CHECK(field(e, FieldId::Strength) == "10-11");
    CHECK(field(e, FieldId::HomeFouls2) == "1");
    CHECK(run(e, cmd::AddStat{Team::Home, Stat::Shots, 1}).reason == "stat.disabled");
}

TEST_CASE("hockey penalties drive strength and follow the clock and its arrows") {
    MatchEngine e = engineFor("ice_hockey");
    CHECK(run(e, cmd::PenaltyAdd{Team::Home, "12", {{minutes(2), true}}}).isOk());
    CHECK(field(e, FieldId::Strength) == "4-5");
    CHECK(field(e, FieldId::HomePenalty1Player) == "12");
    CHECK(field(e, FieldId::HomePenalty1Time) == "2:00");
    CHECK(field(e, FieldId::HomePenalty1Label) == "12 2:00");
    CHECK(field(e, FieldId::HomePenalty2Player) == "");
    run(e, cmd::ClockStart{});
    e.advance(sec(30));
    CHECK(field(e, FieldId::HomePenalty1Time) == "1:30");
    run(e, cmd::ClockAdjust{seconds(10)}, sec(30)); // the countdown goes back 10 s
    CHECK(field(e, FieldId::HomePenalty1Time) == "1:40");
    e.advance(sec(130));
    CHECK(field(e, FieldId::HomePenalty1Player) == "");
    CHECK(field(e, FieldId::Strength) == "");
}

TEST_CASE("penalties do not run while the clock is stopped") {
    MatchEngine e = engineFor("ice_hockey");
    run(e, cmd::PenaltyAdd{Team::Away, "3", {{minutes(2), true}}});
    e.advance(sec(60));
    CHECK(field(e, FieldId::AwayPenalty1Time) == "2:00");
}

TEST_CASE("compound penalty label shows the second phase; edit, cancel and cancel-by-slot") {
    MatchEngine e = engineFor("ice_hockey");
    run(e, cmd::PenaltyAdd{Team::Home, "7", {{minutes(2), true}, {minutes(2), true}}});
    run(e, cmd::PenaltyAdd{Team::Home, "9", {{minutes(5), true}}});
    CHECK(field(e, FieldId::HomePenalty1Label) == "7 2:00");
    const PenaltyId first = e.penalties(Team::Home).all()[0].id;
    const PenaltyId second = e.penalties(Team::Home).all()[1].id;
    CHECK(run(e, cmd::PenaltyEdit{first, seconds(45)}).isOk());
    CHECK(field(e, FieldId::HomePenalty1Time) == "0:45");
    CHECK(run(e, cmd::PenaltyCancel{first}).isOk());
    CHECK(field(e, FieldId::HomePenalty1Time) == "2:00"); // second phase
    CHECK(run(e, cmd::PenaltyCancelActive{Team::Home, 1}).isOk());
    CHECK(e.penalties(Team::Home).find(second) == nullptr);
    CHECK(run(e, cmd::PenaltyCancelActive{Team::Home, 5}).reason == "penalty.notFound");
    CHECK(run(e, cmd::PenaltyEdit{999, 10}).reason == "penalty.notFound");
    CHECK(run(e, cmd::PenaltyCancel{999}).reason == "penalty.notFound");
}

TEST_CASE("penalty label template with the second phase placeholder") {
    MatchSettings s = MatchSettings::forSport(*findSport("ice_hockey"));
    s.penaltyLabelFormat = "#{player} {time}{phase2}";
    MatchEngine e(s);
    run(e, cmd::PenaltyAdd{Team::Away, "22", {{minutes(2), true}, {minutes(10), false}}});
    CHECK(field(e, FieldId::AwayPenalty1Label) == "#22 2:00+10:00");
}

TEST_CASE("penalty commands are refused where they do not apply") {
    MatchEngine futsal = engineFor("futsal");
    CHECK(run(futsal, cmd::PenaltyAdd{Team::Home, "1", {{minutes(2), true}}}).reason ==
          "penalty.disabled");
    MatchEngine hockey = engineFor("ice_hockey");
    CHECK(run(hockey, cmd::PenaltyAdd{Team::Home, "1", {}}).reason == "penalty.invalid");
    for (int i = 0; i < 8; ++i)
        run(hockey, cmd::PenaltyAdd{Team::Home, std::to_string(i), {{minutes(2), true}}});
    CHECK(run(hockey, cmd::PenaltyAdd{Team::Home, "x", {{minutes(2), true}}}).reason ==
          "penalty.full");
}

TEST_CASE("score values, names, score events and end of match") {
    MatchEngine e = engineFor("rugby_union");
    run(e, cmd::SetTeamName{Team::Home, "Lions"});
    CHECK(run(e, cmd::AddStat{Team::Home, Stat::Score, 5}).isOk());
    run(e, cmd::AddStat{Team::Home, Stat::Score, -10});
    CHECK(field(e, FieldId::HomeScore) == "0");
    CHECK(field(e, FieldId::HomeName) == "Lions");
    const auto& events = e.events().events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == EventType::Score);
    CHECK(events[0].team == Team::Home);
    CHECK(events[0].homeScore == 5);
    run(e, cmd::ClockStart{});
    CHECK(run(e, cmd::EndMatch{}, sec(3)).isOk());
    CHECK_FALSE(e.clock().running());
    CHECK(e.events().events().back().type == EventType::MatchEnd);
}

TEST_CASE("play time adds up completed periods") {
    MatchEngine e = engineFor("futsal");
    run(e, cmd::ClockStart{});
    e.advance(sec(1200));
    CHECK(run(e, cmd::PeriodNext{}, sec(1200)).isOk());
    run(e, cmd::ClockStart{}, sec(1300));
    e.advance(sec(1600));
    CHECK(field(e, FieldId::PlayTime) == "25:00");
    CHECK(e.playTime() == minutes(25));
}

TEST_CASE("leaving a period mid-way logs its end once") {
    MatchEngine e = engineFor("soccer");
    run(e, cmd::ClockStart{});
    run(e, cmd::ClockStop{}, sec(60));
    run(e, cmd::PeriodNext{true}, sec(60));
    const auto& events = e.events().events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].type == EventType::PeriodStart);
    CHECK(events[1].type == EventType::PeriodEnd);
    CHECK(events[1].period == 1);
}

TEST_CASE("snapshot and fromSnapshot give back the same match, clock stopped") {
    MatchEngine e = engineFor("ice_hockey");
    run(e, cmd::AddStat{Team::Away, Stat::Score, 2});
    run(e, cmd::AddStat{Team::Home, Stat::Shots, 7});
    run(e, cmd::PenaltyAdd{Team::Home, "12", {{minutes(2), true}}});
    run(e, cmd::ClockStart{});
    e.advance(sec(90));
    run(e, cmd::StoppageSet{2});
    const MatchSnapshot snap = e.snapshot();
    MatchEngine back = MatchEngine::fromSnapshot(snap);
    CHECK(back.fields() == e.fields());
    CHECK_FALSE(back.clock().running());
    CHECK(back.events().events().size() == e.events().events().size());
    run(back, cmd::PenaltyAdd{Team::Home, "4", {{minutes(2), true}}});
    CHECK(back.penalties(Team::Home).all().back().id == snap.nextPenaltyId);
}
