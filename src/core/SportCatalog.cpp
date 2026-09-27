// SPDX-License-Identifier: GPL-2.0-or-later
#include "SportCatalog.hpp"

#include <utility>

namespace sb {
namespace {

PenaltyOption single(std::string label, Tenths d, bool reduces = true) {
    return {std::move(label), {{d, reduces}}};
}

PenaltyOption compound(std::string label, Tenths d1, bool r1, Tenths d2, bool r2) {
    return {std::move(label), {{d1, r1}, {d2, r2}}};
}

std::vector<SportPreset> makeCatalog() {
    std::vector<SportPreset> c;

    { // IIHF Official Rule Book: 3 x 20:00, overtime 5:00 (tournament rules vary).
        SportPreset s;
        s.id = "ice_hockey";
        s.segment = "Period";
        s.periods = 3;
        s.periodDuration = minutes(20);
        s.direction = Direction::Down;
        s.overtimePeriods = 1;
        s.overtimeDuration = minutes(5);
        s.shots = true;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("2'", minutes(2)),
                            single("5'", minutes(5)),
                            single("10'", minutes(10), false),
                            compound("2+2", minutes(2), true, minutes(2), true),
                            compound("2+5", minutes(2), true, minutes(5), true),
                            compound("2+10", minutes(2), true, minutes(10), false)};
        s.playersPerSide = 5;
        s.minPlayers = 3; // skaters, goalkeeper excluded
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // FIBA Official Basketball Rules: 4 x 10:00, 5:00 overtimes until decided; team fouls per
      // quarter.
        SportPreset s;
        s.id = "basketball";
        s.segment = "Quarter";
        s.periods = 4;
        s.periodDuration = minutes(10);
        s.direction = Direction::Down;
        s.overtimePeriods = kUnlimited;
        s.overtimeDuration = minutes(5);
        s.fouls = true;
        s.foulReset = FoulReset::EachRegulationPeriod;
        s.scoreLabel = "Points";
        s.scoreValues = {1, 2, 3};
        c.push_back(s);
    }
    { // IFAB Laws of the Game: 2 x 45:00 count-up, extra time 2 x 15:00; red cards reduce the side.
        SportPreset s;
        s.id = "soccer";
        s.segment = "Half";
        s.periods = 2;
        s.periodDuration = minutes(45);
        s.direction = Direction::Up;
        s.overtimePeriods = 2;
        s.overtimeDuration = minutes(15);
        s.continuousDisplay = true;
        s.fouls = true;
        s.fouls2 = true;
        s.foulsLabel = "Yellow";
        s.fouls2Label = "Red";
        s.scoreLabel = "Goal";
        s.playersPerSide = 11;
        s.minPlayers = 7; // Law 3: fewer than 7 and the match is abandoned
        s.strengthSource = StrengthSource::SecondFouls;
        s.stoppage = StoppageMode::SecondaryCounter;
        c.push_back(s);
    }
    { // FIFA Futsal Laws: 2 x 20:00 stopped clock, extra time 2 x 5:00; accumulated fouls per half.
        SportPreset s;
        s.id = "futsal";
        s.segment = "Half";
        s.periods = 2;
        s.periodDuration = minutes(20);
        s.direction = Direction::Down;
        s.overtimePeriods = 2;
        s.overtimeDuration = minutes(5);
        s.fouls = true;
        s.foulReset = FoulReset::EachRegulationPeriod;
        s.scoreLabel = "Goal";
        c.push_back(s);
    }
    { // NFL/NCAA: 4 x 15:00, overtime 10:00 (NFL regular season).
        SportPreset s;
        s.id = "american_football";
        s.segment = "Quarter";
        s.periods = 4;
        s.periodDuration = minutes(15);
        s.direction = Direction::Down;
        s.overtimePeriods = 1;
        s.overtimeDuration = minutes(10);
        s.fouls = true;
        s.foulsLabel = "Flags";
        s.scoreLabel = "Points";
        s.scoreValues = {6, 1, 2, 3};
        c.push_back(s);
    }
    { // World Lacrosse (field): 4 x 15:00, 4:00 sudden-victory overtimes; 30"/1'/2'/3' penalties.
        SportPreset s;
        s.id = "lacrosse";
        s.segment = "Quarter";
        s.periods = 4;
        s.periodDuration = minutes(15);
        s.direction = Direction::Down;
        s.overtimePeriods = kUnlimited;
        s.overtimeDuration = minutes(4);
        s.shots = true;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("30\"", seconds(30)), single("1'", minutes(1)),
                            single("2'", minutes(2)), single("3'", minutes(3))};
        s.playersPerSide = 10;
        s.minPlayers = 7; // verify: minimum players on field
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // World Rugby Laws: 2 x 40:00, clock runs past time until the ball is dead; extra time 2 x
      // 10:00.
        SportPreset s;
        s.id = "rugby_union";
        s.segment = "Half";
        s.periods = 2;
        s.periodDuration = minutes(40);
        s.direction = Direction::Up;
        s.overtimePeriods = 2;
        s.overtimeDuration = minutes(10);
        s.continuousDisplay = true;
        s.scoreLabel = "Points";
        s.scoreValues = {5, 2, 3};
        s.penaltyOptions = {single("10'", minutes(10)), single("20'", minutes(20))};
        s.playersPerSide = 15;
        s.minPlayers = 12; // verify: minimum players on field
        s.strengthSource = StrengthSource::Penalties;
        s.stoppage = StoppageMode::RunPastDuration;
        c.push_back(s);
    }
    { // World Rugby Sevens variations: 2 x 7:00, 5:00 sudden-death extra periods, 2' sin bin.
        SportPreset s;
        s.id = "rugby_sevens";
        s.segment = "Half";
        s.periods = 2;
        s.periodDuration = minutes(7);
        s.direction = Direction::Up;
        s.overtimePeriods = kUnlimited;
        s.overtimeDuration = minutes(5);
        s.continuousDisplay = true;
        s.scoreLabel = "Points";
        s.scoreValues = {5, 2, 3};
        s.penaltyOptions = {single("2'", minutes(2))};
        s.playersPerSide = 7;
        s.minPlayers = 5; // verify: minimum players on field
        s.strengthSource = StrengthSource::Penalties;
        s.stoppage = StoppageMode::RunPastDuration;
        c.push_back(s);
    }
    { // FIH Rules of Hockey: 4 x 15:00, no overtime (shoot-out); green 2', yellow 5'/10'.
        SportPreset s;
        s.id = "field_hockey";
        s.segment = "Quarter";
        s.periods = 4;
        s.periodDuration = minutes(15);
        s.direction = Direction::Down;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("2'", minutes(2)), single("5'", minutes(5)),
                            single("10'", minutes(10))};
        s.playersPerSide = 11;
        s.minPlayers = 8; // verify: minimum players on field
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // World Aquatics Water Polo Rules: 4 x 8:00, penalty shoot-out instead of overtime; 20"
      // exclusion.
        SportPreset s;
        s.id = "water_polo";
        s.segment = "Quarter";
        s.periods = 4;
        s.periodDuration = minutes(8);
        s.direction = Direction::Down;
        s.shots = true;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("20\"", seconds(20))};
        s.playersPerSide = 7;
        s.minPlayers = 4; // verify: minimum players in the water
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // IHF Rules of the Game: 2 x 30:00, extra time 2 x 5:00 (twice); 2' suspension.
        SportPreset s;
        s.id = "handball";
        s.segment = "Half";
        s.periods = 2;
        s.periodDuration = minutes(30);
        s.direction = Direction::Up;
        s.overtimePeriods = 2;
        s.overtimeDuration = minutes(5);
        s.continuousDisplay = true;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("2'", minutes(2))};
        s.playersPerSide = 7;
        s.minPlayers = 5; // verify: minimum players on court
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // World Skate Rink Hockey Rules: 2 x 25:00 stopped clock, extra time 2 x 5:00; blue card
      // 2'/4'; team fouls.
        SportPreset s;
        s.id = "rink_hockey";
        s.segment = "Half";
        s.periods = 2;
        s.periodDuration = minutes(25);
        s.direction = Direction::Down;
        s.overtimePeriods = 2;
        s.overtimeDuration = minutes(5);
        s.fouls = true;
        s.foulReset = FoulReset::EachRegulationPeriod;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("2'", minutes(2)), single("4'", minutes(4))};
        s.playersPerSide = 5;
        s.minPlayers = 3; // verify: minimum players; team-foul reset per half
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // IFF Rules of the Game: 3 x 20:00, overtime 10:00; 2'/5'/10' (10' personal does not reduce).
        SportPreset s;
        s.id = "floorball";
        s.segment = "Period";
        s.periods = 3;
        s.periodDuration = minutes(20);
        s.direction = Direction::Down;
        s.overtimePeriods = 1;
        s.overtimeDuration = minutes(10);
        s.shots = true;
        s.scoreLabel = "Goal";
        s.penaltyOptions = {single("2'", minutes(2)), single("5'", minutes(5)),
                            single("10'", minutes(10), false),
                            compound("2+2", minutes(2), true, minutes(2), true),
                            compound("2+10", minutes(2), true, minutes(10), false)};
        s.playersPerSide = 6;
        s.minPlayers = 3; // spec table: "6 (3)"
        s.strengthSource = StrengthSource::Penalties;
        c.push_back(s);
    }
    { // Generic: one open-ended count-up segment, everything else off.
        SportPreset s;
        s.id = "generic";
        c.push_back(s);
    }
    return c;
}

} // namespace

const std::vector<SportPreset>& builtInSports() {
    static const std::vector<SportPreset> catalog = makeCatalog();
    return catalog;
}

const SportPreset* findSport(std::string_view id) {
    for (const SportPreset& s : builtInSports())
        if (s.id == id) return &s;
    return nullptr;
}

} // namespace sb
